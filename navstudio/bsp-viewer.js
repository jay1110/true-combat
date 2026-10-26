/*!
 * bsp-viewer.js
 * TCE2 Navstudio local fork of etbrowser/assets/js/bsp-viewer.js (2026-10-11).
 * etbrowser remains unchanged. All asset requests stay on this local origin.
 * WebGL viewer for Quake 3 / Wolfenstein: Enemy Territory BSP files (format v46/v47).
 *
 * Usage:
 *   const viewer = new BSPViewer(canvasElement);
 *   viewer.load('/assets/bsp/maps/etl_sp_delivery.bsp')
 *         .then(() => viewer.start())
 *         .catch(err => console.error(err));
 *   // later:
 *   viewer.destroy();
 */
(function (global) {
  'use strict';

  /* ─────────────────────────── BSP LUMP IDs ─────────────────────────── */
  var L_ENTITIES    = 0;
  var L_TEXTURES    = 1;
  var L_PLANES      = 2;
  var L_NODES       = 3;
  var L_LEAFS       = 4;
  var L_LEAFFACES   = 5;
  var L_LEAFBRUSHES = 6;
  var L_MODELS      = 7;
  var L_BRUSHES     = 8;
  var L_BRUSHSIDES  = 9;
  var L_VERTICES    = 10;
  var L_MESHVERTS   = 11;
  var L_FACES       = 13;
  var L_LIGHTMAPS   = 14;

  /* Q3/ET content flags for brush collision */
  var CONTENTS_SOLID      = 1;
  var CONTENTS_PLAYERCLIP = 0x10000;

  var FACE_POLYGON = 1;
  var FACE_PATCH   = 2;
  var FACE_MESH    = 3;

  var PATCH_SUBDIV = 5;   /* bezier tessellation level */
  var LM_SIZE      = 128; /* lightmap width / height in pixels */
  var LOCAL_BSP_ROOT     = '/assets/bsp/';
  var LOCAL_BSP_MAPS     = LOCAL_BSP_ROOT + 'maps/';
  var LOCAL_BSP_MANIFEST = LOCAL_BSP_ROOT + 'manifest.php';
  var LOCAL_BSP_SHADERS  = LOCAL_BSP_ROOT + 'shaders.php';
  var REMOTE_ETMAIN  = LOCAL_BSP_ROOT + 'etmain/';
  var IMAGE_EXTS     = ['.jpg', '.jpeg', '.png', '.tga', '.webp'];
  var HIDDEN_TOOL_SURFACEPARMS = [
    'nodraw', 'caulk', 'clip', 'playerclip', 'monsterclip', 'weaponclip',
    'botclip', 'hint', 'skip', 'origin', 'trigger', 'clusterportal', 'areaportal', 'donotenter'
  ];
  var HIDDEN_TOOL_TEXTURES = HIDDEN_TOOL_SURFACEPARMS.concat(['fullclip']);
  var HIDDEN_TOOL_SURFACEPARM_RE = new RegExp('^surfaceparm\\s+(?:' + HIDDEN_TOOL_SURFACEPARMS.join('|') + ')\\b');
  var HIDDEN_TOOL_TEXTURE_RE = new RegExp('^common\\/(?:' + HIDDEN_TOOL_TEXTURES.join('|') + ')\\b');

  function normalizeAssetPath(path) {
    return String(path || '').replace(/\\/g, '/').replace(/^\/*/, '').replace(/\0.*$/, '');
  }

  function uniquePush(list, seen, value) {
    if (value && !seen[value]) {
      seen[value] = true;
      list.push(value);
    }
  }

  function stripLineComment(line) {
    return String(line || '').replace(/\/\/.*$/, '').trim();
  }

  function parseShaderList(text) {
    return String(text || '')
      .replace(/\r/g, '')
      .split('\n')
      .map(stripLineComment)
      .filter(Boolean);
  }

  function parseShaderImageRefs(lines) {
    var refs = [];
    var seen = {};

    function pushToken(token) {
      var cleaned = normalizeAssetPath(String(token || '').replace(/^"(.*)"$/, '$1'));
      if (!cleaned || /^\$/.test(cleaned) || /^[0-9.]+$/.test(cleaned)) return;
      uniquePush(refs, seen, cleaned);
    }

    for (var i = 0; i < lines.length; i++) {
      var line = stripLineComment(lines[i]);
      if (!line) continue;

      var single = line.match(/^(?:qer_editorimage|q3map_lightimage|map|clampmap|implicitmap|implicitblend|implicitmask)\s+("?)([^"\s]+)\1/i);
      if (single) {
        /* implicitMap/implicitBlend/implicitMask with "-" means "use shader name
           as texture" — skip it here; the caller uses the shader name directly. */
        if (single[2] !== '-') {
          pushToken(single[2]);
        }
        continue;
      }

      if (/^animmap\s+/i.test(line)) {
        var parts = line.split(/\s+/);
        for (var p = 2; p < parts.length; p++) pushToken(parts[p]);
      }
    }

    return refs;
  }

  var IMAGE_EXT_RE = /\.(jpg|jpeg|png|tga|webp)$/i;

  /* Classify a `blendfunc` line into one of the layer roles the sky renderer
     understands. Returns 'add', 'blend', 'filter' or 'opaque'. */
  function classifySkyBlend(low) {
    var p   = low.split(/\s+/);
    var src = p[1] || '';
    var dst = p[2] || '';
    if (src === 'add' || (src === 'gl_one' && dst === 'gl_one')) return 'add';
    if (src === 'blend' || (src === 'gl_src_alpha' && dst === 'gl_one_minus_src_alpha')) return 'blend';
    if (src === 'filter' || (src === 'gl_dst_color' && dst === 'gl_zero') ||
        (src === 'gl_zero' && dst === 'gl_src_color')) return 'filter';
    return 'opaque';
  }

  /* Parse a single line inside a sky shader stage, mutating `st` in place.
     Captures the texture map, blend mode, tcMod operations and rgbGen so the
     stage can be replayed by the sky-stage renderer. */
  function parseSkyStageLine(line, st) {
    var low = line.toLowerCase();
    var m;
    if ((m = line.match(/^map\s+("?)([^\s"]+)\1/i))) {
      if (m[2] !== '-' && m[2].charAt(0) !== '$') {
        st.map = normalizeAssetPath(m[2]).replace(IMAGE_EXT_RE, '');
        st.clamp = false;
      }
      return;
    }
    if ((m = line.match(/^clampmap\s+("?)([^\s"]+)\1/i))) {
      if (m[2] !== '-' && m[2].charAt(0) !== '$') {
        st.map = normalizeAssetPath(m[2]).replace(IMAGE_EXT_RE, '');
        st.clamp = true;
      }
      return;
    }
    if ((m = line.match(/^animmap\s+([0-9.]+)\s+(.+)$/i))) {
      var frames = m[2].split(/\s+/).filter(Boolean).map(function (f) {
        return normalizeAssetPath(f).replace(IMAGE_EXT_RE, '');
      });
      if (frames.length) {
        st.anim = frames;
        st.animFreq = parseFloat(m[1]) || 0;
        st.map = frames[0];
        st.clamp = false;
      }
      return;
    }
    if (/^blendfunc\b/.test(low)) { st.blend = classifySkyBlend(low); return; }
    if ((m = low.match(/^tcmod\s+scroll\s+(\S+)\s+(\S+)/))) {
      st.tcScrollS = parseFloat(m[1]) || 0; st.tcScrollT = parseFloat(m[2]) || 0; return;
    }
    if ((m = low.match(/^tcmod\s+scale\s+(\S+)\s+(\S+)/))) {
      st.tcScale = [parseFloat(m[1]) || 0, parseFloat(m[2]) || 0]; return;
    }
    if ((m = low.match(/^tcmod\s+rotate\s+(\S+)/))) {
      st.tcRotate = (parseFloat(m[1]) || 0) * Math.PI / 180; return;
    }
    if ((m = low.match(/^tcmod\s+transform\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/))) {
      st.tcTransform = [parseFloat(m[1]) || 0, parseFloat(m[2]) || 0, parseFloat(m[3]) || 0,
                        parseFloat(m[4]) || 0, parseFloat(m[5]) || 0, parseFloat(m[6]) || 0];
      return;
    }
    if (/^rgbgen\s+identitylighting/.test(low)) { st.rgbIdentityLighting = true; return; }
    if ((m = low.match(/^rgbgen\s+const\s*\(\s*([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s*\)/))) {
      st.rgbConst = [parseFloat(m[1]) || 0, parseFloat(m[2]) || 0, parseFloat(m[3]) || 0];
      return;
    }
  }

  /* Extract the ordered list of render stages ({ … } blocks) from a sky
     shader body. `bodyLines` must include the inner stage braces. Each stage
     resolves to { map, clamp, anim, animFreq, blend, tcScrollS, tcScrollT,
     tcScale, tcRotate, tcTransform, rgbConst }. Stages without a texture map
     (e.g. a bare `fog off` block) are skipped. */
  function parseSkyStages(bodyLines) {
    var stages = [];
    var depth  = 0;
    var cur    = null;

    function freshStage() {
      return { map: '', clamp: false, anim: null, animFreq: 0, blend: 'opaque',
               tcScrollS: 0, tcScrollT: 0, tcScale: null, tcRotate: 0,
               tcTransform: null, rgbConst: null, rgbIdentityLighting: false };
    }
    function closeStage() {
      if (cur && (cur.map || cur.anim)) stages.push(cur);
      cur = null;
    }

    for (var i = 0; i < bodyLines.length; i++) {
      var raw = stripLineComment(bodyLines[i]);
      if (!raw) continue;
      var opens   = (raw.match(/\{/g) || []).length;
      var closes  = (raw.match(/\}/g) || []).length;
      var content = raw.replace(/[{}]/g, '').trim();

      if (depth === 0) {
        if (opens > 0) {
          cur = freshStage();
          depth += opens - closes;
          if (content) parseSkyStageLine(content, cur);
          if (depth <= 0) { closeStage(); depth = 0; }
        }
        /* depth-0 lines without an opening brace are outer directives
           (skyparms, q3map_sun, surfaceparm…) – ignored here. */
        continue;
      }

      depth += opens - closes;
      if (content) parseSkyStageLine(content, cur);
      if (depth <= 0) { closeStage(); depth = 0; }
    }
    closeStage();
    return stages;
  }

  function parseShaderMeta(lines) {
    var transparent = false;
    var noLightmap = false;
    var additive = false;
    var hidden = false;
    var isSky = false;
    var skyFarBox = '';   /* base path of farbox cubemap, e.g. "env/country/country" */
    /* tcMod scroll/rotate: take the first scroll/rotate stage we see. Multiple
       stages are uncommon in ET and stacking them properly would need a
       multi-pass renderer, which is well outside the scope of this viewer. */
    var tcScrollS = 0, tcScrollT = 0, tcRotate = 0;

    /* Preserve original case of skyparms tokens (env paths are case-sensitive
       on Linux file systems) by scanning the raw line in addition to the
       lowercased version used for matching keywords. */
    for (var i = 0; i < lines.length; i++) {
      var raw  = stripLineComment(lines[i]);
      var line = raw.toLowerCase();
      if (!line) continue;

      if (/^surfaceparm\s+nolightmap\b/.test(line)) {
        noLightmap = true;
      }
      if (HIDDEN_TOOL_SURFACEPARM_RE.test(line)) {
        hidden = true;
      }
      /* Sky surfaces – render them with a dedicated skybox cubemap. The
         original geometry is kept (for depth-write & culling), but at
         draw-time the sky shader replaces its appearance. */
      if (/^surfaceparm\s+sky\b/.test(line)) {
        isSky = true;
        noLightmap = true;
      }
      var spm = raw.match(/^\s*skyparms\s+("?)([^\s"]+)\1/i);
      if (spm) {
        isSky = true;
        noLightmap = true;
        if (spm[2] && spm[2] !== '-') {
          /* Strip any extension – we'll append `_bk.jpg` etc. ourselves. */
          skyFarBox = normalizeAssetPath(spm[2]).replace(/\.(jpg|jpeg|png|tga|webp)$/i, '');
        }
      }
      if (/^alphafunc\b/.test(line)) {
        transparent = true;
      }
      if (/^implicit(?:blend|mask)\b/.test(line)) {
        transparent = true;
      }
      var tcm = line.match(/^tcmod\s+(scroll|rotate)\s+(.+)$/);
      if (tcm) {
        var nums = tcm[2].split(/\s+/).map(parseFloat);
        if (tcm[1] === 'scroll' && nums.length >= 2 &&
            isFinite(nums[0]) && isFinite(nums[1])) {
          if (tcScrollS === 0 && tcScrollT === 0) {
            tcScrollS = nums[0]; tcScrollT = nums[1];
          }
        } else if (tcm[1] === 'rotate' && nums.length >= 1 && isFinite(nums[0])) {
          if (tcRotate === 0) {
            /* Q3 tcMod rotate is degrees per second; convert to radians. */
            tcRotate = nums[0] * Math.PI / 180;
          }
        }
      }
      if (/^blendfunc\b/.test(line)) {
        var blendParts = line.split(/\s+/);
        var src = blendParts[1] || '';
        var dst = blendParts[2] || '';

        if (src === 'add' || (src === 'gl_one' && dst === 'gl_one')) {
          additive = true;
          transparent = true;
          continue;
        }
        if (src === 'blend') {
          transparent = true;
          continue;
        }
        if (src === 'filter') {
          /* Q3/ET "filter" (dst_color, zero) is opaque modulation, not alpha transparency. */
          continue;
        }

        var usesAlpha = /alpha/.test(src) || /alpha/.test(dst) || src === 'gl_src_alpha_saturate';
        var knownOpaqueBlend =
          (src === 'gl_one' && dst === 'gl_zero') ||
          (src === 'gl_dst_color' && dst === 'gl_zero') ||
          (src === 'gl_zero' && dst === 'gl_src_color');

        if (usesAlpha || !knownOpaqueBlend) {
          transparent = true;
        }
      }
    }

    return {
      transparent: transparent,
      noLightmap: noLightmap,
      additive: additive,
      hidden: hidden,
      isSky: isSky,
      skyFarBox: skyFarBox,
      tcScrollS: tcScrollS,
      tcScrollT: tcScrollT,
      tcRotate: tcRotate
    };
  }

  function isHiddenToolTexture(name) {
    var key = normalizeAssetPath(name).replace(/\.(jpg|jpeg|png|tga|webp)$/i, '').toLowerCase();
    if (!key) return false;
    return HIDDEN_TOOL_TEXTURE_RE.test(key);
  }

  /* Heuristic sky detection: BSP texture names containing "skies/" are almost
     always sky shaders, even when the .shader definition can't be loaded.
     This prevents sky faces from being rendered as opaque geometry. */
  function isLikelySkyTexture(name) {
    var key = normalizeAssetPath(name).toLowerCase();
    return /(?:^|\/)(skies|sky)\//.test(key);
  }

  function normalizeShaderDefEntry(entry) {
    if (!entry) return null;
    if (Array.isArray(entry)) {
      return { refs: entry, transparent: false, noLightmap: false, additive: false, hidden: false,
               isSky: false, skyFarBox: '', skyStages: [],
               tcScrollS: 0, tcScrollT: 0, tcRotate: 0 };
    }
    return {
      refs: entry.refs || [],
      transparent: !!entry.transparent,
      noLightmap: !!entry.noLightmap,
      additive: !!entry.additive,
      hidden: !!entry.hidden,
      isSky: !!entry.isSky,
      skyFarBox: entry.skyFarBox || '',
      skyStages: entry.skyStages || [],
      tcScrollS: +entry.tcScrollS || 0,
      tcScrollT: +entry.tcScrollT || 0,
      tcRotate:  +entry.tcRotate  || 0
    };
  }

  function parseShaderDefinitions(text) {
    var defs = {};
    var lines = String(text || '').replace(/\r/g, '').split('\n');
    var pendingName = '';
    var currentName = '';
    var currentLines = [];
    var currentBody = [];
    var depth = 0;

    for (var i = 0; i < lines.length; i++) {
      var raw = stripLineComment(lines[i]);
      if (!raw) continue;

      if (depth === 0) {
        if (raw === '{' && pendingName) {
          currentName = normalizeAssetPath(pendingName);
          currentLines = [];
          currentBody = [];
          depth = 1;
          pendingName = '';
          continue;
        }
        if (/\{$/.test(raw)) {
          currentName = normalizeAssetPath(raw.replace(/\{$/, '').trim());
          currentLines = [];
          currentBody = [];
          depth = 1;
          pendingName = '';
          continue;
        }
        pendingName = raw;
        continue;
      }

      depth += (raw.match(/\{/g) || []).length;
      depth -= (raw.match(/\}/g) || []).length;
      /* currentLines drops bare braces (used by refs/meta keyword scans);
         currentBody keeps them so sky stage blocks can be delimited. */
      currentBody.push(raw);
      if (raw !== '{' && raw !== '}') currentLines.push(raw);

      if (depth <= 0) {
        var refs = parseShaderImageRefs(currentLines);
        var meta = parseShaderMeta(currentLines);
        var skyStages = meta.isSky ? parseSkyStages(currentBody) : [];
        if (currentName && (refs.length || meta.transparent || meta.noLightmap ||
                            meta.additive || meta.hidden || meta.isSky ||
                            meta.tcScrollS || meta.tcScrollT || meta.tcRotate)) {
          defs[currentName] = {
            refs: refs,
            transparent: meta.transparent,
            noLightmap: meta.noLightmap,
            additive: meta.additive,
            hidden: meta.hidden,
            isSky: meta.isSky,
            skyFarBox: meta.skyFarBox,
            skyStages: skyStages,
            tcScrollS: meta.tcScrollS,
            tcScrollT: meta.tcScrollT,
            tcRotate: meta.tcRotate
          };
        }
        currentName = '';
        currentLines = [];
        currentBody = [];
        depth = 0;
      }
    }

    return defs;
  }

  /* ─────────────────────────── TGA DECODER ───────────────────────────── */
  /*
   * Decodes a TGA ArrayBuffer into { data: Uint8Array(RGBA), width, height }.
   * Supports types 2/10 (true-colour) and 3/11 (grayscale), with or without
   * RLE compression, in 8/16/24/32 bpp.  Returns null for unsupported files.
   */
  function decodeTGA(buf) {
    var b  = new Uint8Array(buf);
    var dv = new DataView(buf);

    var idLen   = b[0];
    var cmType  = b[1];
    var imgType = b[2];
    var cmLen   = dv.getUint16(5,  true);
    var cmBPE   = b[7];
    var width   = dv.getUint16(12, true);
    var height  = dv.getUint16(14, true);
    var bpp     = b[16];
    var imgDesc = b[17];
    var alphaBits = imgDesc & 0x0F;

    var rle = (imgType === 10 || imgType === 11);
    if (imgType !== 2 && imgType !== 3 && imgType !== 10 && imgType !== 11) return null;
    if (!width || !height) return null;

    var Bpp  = Math.ceil(bpp / 8);
    var flip = !(imgDesc & 0x20);    /* 0 = bottom-left origin → need Y-flip */
    var off  = 18 + idLen + cmLen * Math.ceil(cmBPE / 8);

    var total = width * height;
    var rgba  = new Uint8Array(total * 4);
    var dst   = 0;

    function writePixel(o) {
      var d = dst++ * 4;
      if (Bpp >= 3) {
        rgba[d]   = b[o + 2];
        rgba[d+1] = b[o + 1];
        rgba[d+2] = b[o];
        rgba[d+3] = (Bpp >= 4) ? b[o + 3] : 255;
      } else if (Bpp === 2) {
        var v = b[o] | (b[o+1] << 8);
        rgba[d]   = (((v >> 10) & 31) * 255 / 31) | 0;
        rgba[d+1] = (((v >>  5) & 31) * 255 / 31) | 0;
        rgba[d+2] = ((v         & 31) * 255 / 31) | 0;
        rgba[d+3] = (alphaBits > 0) ? ((v & 0x8000) ? 255 : 0) : 255;
      } else {
        rgba[d] = rgba[d+1] = rgba[d+2] = b[o];
        rgba[d+3] = 255;
      }
    }

    if (rle) {
      while (dst < total) {
        var h = b[off++];
        var n = (h & 0x7F) + 1;
        if (h & 0x80) {
          var o0 = off; off += Bpp;
          for (var k = 0; k < n && dst < total; k++) writePixel(o0);
        } else {
          for (var k2 = 0; k2 < n && dst < total; k2++, off += Bpp) writePixel(off);
        }
      }
    } else {
      for (var p = 0; p < total; p++, off += Bpp) writePixel(off);
    }

    if (flip) {
      var row = new Uint8Array(width * 4);
      for (var y = 0; y < (height >> 1); y++) {
        var a = y * width * 4;
        var c = (height - 1 - y) * width * 4;
        row.set(rgba.subarray(a, a + width * 4));
        rgba.copyWithin(a, c, c + width * 4);
        rgba.set(row, c);
      }
    }

    return { data: rgba, width: width, height: height };
  }

  /* ─────────────────────────── GLSL SHADERS ──────────────────────────── */
  var VERT = [
    'attribute vec3 aPos;',
    'attribute vec2 aTex;',
    'attribute vec2 aLM;',
    'attribute vec4 aCol;',
    'uniform   mat4 uMVP;',
    'uniform   mat4 uView;',
    'varying   vec2 vTex;',
    'varying   vec2 vLM;',
    'varying   vec4 vCol;',
    'varying   float vViewDist;',
    'void main(){',
    '  gl_Position = uMVP * vec4(aPos, 1.0);',
    '  vTex = aTex;',
    '  vLM  = aLM;',
    '  vCol = aCol;',
    '  vec4 vp = uView * vec4(aPos, 1.0);',
    '  vViewDist = length(vp.xyz);',
    '}'
  ].join('\n');

  /* Fragment shader features:
   *   – Lightmap and BSP vertex colors are already overbright-shifted at
   *     upload time (matches ET's R_ColorShiftLightingBytes), so we no
   *     longer need ACES filmic tone-mapping here. A simple gamma curve
   *     (default uGamma = 1.2, matching ET's r_gamma) plus the per-user
   *     uBrightness multiplier is enough.
   *   – Distance-based exponential fog blends fragments with uFogColor.  Both
   *     fog density and color can be controlled from JS (setFog / setFogColor).
   *   – tcMod scroll/rotate: a per-batch uTcScroll (su, sv) and uTcRotate
   *     (radians/sec) animate the base texture coordinates over time via the
   *     uTime uniform. Both default to zero so non-animated surfaces are
   *     unchanged.
   */
  var FRAG = [
    'precision mediump float;',
    'varying vec2  vTex;',
    'varying vec2  vLM;',
    'varying vec4  vCol;',
    'varying float vViewDist;',
    'uniform sampler2D uBaseTex;',
    'uniform sampler2D uLMTex;',
    'uniform float     uHasBase;',
    'uniform float     uHasLM;',
    'uniform float     uBrightness;',
    'uniform float     uGamma;',
    'uniform float     uFogDensity;',
    'uniform vec3      uFogColor;',
    'uniform float     uTime;',
    'uniform vec2      uTcScroll;',
    'uniform float     uTcRotate;',
    'uniform float     uWireOverlay;', /* 1.0 => output pure white (for wireframe overlay) */
    'void main(){',
    '  vec2 tc = vTex + uTcScroll * uTime;',
    '  if (uTcRotate != 0.0) {',
    '    float ang = uTcRotate * uTime;',
    '    float cs = cos(ang), sn = sin(ang);',
    '    vec2 c2 = tc - 0.5;',
    '    tc = vec2(c2.x * cs - c2.y * sn, c2.x * sn + c2.y * cs) + 0.5;',
    '  }',
    '  vec4 base = mix(vec4(1.0), texture2D(uBaseTex, tc), uHasBase);',
    /* Lightmapped faces carry their lighting in the lightmap; ET composites
       these as texture*lightmap with rgbGen identity, so the per-vertex
       color must NOT also modulate them (that double-darkens the map).
       Non-lightmapped faces are vertex-lit, so keep vCol and use a neutral
       1.0 lightmap (no 0.5 darkening). */
    '  vec4 lm   = mix(vec4(1.0), texture2D(uLMTex, vLM), uHasLM);',
    '  vec3 vcol = mix(vCol.rgb, vec3(1.0), uHasLM);',
    '  vec3 lit  = base.rgb * vcol * lm.rgb * uBrightness;',
    '  /* Gamma curve – ET applies r_gamma via the HW gamma ramp; we do the',
    '     equivalent in shader since WebGL has no hardware gamma. */',
    '  float invG = 1.0 / max(uGamma, 0.01);',
    '  vec3 rgb = clamp(pow(max(lit, vec3(0.0)), vec3(invG)), 0.0, 1.0);',
    '  /* Exponential squared fog (Quake-style). uFogDensity == 0 disables it. */',
    '  float f = exp(-uFogDensity * uFogDensity * vViewDist * vViewDist);',
    '  f = clamp(f, 0.0, 1.0);',
    '  rgb = mix(uFogColor, rgb, f);',
    '  float a = clamp(base.a * vCol.a, 0.0, 1.0);',
    '  if (uWireOverlay > 0.5) { gl_FragColor = vec4(1.0); return; }',
    '  gl_FragColor = vec4(rgb, a);',
    '}'
  ].join('\n');

  /* ─────────────────────────── MATRIX MATH ───────────────────────────── */
  /* All matrices are Float32Array in column-major order (WebGL standard). */

  function dot3(a, b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  }

  function cross3(a, b) {
    return [
      a[1] * b[2] - a[2] * b[1],
      a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0]
    ];
  }

  function norm3(v) {
    var l = Math.sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    return l > 1e-9 ? [v[0]/l, v[1]/l, v[2]/l] : [0, 1, 0]; /* fall back to world-up */
  }

  function matPerspective(fovY, aspect, near, far) {
    var f  = 1.0 / Math.tan(fovY * 0.5);
    var nf = 1.0 / (near - far);
    return new Float32Array([
      f / aspect, 0,  0,                    0,
      0,          f,  0,                    0,
      0,          0,  (far + near) * nf,   -1,
      0,          0,  2 * far * near * nf,  0
    ]);
  }

  function matLookAt(eye, center, worldUp) {
    var f = norm3([center[0]-eye[0], center[1]-eye[1], center[2]-eye[2]]);
    var r = norm3(cross3(f, worldUp));
    var u = cross3(r, f);
    return new Float32Array([
       r[0],  u[0], -f[0], 0,
       r[1],  u[1], -f[1], 0,
       r[2],  u[2], -f[2], 0,
      -dot3(r, eye), -dot3(u, eye), dot3(f, eye), 1
    ]);
  }

  function matMul(a, b) {
    var c = new Float32Array(16);
    for (var col = 0; col < 4; col++) {
      for (var row = 0; row < 4; row++) {
        var s = 0;
        for (var k = 0; k < 4; k++) s += a[k*4+row] * b[col*4+k];
        c[col*4+row] = s;
      }
    }
    return c;
  }

  /* ─────────────────────────── BSP PARSER ────────────────────────────── */
  function parseBSP(buf, overbrightBits) {
    if (overbrightBits == null) overbrightBits = 1;
    var dv = new DataView(buf);
    var magic = String.fromCharCode(
      dv.getUint8(0), dv.getUint8(1), dv.getUint8(2), dv.getUint8(3)
    );
    var version = dv.getInt32(4, true);
    if (magic !== 'IBSP')
      throw new Error('Not a BSP file (magic: ' + magic + ')');
    if (version !== 46 && version !== 47)
      throw new Error('Unsupported BSP version ' + version + ' (expected 46 or 47)');

    var lumps = [];
    for (var i = 0; i < 17; i++) {
      var base = 8 + i * 8;
      lumps.push({
        offset: dv.getInt32(base,   true),
        length: dv.getInt32(base+4, true)
      });
    }

    /* ─── Entities lump (lump 0): plain-text key/value pairs ─── */
    var entities = [];
    var el = lumps[L_ENTITIES];
    if (el.length > 0) {
      var entBytes = new Uint8Array(buf, el.offset, el.length);
      var entStr = '';
      for (var ei = 0; ei < entBytes.length; ei++) {
        var c = entBytes[ei];
        if (c === 0) break;
        entStr += String.fromCharCode(c);
      }
      /* Parse Q3/ET entity string: sequence of { "key" "value" ... } blocks */
      var entRe = /\{([^}]*)\}/g;
      var entMatch;
      while ((entMatch = entRe.exec(entStr)) !== null) {
        var block = entMatch[1];
        var kvRe = /"([^"]+)"\s+"([^"]*)"/g;
        var kvMatch;
        var ent = {};
        while ((kvMatch = kvRe.exec(block)) !== null) {
          ent[kvMatch[1]] = kvMatch[2];
        }
        if (ent.classname) entities.push(ent);
      }
    }

    /* Textures/Shaders – 72 bytes each: name[64], flags[4], contents[4] */
    var tl = lumps[L_TEXTURES];
    var nt = Math.floor(tl.length / 72);
    var textures = new Array(nt);
    for (var ti = 0; ti < nt; ti++) {
      var tb = tl.offset + ti * 72;
      var chars = [];
      for (var tc = 0; tc < 64; tc++) {
        var ch = dv.getUint8(tb + tc);
        if (ch === 0) break;
        chars.push(String.fromCharCode(ch));
      }
      textures[ti] = {
        name: chars.join(''),
        flags: dv.getInt32(tb + 64, true),
        contents: dv.getInt32(tb + 68, true)
      };
    }

    /* Planes – 16 bytes each: normal[3](12), dist(4) */
    var pl = lumps[L_PLANES];
    var np = Math.floor(pl.length / 16);
    var planes = new Array(np);
    for (var pi = 0; pi < np; pi++) {
      var pb = pl.offset + pi * 16;
      planes[pi] = {
        normal: [dv.getFloat32(pb, true), dv.getFloat32(pb+4, true), dv.getFloat32(pb+8, true)],
        dist: dv.getFloat32(pb+12, true)
      };
    }

    /* Nodes – 36 bytes each (v46) : plane(4), children[2](8), mins[3](12), maxs[3](12) */
    var nl = lumps[L_NODES];
    var nn = Math.floor(nl.length / 36);
    var nodes = new Array(nn);
    for (var ni = 0; ni < nn; ni++) {
      var nb = nl.offset + ni * 36;
      nodes[ni] = {
        plane: dv.getInt32(nb, true),
        children: [dv.getInt32(nb+4, true), dv.getInt32(nb+8, true)]
      };
    }

    /* Leafs – 48 bytes each: cluster(4), area(4), mins[3](12), maxs[3](12),
       leafface(4), n_leaffaces(4), leafbrush(4), n_leafbrushes(4) */
    var lfl = lumps[L_LEAFS];
    var nleafs = Math.floor(lfl.length / 48);
    var leafs = new Array(nleafs);
    for (var lli = 0; lli < nleafs; lli++) {
      var lb = lfl.offset + lli * 48;
      leafs[lli] = {
        leafface: dv.getInt32(lb + 32, true),
        n_leaffaces: dv.getInt32(lb + 36, true),
        leafbrush: dv.getInt32(lb + 40, true),
        n_leafbrushes: dv.getInt32(lb + 44, true)
      };
    }

    /* Leaf Faces – 4 bytes each (int32 face index) */
    var lfl2 = lumps[L_LEAFFACES];
    var nlf = Math.floor(lfl2.length / 4);
    var leaffaces = new Int32Array(buf, lfl2.offset, nlf);

    /* Leaf Brushes – 4 bytes each (int32 brush index) */
    var lbl = lumps[L_LEAFBRUSHES];
    var nlb = Math.floor(lbl.length / 4);
    var leafbrushes = new Int32Array(buf, lbl.offset, nlb);

    /* Brushes – 12 bytes each: brushside(4), n_brushsides(4), texture(4) */
    var brl = lumps[L_BRUSHES];
    var nbr = Math.floor(brl.length / 12);
    var brushes = new Array(nbr);
    for (var bi = 0; bi < nbr; bi++) {
      var bb = brl.offset + bi * 12;
      brushes[bi] = {
        brushside: dv.getInt32(bb, true),
        n_brushsides: dv.getInt32(bb+4, true),
        texture: dv.getInt32(bb+8, true)
      };
    }

    /* Brush Sides – 8 bytes each: plane(4), texture(4) */
    var bsl = lumps[L_BRUSHSIDES];
    var nbs = Math.floor(bsl.length / 8);
    var brushsides = new Array(nbs);
    for (var bsi = 0; bsi < nbs; bsi++) {
      var bsb = bsl.offset + bsi * 8;
      brushsides[bsi] = {
        plane: dv.getInt32(bsb, true),
        texture: dv.getInt32(bsb+4, true)
      };
    }

    /* Vertices – 44 bytes each
       pos[3], tc[2], lm[2], normal[3], color[4].
       Apply ET overbright shift to RGB (mirrors R_ColorShiftLightingBytes on
       the BSP drawVerts in tr_bsp.c) so vertex-lit surfaces match the engine.
       Also enforce _minvertexlight from worldspawn so vertex-lit faces are
       never darker than the intended floor. */
    var vl = lumps[L_VERTICES];
    var nv = Math.floor(vl.length / 44);
    var verts = new Array(nv);
    var vertShift = overbrightBits | 0;
    /* Extract _minvertexlight from worldspawn for vertex color clamping */
    var vertMinLight = 0;
    for (var wvi = 0; wvi < entities.length; wvi++) {
      if (entities[wvi].classname === 'worldspawn') {
        var mvlVal = parseInt(entities[wvi]._minvertexlight || '0', 10) || 0;
        var mlVal  = parseInt(entities[wvi]._minlight || '0', 10) || 0;
        vertMinLight = Math.max(mvlVal, mlVal);
        break;
      }
    }
    var vTmp = new Uint8Array(4);
    for (var vi = 0; vi < nv; vi++) {
      var b = vl.offset + vi * 44;
      vTmp[0] = dv.getUint8(b+40);
      vTmp[1] = dv.getUint8(b+41);
      vTmp[2] = dv.getUint8(b+42);
      vTmp[3] = dv.getUint8(b+43);
      /* Enforce minimum vertex light floor */
      if (vertMinLight > 0) {
        if (vTmp[0] < vertMinLight) vTmp[0] = vertMinLight;
        if (vTmp[1] < vertMinLight) vTmp[1] = vertMinLight;
        if (vTmp[2] < vertMinLight) vTmp[2] = vertMinLight;
      }
      if (vertShift > 0) colorShiftLightingBytes(vTmp, 0, vertShift);
      verts[vi] = {
        pos: [dv.getFloat32(b,    true), dv.getFloat32(b+4,  true), dv.getFloat32(b+8,  true)],
        tc:  [dv.getFloat32(b+12, true), dv.getFloat32(b+16, true)],
        lm:  [dv.getFloat32(b+20, true), dv.getFloat32(b+24, true)],
        col: [
          vTmp[0] / 255.0,
          vTmp[1] / 255.0,
          vTmp[2] / 255.0,
          vTmp[3] / 255.0
        ]
      };
    }

    /* Mesh verts – 4 bytes each */
    var ml = lumps[L_MESHVERTS];
    var nm = Math.floor(ml.length / 4);
    var meshverts = new Int32Array(buf, ml.offset, nm);

    /* Faces – 104 bytes each
       texture(4), effect(4), type(4), vertex(4), n_verts(4),
       meshvert(4), n_meshverts(4), lm_index(4), lm_start[2](8),
       lm_size[2](8), lm_origin[3](12), lm_vecs[2][3](24),
       normal[3](12), size[2](8)  → total 104                           */
    var fl = lumps[L_FACES];
    var nf = Math.floor(fl.length / 104);
    var faces = new Array(nf);
    for (var fi = 0; fi < nf; fi++) {
      var fb = fl.offset + fi * 104;
      faces[fi] = {
        texture:    dv.getInt32(fb,     true),
        type:       dv.getInt32(fb+8,   true),
        vertex:     dv.getInt32(fb+12,  true),
        n_verts:    dv.getInt32(fb+16,  true),
        meshvert:   dv.getInt32(fb+20,  true),
        n_meshverts:dv.getInt32(fb+24,  true),
        lm_index:   dv.getInt32(fb+28,  true),
        size:       [dv.getInt32(fb+96, true), dv.getInt32(fb+100, true)]
      };
    }

    /* Lightmaps – LM_SIZE×LM_SIZE×3 bytes each */
    var ll   = lumps[L_LIGHTMAPS];
    var lmsz = LM_SIZE * LM_SIZE * 3;
    var nlm  = Math.floor(ll.length / lmsz);
    var lightmaps = new Array(nlm);
    for (var li = 0; li < nlm; li++) {
      lightmaps[li] = new Uint8Array(buf, ll.offset + li * lmsz, lmsz);
    }

    return {
      verts: verts,
      meshverts: meshverts,
      faces: faces,
      lightmaps: lightmaps,
      textures: textures,
      planes: planes,
      nodes: nodes,
      leafs: leafs,
      leaffaces: leaffaces,
      leafbrushes: leafbrushes,
      brushes: brushes,
      brushsides: brushsides,
      entities: entities
    };
  }

  /* ──────────────────── BEZIER PATCH TESSELLATION ────────────────────── */
  /* Each vertex attribute array: [x, y, z, tu, tv, lmu, lmv, r, g, b, a] (11 floats) */

  function bez3(p0, p1, p2, t) {
    var mt = 1.0 - t;
    var out = new Array(p0.length);
    for (var i = 0; i < p0.length; i++) {
      out[i] = mt*mt*p0[i] + 2.0*mt*t*p1[i] + t*t*p2[i];
    }
    return out;
  }

  function tessPatch3x3(ctrl9, level) {
    /* ctrl9: 9 flat vertex arrays, row-major (top-left → top-right → … → bottom-right) */
    var L    = level;
    var W    = L + 1;
    var verts = [];

    for (var tj = 0; tj <= L; tj++) {
      var t  = tj / L;
      /* Interpolate each column of the 3×3 grid along t */
      var c0 = bez3(ctrl9[0], ctrl9[3], ctrl9[6], t);
      var c1 = bez3(ctrl9[1], ctrl9[4], ctrl9[7], t);
      var c2 = bez3(ctrl9[2], ctrl9[5], ctrl9[8], t);
      for (var si = 0; si <= L; si++) {
        verts.push(bez3(c0, c1, c2, si / L));
      }
    }

    var idxs = [];
    for (var tj2 = 0; tj2 < L; tj2++) {
      for (var si2 = 0; si2 < L; si2++) {
        var i = tj2 * W + si2;
        idxs.push(i, i+1, i+W);
        idxs.push(i+1, i+W+1, i+W);
      }
    }
    return { verts: verts, idxs: idxs };
  }

  /* ─────────────────── BUILD PER-LIGHTMAP BATCHES ────────────────────── */
  /*
   * Q3 coordinate system: X right, Y forward, Z up.
   * WebGL coordinate system: X right, Y up, Z backward.
   * Transform: (x, y, z)_q3 → (x, z, −y)_gl
   * Determinant of this rotation = +1, so triangle winding is preserved.
   */
  function q3ToGL(pos) {
    return [pos[0], pos[2], -pos[1]];
  }

  function vertToFlat(v) {
    var p = q3ToGL(v.pos);
    return [p[0], p[1], p[2], v.tc[0], v.tc[1], v.lm[0], v.lm[1], v.col[0], v.col[1], v.col[2], v.col[3]];
  }

  function buildBatches(bsp) {
    /* Map<lmIndex|texIndex, {pos[], tc[], lm[], col[], idx[], lmIdx, texIdx}> */
    var batches = {};

    function getBatch(lmIdx, texIdx) {
      var key = lmIdx + '|' + texIdx;
      if (!batches[key]) {
        batches[key] = { pos: [], tc: [], lm: [], col: [], idx: [], lmIdx: lmIdx, texIdx: texIdx };
      }
      return batches[key];
    }

    function addTri(batch, f0, f1, f2) {
      var base = batch.pos.length / 3;
      var faces_arr = [f0, f1, f2];
      for (var k = 0; k < 3; k++) {
        var f = faces_arr[k];
        batch.pos.push(f[0], f[1], f[2]);
        batch.tc.push(f[3], f[4]);
        batch.lm.push(f[5], f[6]);
        batch.col.push(f[7], f[8], f[9], f[10]);
      }
      batch.idx.push(base, base+1, base+2);
    }

    for (var fi = 0; fi < bsp.faces.length; fi++) {
      var face  = bsp.faces[fi];
      var batch = getBatch(face.lm_index, face.texture);

      if (face.type === FACE_POLYGON || face.type === FACE_MESH) {
        var n = face.n_meshverts;
        for (var j = 0; j < n; j += 3) {
          var i0 = bsp.meshverts[face.meshvert + j];
          var i1 = bsp.meshverts[face.meshvert + j + 1];
          var i2 = bsp.meshverts[face.meshvert + j + 2];
          var v0 = bsp.verts[face.vertex + i0];
          var v1 = bsp.verts[face.vertex + i1];
          var v2 = bsp.verts[face.vertex + i2];
          if (v0 && v1 && v2) addTri(batch, vertToFlat(v0), vertToFlat(v1), vertToFlat(v2));
        }

      } else if (face.type === FACE_PATCH) {
        var W = face.size[0];
        var H = face.size[1];
        if (W < 3 || H < 3) continue;
        /* Iterate over 3×3 sub-patches */
        for (var py = 0; py < (H - 1) / 2; py++) {
          for (var px = 0; px < (W - 1) / 2; px++) {
            var ctrl = [];
            for (var cy = 0; cy < 3; cy++) {
              for (var cx = 0; cx < 3; cx++) {
                var vi = (py*2+cy)*W + (px*2+cx);
                var sv = bsp.verts[face.vertex + vi];
                if (sv) {
                  var p = q3ToGL(sv.pos);
                  ctrl.push([p[0], p[1], p[2], sv.tc[0], sv.tc[1], sv.lm[0], sv.lm[1],
                             sv.col[0], sv.col[1], sv.col[2], sv.col[3]]);
                } else {
                  ctrl.push([0, 0, 0, 0, 0, 0, 0, 0.5, 0.5, 0.5, 1]);
                }
              }
            }
            var tess = tessPatch3x3(ctrl, PATCH_SUBDIV);
            var base = batch.pos.length / 3;
            for (var ti = 0; ti < tess.verts.length; ti++) {
              var tv = tess.verts[ti];
              batch.pos.push(tv[0], tv[1], tv[2]);
              batch.tc.push(tv[3], tv[4]);
              batch.lm.push(tv[5], tv[6]);
              batch.col.push(tv[7], tv[8], tv[9], tv[10]);
            }
            for (var ii = 0; ii < tess.idxs.length; ii++) {
              batch.idx.push(base + tess.idxs[ii]);
            }
          }
        }
      }
    }

    return batches;
  }

  /* ─────────────────────────── WEBGL HELPERS ─────────────────────────── */
  function compileShader(gl, type, src) {
    var sh = gl.createShader(type);
    gl.shaderSource(sh, src);
    gl.compileShader(sh);
    if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
      throw new Error('Shader compile error:\n' + gl.getShaderInfoLog(sh));
    }
    return sh;
  }

  function linkProgram(gl, vs, fs) {
    var prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      throw new Error('Program link error:\n' + gl.getProgramInfoLog(prog));
    }
    return prog;
  }

  function maxOfArray(arr) {
    var m = 0;
    for (var i = 0; i < arr.length; i++) if (arr[i] > m) m = arr[i];
    return m;
  }

  function hasNonOpaqueAlpha(rgba) {
    /* A texture has a "real" alpha channel for transparency only when it contains
     * at least one fully-transparent pixel (alpha === 0).  This deliberately
     * excludes 32-bit TGA files that store specular/gloss data in the alpha
     * channel – those never reach alpha 0, so they will not be misdetected as
     * transparent surfaces. */
    if (!rgba || !rgba.length) return false;
    for (var i = 3; i < rgba.length; i += 4) {
      if (rgba[i] === 0) return true;
    }
    return false;
  }

  function imageHasAlpha(img) {
    if (!img || !img.width || !img.height) return false;

    var canvas = document.createElement('canvas');
    canvas.width = img.width;
    canvas.height = img.height;
    var ctx = canvas.getContext('2d', { willReadFrequently: true });
    if (!ctx) return false;

    try {
      ctx.drawImage(img, 0, 0);
      return hasNonOpaqueAlpha(ctx.getImageData(0, 0, img.width, img.height).data);
    } catch (e) {
      return false;
    }
  }

  /* ──────────────────────────── BSP VIEWER ───────────────────────────── */
  function BSPViewer(canvas) {
    this.canvas = canvas;
    /* Session-level cache-busting token. Each BSPViewer instance gets a
       unique timestamp so fallback texture URLs bypass stale HTTP cache. */
    this._cacheBust = Date.now();

    var gl = canvas.getContext('webgl2', { alpha: false }) ||
             canvas.getContext('webgl',  { alpha: false }) ||
             canvas.getContext('experimental-webgl', { alpha: false });
    if (!gl) throw new Error('WebGL is not supported by your browser.');
    this.gl = gl;

    /* Uint32 index support */
    if (!canvas.getContext('webgl2')) {
      this._u32ext = gl.getExtension('OES_element_index_uint');
    } else {
      this._u32ext = true; /* WebGL2 has it natively */
    }

    this._drawCalls = [];
    this._running   = false;
    this._raf       = null;
    /* Brightness/gamma model now matches ET's pipeline:
       lightmap & vertex colors are overbright-shifted at upload time
       (R_ColorShiftLightingBytes); fragment shader applies gamma only.
       _brightness remains as a final user multiplier (defaults to 1.5 to
       approximate the combined effect of ET's overbright + hardware gamma
       ramp that the WebGL context cannot replicate exactly). */
    this._brightness     = 1.5;
    this._gamma          = 1.2;   /* ET default r_gamma */
    this._overbrightBits = 1;     /* ET default r_overBrightBits */
    this._wireframe  = false;
    /* Background color used by gl.clearColor(); also drives the fog color so
       that distance fog blends seamlessly with the void.  Stored as 0..1 RGB. */
    this._bgColor    = [0.03, 0.03, 0.08];
    /* Fog density in 1 / world-units.  0 disables fog. The default is a very
       light haze that mostly affects far geometry on large maps. */
    this._fogDensity = 0.00006;

    this._stats      = null;
    this.onProgress  = null;  /* function(loaded, total, assetName) */
    this._baseTexCache = {};
    this._shaderTexCache = {};
    this._shaderDefs = null;
    this._shaderDefsPromise = null;
    /* Worldspawn lighting / sky properties (populated on each BSP load). */
    this._minLight = 0;
    this._skyColorTop = null;
    this._skyColorHorizon = null;
    /* Bookmark slots ("1".."5") for free-fly camera positions. Persisted to
       localStorage keyed by BSP URL so per-map bookmarks survive reloads. */
    this._bookmarks    = {};
    this._bookmarksKey = null;

    /* Spawn-point overlay: a separate VBO of GL_LINES rendered with a tiny
       solid-color program. Populated from BSP entities (info_player_*, etc.)
       after each load. Off by default to avoid visual clutter. */
    this._spawnMarkers     = false;
    this._spawnVbo         = null;
    this._spawnVertexCount = 0;
    this._spawnCount       = 0;
    this._spawnProg        = null;
    this._spawnLoc         = null;

    /* Missing-model overlay: RGB triangles drawn at entity positions that
       reference external .md3/.mdc models which the viewer cannot load.
       Uses the same solid-color shader program as spawn markers. */
    this._modelMarkers     = false;
    this._modelVbo         = null;
    this._modelVertexCount = 0;
    this._modelCount       = 0;

    /* Loaded MD3/MDC models: array of {vbo, ibo, count, idxType, tex, modelMat} */
    this._md3Models        = [];
    this._md3Loading       = false;

    /* Asset diagnostics log – populated by _loadShaderMaterial and the sky
       cubemap loader. Surfaced via getAssetReport() / onAssetUpdate so the
       host UI can render a "missing textures" console. */
    this._assetLog = {
      shaderMissing: {},   /* shader name → array of candidate .shader files we searched */
      shaderNoImage: {},   /* shader resolved but its image refs all failed */
      imageFailed:   {},   /* image-only material whose URL candidates all 404'd */
      skyMissing:    {},   /* sky cubemap face → URLs tried */
      modelMissing:  {}    /* entity model path → entity classname */
    };
    this._assetUpdateTimer = null;
    this.onAssetUpdate = null;

    /* Sky / skybox state, populated by _initSkyProgram and the per-load sky
       loader. Defaults are inert: when no cubemap is loaded the world draws
       normally and the background colour fills the void. */
    this._skyProg          = null;
    this._skyLoc           = null;
    this._skyVbo           = null;
    this._skyIbo           = null;
    this._skyIndexCount    = 0;
    this._skyCubeTex       = null;
    this._skyIsReal        = false;
    this._skyFarBox        = '';   /* base path currently loaded (debug) */
    this._skyMissingFaces  = 0;
    this._hasSkyFaces      = false; /* true if any sky surface encountered */
    this._skyFaceCount     = 0;     /* number of world faces flagged as sky */
    /* Stage-based sky (the common ET case where the sky shader builds its
       appearance from layered cloud/mask passes rather than a farbox cubemap). */
    this._skyStageProg     = null;
    this._skyStageLoc      = null;
    this._skyStageVbo      = null;
    this._skyStageIbo      = null;
    this._skyStageIndexCount = 0;
    this._skyStages        = null; /* loaded stage list with GL textures */
    this._skyStagePromise  = null;

    /* Initialise the GL context, shader programs (including the sky and
       sky-stage programs) and camera last, so the inert sky-state defaults
       above don't clobber the GL resources created by _initGL(). */
    this._initGL();
    this._initCamera();
  }

  BSPViewer.prototype._reportProgress = function (loaded, total, name) {
    if (typeof this.onProgress === 'function') {
      this.onProgress(loaded, total, name);
    }
  };

  /* Record an asset diagnostics entry and notify the host UI (debounced
     so that a flood of resolutions during BSP load only triggers a few
     repaints). Categories: shaderMissing, shaderNoImage, imageFailed,
     skyMissing. `key` is the shader/face name, `urls` the candidate list. */
  BSPViewer.prototype._logAsset = function (category, key, urls) {
    if (!this._assetLog || !this._assetLog[category]) return;
    if (this._assetLog[category][key]) return; /* already recorded */
    this._assetLog[category][key] = (urls || []).slice();
    var self = this;
    if (this._assetUpdateTimer) return;
    this._assetUpdateTimer = setTimeout(function () {
      self._assetUpdateTimer = null;
      if (typeof self.onAssetUpdate === 'function') {
        try { self.onAssetUpdate(self.getAssetReport()); } catch (e) { /* swallow */ }
      }
    }, 150);
  };

  /* Snapshot of the asset log for the debug console.  Each category is an
     object: { shaderName: [urlsTried, …], … }. Also includes summary counts. */
  BSPViewer.prototype.getAssetReport = function () {
    var out = {
      shaderMissing: {}, shaderNoImage: {}, imageFailed: {}, skyMissing: {},
      modelMissing: {},
      counts: { shaderMissing: 0, shaderNoImage: 0, imageFailed: 0, skyMissing: 0, modelMissing: 0 }
    };
    if (!this._assetLog) return out;
    var cats = ['shaderMissing', 'shaderNoImage', 'imageFailed', 'skyMissing', 'modelMissing'];
    for (var c = 0; c < cats.length; c++) {
      var cat = cats[c];
      var src = this._assetLog[cat] || {};
      var keys = Object.keys(src).sort();
      for (var k = 0; k < keys.length; k++) {
        out[cat][keys[k]] = src[keys[k]].slice();
      }
      out.counts[cat] = keys.length;
    }
    return out;
  };

  /* ET-style overbright bit shift + desaturation.  Mirrors the engine's
     R_ColorShiftLightingBytes() in tr_image.c: shift each RGB channel left
     by `shift` bits and, if any channel saturates above 255, scale all
     three down uniformly so the chromaticity is preserved. Operates in
     place on a Uint8Array starting at offset `o`. */
  function colorShiftLightingBytes(buf, o, shift) {
    var r = buf[o]   << shift;
    var g = buf[o+1] << shift;
    var b = buf[o+2] << shift;
    var m = r;
    if (g > m) m = g;
    if (b > m) m = b;
    if (m > 255) {
      r = (r * 255 / m) | 0;
      g = (g * 255 / m) | 0;
      b = (b * 255 / m) | 0;
    }
    buf[o]   = r;
    buf[o+1] = g;
    buf[o+2] = b;
  }

  BSPViewer.prototype._initGL = function () {
    var gl = this.gl;
    gl.enable(gl.DEPTH_TEST);
    gl.depthFunc(gl.LEQUAL);
    gl.disable(gl.CULL_FACE); /* disable culling – viewer, not a game */
    gl.clearColor(this._bgColor[0], this._bgColor[1], this._bgColor[2], 1.0);

    var vs   = compileShader(gl, gl.VERTEX_SHADER,   VERT);
    var fs   = compileShader(gl, gl.FRAGMENT_SHADER, FRAG);
    this._prog = linkProgram(gl, vs, fs);

    this._loc = {
      aPos:        gl.getAttribLocation(this._prog,  'aPos'),
      aTex:        gl.getAttribLocation(this._prog,  'aTex'),
      aLM:         gl.getAttribLocation(this._prog,  'aLM'),
      aCol:        gl.getAttribLocation(this._prog,  'aCol'),
      uMVP:        gl.getUniformLocation(this._prog, 'uMVP'),
      uView:       gl.getUniformLocation(this._prog, 'uView'),
      uBaseTex:    gl.getUniformLocation(this._prog, 'uBaseTex'),
      uLMTex:      gl.getUniformLocation(this._prog, 'uLMTex'),
      uHasBase:    gl.getUniformLocation(this._prog, 'uHasBase'),
      uHasLM:      gl.getUniformLocation(this._prog, 'uHasLM'),
      uBrightness: gl.getUniformLocation(this._prog, 'uBrightness'),
      uGamma:      gl.getUniformLocation(this._prog, 'uGamma'),
      uFogDensity: gl.getUniformLocation(this._prog, 'uFogDensity'),
      uFogColor:   gl.getUniformLocation(this._prog, 'uFogColor'),
      uTime:       gl.getUniformLocation(this._prog, 'uTime'),
      uTcScroll:   gl.getUniformLocation(this._prog, 'uTcScroll'),
      uTcRotate:   gl.getUniformLocation(this._prog, 'uTcRotate'),
      uWireOverlay:gl.getUniformLocation(this._prog, 'uWireOverlay')
    };

    /* 1×1 grey fallback texture (used when no lightmap) */
    this._greyTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this._greyTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
      new Uint8Array([128, 128, 128, 255]));
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);

    /* 1×1 white fallback texture (used when no base texture) */
    this._whiteTex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, this._whiteTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE,
      new Uint8Array([255, 255, 255, 255]));
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);

    /* Minimal solid-color program for the spawn-point overlay. Takes a
       per-vertex RGB color and a single uMVP uniform; no textures, no fog. */
    var svs = compileShader(gl, gl.VERTEX_SHADER,
      'attribute vec3 aPos;' +
      'attribute vec3 aCol;' +
      'uniform mat4 uMVP;' +
      'varying vec3 vCol;' +
      'void main(){ vCol = aCol; gl_Position = uMVP * vec4(aPos, 1.0); }'
    );
    var sfs = compileShader(gl, gl.FRAGMENT_SHADER,
      'precision mediump float;' +
      'varying vec3 vCol;' +
      'void main(){ gl_FragColor = vec4(vCol, 1.0); }'
    );
    this._spawnProg = linkProgram(gl, svs, sfs);
    this._spawnLoc = {
      aPos: gl.getAttribLocation(this._spawnProg,  'aPos'),
      aCol: gl.getAttribLocation(this._spawnProg,  'aCol'),
      uMVP: gl.getUniformLocation(this._spawnProg, 'uMVP')
    };

    this._initSkyProgram();
    this._initSkyStageProgram();
  };

  /* Sky program: samples a cubemap using the world-space direction from the
     camera to each fragment. The skybox itself is drawn as a unit cube
     centred on the camera (translation removed from the view matrix) with
     depth writes disabled so the world geometry renders on top of it. */
  BSPViewer.prototype._initSkyProgram = function () {
    var gl = this.gl;
    var svs = compileShader(gl, gl.VERTEX_SHADER,
      'attribute vec3 aPos;' +
      'uniform mat4 uVP;' +
      'varying vec3 vDir;' +
      'void main(){' +
      '  vDir = aPos;' +
      '  vec4 p = uVP * vec4(aPos, 1.0);' +
      /* Force depth to far plane so the skybox stays behind everything else
         while still passing gl.LEQUAL when depth buffer is cleared to 1.0. */
      '  gl_Position = p.xyww;' +
      '}'
    );
    var sfs = compileShader(gl, gl.FRAGMENT_SHADER,
      'precision mediump float;' +
      'uniform samplerCube uCube;' +
      'uniform float uGamma;' +
      'uniform float uBrightness;' +
      'uniform float uUseCubemap;' +
      'uniform vec3  uSkyColorTop;' +
      'uniform vec3  uSkyColorHorizon;' +
      'varying vec3 vDir;' +
      'void main(){' +
      '  vec3 dir = normalize(vDir);' +
      '  vec3 c;' +
      '  if (uUseCubemap > 0.5) {' +
      '    c = textureCube(uCube, dir).rgb * uBrightness;' +
      '  } else {' +
      /* Procedural gradient sky: blend between horizon and zenith based on
         the vertical component of the view direction. This provides a
         convincing sky even when no cubemap textures are available. */
      '    float h = clamp(dir.y, 0.0, 1.0);' +
      '    c = mix(uSkyColorHorizon, uSkyColorTop, h) * uBrightness;' +
      '  }' +
      '  float invG = 1.0 / max(uGamma, 0.01);' +
      '  c = clamp(pow(max(c, vec3(0.0)), vec3(invG)), 0.0, 1.0);' +
      '  gl_FragColor = vec4(c, 1.0);' +
      '}'
    );
    this._skyProg = linkProgram(gl, svs, sfs);
    this._skyLoc = {
      aPos:             gl.getAttribLocation(this._skyProg,  'aPos'),
      uVP:              gl.getUniformLocation(this._skyProg, 'uVP'),
      uCube:            gl.getUniformLocation(this._skyProg, 'uCube'),
      uGamma:           gl.getUniformLocation(this._skyProg, 'uGamma'),
      uBrightness:      gl.getUniformLocation(this._skyProg, 'uBrightness'),
      uUseCubemap:      gl.getUniformLocation(this._skyProg, 'uUseCubemap'),
      uSkyColorTop:     gl.getUniformLocation(this._skyProg, 'uSkyColorTop'),
      uSkyColorHorizon: gl.getUniformLocation(this._skyProg, 'uSkyColorHorizon')
    };

    /* Unit cube spanning [-1,1]^3 with inward-facing winding (we disable
       culling globally so winding doesn't matter, but keep CCW for clarity). */
    var verts = new Float32Array([
      -1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
      -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1
    ]);
    var idx = new Uint16Array([
      0,2,1, 0,3,2,   /* -Z */
      4,5,6, 4,6,7,   /* +Z */
      0,4,7, 0,7,3,   /* -X */
      1,2,6, 1,6,5,   /* +X */
      3,7,6, 3,6,2,   /* +Y */
      0,1,5, 0,5,4    /* -Y */
    ]);
    this._skyVbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this._skyVbo);
    gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STATIC_DRAW);
    this._skyIbo = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this._skyIbo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idx, gl.STATIC_DRAW);
    this._skyIndexCount = idx.length;
  };

  /* Sky-stage program: draws the sky shader's layered cloud/mask passes on a
     unit cube whose faces carry planar [0,1] texture coordinates (matching
     ET's per-face sky tex-coord generation in renderer/tr_sky.c MakeSkyVec).
     Each stage supplies a 2D texture, an affine tcMod matrix and an RGB tint;
     the caller selects the blend mode per pass. */
  BSPViewer.prototype._initSkyStageProgram = function () {
    var gl = this.gl;
    var vs = compileShader(gl, gl.VERTEX_SHADER,
      'attribute vec3 aPos;' +
      'attribute vec2 aUV;' +
      'uniform mat4 uVP;' +
      'uniform mat3 uTc;' +
      'varying vec2 vUV;' +
      'void main(){' +
      '  vec3 t = uTc * vec3(aUV, 1.0);' +
      '  vUV = t.xy;' +
      '  vec4 p = uVP * vec4(aPos, 1.0);' +
      /* Keep the sky at the far plane (same trick as the cubemap program). */
      '  gl_Position = p.xyww;' +
      '}'
    );
    var fs = compileShader(gl, gl.FRAGMENT_SHADER,
      'precision mediump float;' +
      'uniform sampler2D uTex;' +
      'uniform vec3  uColor;' +
      'uniform float uBrightness;' +
      'uniform float uGamma;' +
      'varying vec2 vUV;' +
      'void main(){' +
      '  vec4 tx = texture2D(uTex, vUV);' +
      '  vec3 c = tx.rgb * uColor * uBrightness;' +
      '  float invG = 1.0 / max(uGamma, 0.01);' +
      '  c = clamp(pow(max(c, vec3(0.0)), vec3(invG)), 0.0, 1.0);' +
      '  gl_FragColor = vec4(c, tx.a);' +
      '}'
    );
    this._skyStageProg = linkProgram(gl, vs, fs);
    this._skyStageLoc = {
      aPos:        gl.getAttribLocation(this._skyStageProg,  'aPos'),
      aUV:         gl.getAttribLocation(this._skyStageProg,  'aUV'),
      uVP:         gl.getUniformLocation(this._skyStageProg, 'uVP'),
      uTc:         gl.getUniformLocation(this._skyStageProg, 'uTc'),
      uTex:        gl.getUniformLocation(this._skyStageProg, 'uTex'),
      uColor:      gl.getUniformLocation(this._skyStageProg, 'uColor'),
      uBrightness: gl.getUniformLocation(this._skyStageProg, 'uBrightness'),
      uGamma:      gl.getUniformLocation(this._skyStageProg, 'uGamma')
    };

    /* Cube with 6 independent faces (4 verts each), interleaved pos(3)+uv(2).
       Each face's UVs span the full [0,1] range so a cloud texture tiles once
       per face (tcMod scale/scroll then animate it). */
    function face(ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz) {
      return [ax,ay,az, 0,0,  bx,by,bz, 1,0,  cx,cy,cz, 1,1,  dx,dy,dz, 0,1];
    }
    var v = [].concat(
      face(-1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1), /* -Z */
      face( 1,-1, 1, -1,-1, 1, -1, 1, 1,  1, 1, 1), /* +Z */
      face(-1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1, 1), /* -X */
      face( 1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1,-1), /* +X */
      face(-1, 1,-1,  1, 1,-1,  1, 1, 1, -1, 1, 1), /* +Y */
      face(-1,-1, 1,  1,-1, 1,  1,-1,-1, -1,-1,-1)  /* -Y */
    );
    var idx = [];
    for (var f = 0; f < 6; f++) {
      var o = f * 4;
      idx.push(o, o + 1, o + 2, o, o + 2, o + 3);
    }
    this._skyStageVbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this._skyStageVbo);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(v), gl.STATIC_DRAW);
    this._skyStageIbo = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this._skyStageIbo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(idx), gl.STATIC_DRAW);
    this._skyStageIndexCount = idx.length;
  };

  /* Compose a stage's tcMod operations into a single affine matrix applied in
     ET's canonical order (scale → transform → rotate → scroll). `time` is the
     monotonic viewer time in seconds, used to animate scroll/rotate. Returns a
     column-major mat3 suitable for uniformMatrix3fv. */
  function skyStageTcMatrix(stage, time) {
    /* affine = { a,b,c,d,e,f } with s' = a*s + c*t + e, t' = b*s + d*t + f */
    function ident() { return { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }; }
    function compose(A, B) { /* apply B first, then A */
      return {
        a: A.a * B.a + A.c * B.b,
        b: A.b * B.a + A.d * B.b,
        c: A.a * B.c + A.c * B.d,
        d: A.b * B.c + A.d * B.d,
        e: A.a * B.e + A.c * B.f + A.e,
        f: A.b * B.e + A.d * B.f + A.f
      };
    }
    var acc = ident();
    if (stage.tcScale) {
      acc = compose({ a: stage.tcScale[0], b: 0, c: 0, d: stage.tcScale[1], e: 0, f: 0 }, acc);
    }
    if (stage.tcTransform) {
      var t = stage.tcTransform;
      acc = compose({ a: t[0], b: t[1], c: t[2], d: t[3], e: t[4], f: t[5] }, acc);
    }
    if (stage.tcRotate) {
      var ang = stage.tcRotate * time;
      var cs = Math.cos(ang), sn = Math.sin(ang);
      /* Rotate around the (0.5,0.5) texture centre, like Q3's tcMod rotate. */
      var rot = compose({ a: cs, b: sn, c: -sn, d: cs, e: 0, f: 0 },
                        { a: 1, b: 0, c: 0, d: 1, e: -0.5, f: -0.5 });
      rot = compose({ a: 1, b: 0, c: 0, d: 1, e: 0.5, f: 0.5 }, rot);
      acc = compose(rot, acc);
    }
    if (stage.tcScrollS || stage.tcScrollT) {
      acc = compose({ a: 1, b: 0, c: 0, d: 1, e: stage.tcScrollS * time, f: stage.tcScrollT * time }, acc);
    }
    return new Float32Array([acc.a, acc.b, 0, acc.c, acc.d, 0, acc.e, acc.f, 1]);
  }

  /* Load the 2D textures for a sky shader's stages. Each resolved stage gets a
     `.tex` (WebGL texture) and `.wrap` (gl wrap mode). Returns a Promise that
     resolves to the populated stage array (textures that fail to load are
     logged and left null so the stage is skipped at draw time). */
  BSPViewer.prototype._loadSkyStages = function (stages) {
    var self = this;
    var gl   = this.gl;
    if (!stages || !stages.length) return Promise.resolve([]);

    return Promise.all(stages.map(function (s) {
      var out = {
        clamp: !!s.clamp, blend: s.blend || 'opaque',
        tcScrollS: s.tcScrollS || 0, tcScrollT: s.tcScrollT || 0,
        tcScale: s.tcScale, tcRotate: s.tcRotate || 0, tcTransform: s.tcTransform,
        rgbConst: s.rgbConst, tex: null
      };
      if (!s.map) return Promise.resolve(out);
      var urls = self._shaderToCandidates(s.map);
      return self._loadImageFromUrls(urls).then(function (texInfo) {
        if (texInfo && texInfo.tex) {
          out.tex = texInfo.tex;
          /* clampmap masks must clamp. Non-clamp layers keep the texture's
             default wrap (REPEAT for power-of-two, CLAMP for NPOT) which is
             already valid in WebGL1 – forcing REPEAT on a NPOT cloud texture
             would render it black. */
          if (out.clamp) {
            gl.bindTexture(gl.TEXTURE_2D, out.tex);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
          }
        } else {
          self._logAsset('skyMissing', s.map, urls);
        }
        return out;
      });
    }));
  };

  /* Free the GL textures held by the loaded sky stage layers (if any) and
     clear the list. Shared by map reloads and dispose. */
  BSPViewer.prototype._disposeSkyStageTextures = function () {
    if (!this._skyStages) return;
    for (var i = 0; i < this._skyStages.length; i++) {
      if (this._skyStages[i] && this._skyStages[i].tex) {
        this.gl.deleteTexture(this._skyStages[i].tex);
      }
    }
    this._skyStages = null;
  };

  /* Cubemap face suffix → WebGL face target. ET's `sky_axis` table in
     renderer/tr_sky.c maps the legacy Quake convention:
       _ft = front (+X looking in-game), _bk = back (-X),
       _rt = right (-Y), _lf = left (+Y),
       _up = top (+Z), _dn = bottom (-Z).
     WebGL cubemaps use a left-handed [+X,-X,+Y,-Y,+Z,-Z] order where +Y is
     up. We rotate Q3's Z-up world to GL's Y-up at draw time by sampling the
     cubemap with a swapped direction (see _drawSky), so the face mapping
     here uses GL axes directly:
       +X → ft, -X → bk, +Y → up, -Y → dn, +Z → rt, -Z → lf. */
  var SKY_FACE_TARGETS = null;
  function getSkyFaceTargets(gl) {
    if (!SKY_FACE_TARGETS) {
      SKY_FACE_TARGETS = [
        { suffix: 'ft', target: gl.TEXTURE_CUBE_MAP_POSITIVE_X },
        { suffix: 'bk', target: gl.TEXTURE_CUBE_MAP_NEGATIVE_X },
        { suffix: 'up', target: gl.TEXTURE_CUBE_MAP_POSITIVE_Y },
        { suffix: 'dn', target: gl.TEXTURE_CUBE_MAP_NEGATIVE_Y },
        { suffix: 'rt', target: gl.TEXTURE_CUBE_MAP_POSITIVE_Z },
        { suffix: 'lf', target: gl.TEXTURE_CUBE_MAP_NEGATIVE_Z }
      ];
    }
    return SKY_FACE_TARGETS;
  }

  /* Load a single image for a skybox face.  Tries the given URL list
     (different roots & extensions) and resolves with an HTMLImageElement
     or HTMLCanvasElement (for fetched/decoded .tga sources), or null if
     every candidate failed.  Both element types are accepted by
     `gl.texImage2D` and expose `.width` / `.height`. */
  BSPViewer.prototype._loadImageElementFromUrls = function (urls) {
    function loadOne(url) {
      /* .tga can't be decoded by the browser's image pipeline – fetch the
         bytes ourselves, run them through decodeTGA, and bake the result
         into a 2D canvas so the cubemap upload path stays uniform. */
      if (/\.tga(?:[?&#]|$)/i.test(decodeURIComponent(url))) {
        return fetch(url).then(function (r) {
          if (!r.ok) return null;
          return r.arrayBuffer();
        }).then(function (buf) {
          if (!buf) return null;
          var dec = decodeTGA(buf);
          if (!dec) return null;
          var cv = document.createElement('canvas');
          cv.width = dec.width; cv.height = dec.height;
          var ctx = cv.getContext('2d');
          var imgData = ctx.createImageData(dec.width, dec.height);
          imgData.data.set(dec.data);
          ctx.putImageData(imgData, 0, 0);
          return cv;
        }).catch(function () { return null; });
      }
      return new Promise(function (resolve) {
        var img = new Image();
        if (/^https?:\/\//i.test(url)) img.crossOrigin = 'anonymous';
        img.onload  = function () { resolve(img); };
        img.onerror = function () { resolve(null); };
        img.src = url;
      });
    }
    function tryIdx(i) {
      if (i >= urls.length) return Promise.resolve(null);
      return loadOne(urls[i]).then(function (img) {
        if (img) return img;
        return tryIdx(i + 1);
      });
    }
    return tryIdx(0);
  };

  /* Build the GL_TEXTURE_CUBE_MAP for the current map's `skyparms` farbox.
     basePath is the path *without* the `_xx` suffix or extension, e.g.
     "env/country/country". Returns a Promise resolving when all six faces
     have either loaded or been logged as missing. */
  BSPViewer.prototype._loadSkyCubemap = function (basePath) {
    if (!basePath) return Promise.resolve(false);
    var self = this;
    var gl   = this.gl;

    /* Drop any previously loaded skybox. */
    if (this._skyCubeTex) { gl.deleteTexture(this._skyCubeTex); this._skyCubeTex = null; }
    this._skyFarBox = basePath;
    this._skyMissingFaces = 0;

    var faces = getSkyFaceTargets(gl);
    /* Try a couple of candidate paths per face (jpg/tga/png in several roots). */
    function urlsFor(suffix) {
      var name = basePath + '_' + suffix;
      /* Reuse the texture candidate resolver – it already understands the
         per-map / etmain / manifest fall-backs. */
      return self._shaderToCandidates(name);
    }

    var imgs = new Array(6);
    return Promise.all(faces.map(function (f, idx) {
      var urls = urlsFor(f.suffix);
      return self._loadImageElementFromUrls(urls).then(function (img) {
        imgs[idx] = img;
        if (!img) {
          self._skyMissingFaces++;
          self._logAsset('skyMissing', basePath + '_' + f.suffix, urls);
        }
      });
    })).then(function () {
      /* Need at least one face to bother creating a cubemap. */
      var anyLoaded = imgs.some(function (i) { return !!i; });
      if (!anyLoaded) return false;

      var tex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_CUBE_MAP, tex);
      /* Use the first loaded image's size as the cubemap resolution and
         fill missing faces with that size (1×1 transparent black). All
         faces of a cubemap must have identical dimensions. */
      var ref = null;
      for (var ri = 0; ri < imgs.length; ri++) { if (imgs[ri]) { ref = imgs[ri]; break; } }
      var refW = ref.width, refH = ref.height;
      for (var i = 0; i < 6; i++) {
        if (imgs[i] && imgs[i].width === refW && imgs[i].height === refH) {
          gl.texImage2D(faces[i].target, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, imgs[i]);
        } else if (imgs[i]) {
          /* Different size: rescale via 2D canvas so the cubemap is valid. */
          var cv = document.createElement('canvas');
          cv.width = refW; cv.height = refH;
          var ctx = cv.getContext('2d');
          ctx.drawImage(imgs[i], 0, 0, refW, refH);
          gl.texImage2D(faces[i].target, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, cv);
        } else {
          /* Missing face – upload a fixed near-black colour so the seam is
             not random GPU memory and is independent of the BG preset. */
          var pix = new Uint8Array(refW * refH * 4);
          for (var p = 0; p < pix.length; p += 4) { pix[p]=5; pix[p+1]=5; pix[p+2]=12; pix[p+3]=255; }
          gl.texImage2D(faces[i].target, 0, gl.RGBA, refW, refH, 0, gl.RGBA, gl.UNSIGNED_BYTE, pix);
        }
      }
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      self._skyCubeTex = tex;
      self._skyIsReal = true;
      return true;
    });
  };

  /* Draw the sky. Order of preference:
       1. If the sky shader has opaque stage layers, those provide the backdrop
          and are drawn first (in the stage pass below).
       2. Otherwise draw the farbox cubemap if one loaded, else a procedural
          gradient, as the backdrop.
       3. Any additive / blend / filter stage layers (scrolling clouds, masks)
          are then composited on top.
     Called from _draw before the world; the view matrix has its translation
     stripped so the box stays centred on the camera and depth writes are off
     so subsequent world draws overlay correctly. */
  BSPViewer.prototype._drawSky = function (proj, view) {
    if (!this._skyProg) return;
    var readyStages = [];
    if (this._skyStages) {
      for (var si = 0; si < this._skyStages.length; si++) {
        if (this._skyStages[si] && this._skyStages[si].tex) readyStages.push(this._skyStages[si]);
      }
    }
    if (!this._hasSkyFaces && !this._skyCubeTex && !readyStages.length) return;

    /* Build a "rotation-only" view matrix by zeroing the translation
       column. Then build VP = proj * viewRot. */
    var v = new Float32Array(view);
    v[12] = 0; v[13] = 0; v[14] = 0;
    var vp = matMul(proj, v);

    var hasOpaqueStage = false;
    for (var oi = 0; oi < readyStages.length; oi++) {
      if (readyStages[oi].blend === 'opaque') { hasOpaqueStage = true; break; }
    }

    /* Backdrop: cubemap or gradient – skipped when an opaque stage will paint
       the whole sky itself. */
    if (!hasOpaqueStage) this._drawSkyBackdrop(vp);

    /* Stage passes (clouds / masks / night-sky layers). */
    if (readyStages.length) this._drawSkyStages(vp, readyStages);
  };

  /* Draw the cubemap (if a real farbox loaded) or the procedural gradient as
     the sky backdrop, using the cubemap sky program. */
  BSPViewer.prototype._drawSkyBackdrop = function (vp) {
    var gl = this.gl;
    var hasCubemap = !!(this._skyCubeTex && this._skyIsReal);

    /* If no cubemap loaded, create a 1×1 dummy so the sampler doesn't error
       (the shader won't actually sample it when uUseCubemap == 0). */
    if (!this._skyCubeTex) {
      var fb = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_CUBE_MAP, fb);
      var pix = new Uint8Array([0, 0, 0, 255]);
      for (var fi = 0; fi < 6; fi++) {
        gl.texImage2D(gl.TEXTURE_CUBE_MAP_POSITIVE_X + fi, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, pix);
      }
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      this._skyCubeTex = fb;
      this._skyIsReal = false;
    }

    gl.useProgram(this._skyProg);
    gl.depthFunc(gl.LEQUAL);
    gl.depthMask(false);
    gl.disable(gl.BLEND);
    gl.bindBuffer(gl.ARRAY_BUFFER,         this._skyVbo);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this._skyIbo);
    gl.enableVertexAttribArray(this._skyLoc.aPos);
    gl.vertexAttribPointer(this._skyLoc.aPos, 3, gl.FLOAT, false, 0, 0);
    gl.uniformMatrix4fv(this._skyLoc.uVP, false, vp);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_CUBE_MAP, this._skyCubeTex);
    gl.uniform1i(this._skyLoc.uCube, 0);
    if (this._skyLoc.uGamma)      gl.uniform1f(this._skyLoc.uGamma, this._gamma || 1.0);
    if (this._skyLoc.uBrightness) gl.uniform1f(this._skyLoc.uBrightness, this._brightness || 1.0);
    /* Toggle between cubemap sampling and procedural gradient */
    gl.uniform1f(this._skyLoc.uUseCubemap, hasCubemap ? 1.0 : 0.0);
    /* Sky gradient colours – use worldspawn _color if available, else a
       pleasant blue gradient loosely matching ET's typical outdoor skies. */
    var top = this._skyColorTop     || [0.15, 0.25, 0.55];
    var hor = this._skyColorHorizon || [0.45, 0.55, 0.70];
    gl.uniform3f(this._skyLoc.uSkyColorTop,     top[0], top[1], top[2]);
    gl.uniform3f(this._skyLoc.uSkyColorHorizon, hor[0], hor[1], hor[2]);
    gl.drawElements(gl.TRIANGLES, this._skyIndexCount, gl.UNSIGNED_SHORT, 0);
    gl.disableVertexAttribArray(this._skyLoc.aPos);
    gl.depthMask(true);
  };

  /* Composite the sky shader's stage layers (clouds, masks, night sky) onto
     the sky cube, honouring each stage's blend mode, tcMod animation and tint. */
  BSPViewer.prototype._drawSkyStages = function (vp, stages) {
    var gl = this.gl;
    if (!this._skyStageProg || !stages.length) return;

    if (!this._t0) this._t0 = performance.now();
    var time = (performance.now() - this._t0) * 0.001;

    gl.useProgram(this._skyStageProg);
    gl.depthFunc(gl.LEQUAL);
    gl.depthMask(false);
    gl.bindBuffer(gl.ARRAY_BUFFER,         this._skyStageVbo);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this._skyStageIbo);
    var stride = 5 * 4;
    gl.enableVertexAttribArray(this._skyStageLoc.aPos);
    gl.vertexAttribPointer(this._skyStageLoc.aPos, 3, gl.FLOAT, false, stride, 0);
    gl.enableVertexAttribArray(this._skyStageLoc.aUV);
    gl.vertexAttribPointer(this._skyStageLoc.aUV, 2, gl.FLOAT, false, stride, 3 * 4);
    gl.uniformMatrix4fv(this._skyStageLoc.uVP, false, vp);
    gl.uniform1i(this._skyStageLoc.uTex, 0);
    gl.uniform1f(this._skyStageLoc.uGamma, this._gamma || 1.0);
    gl.uniform1f(this._skyStageLoc.uBrightness, this._brightness || 1.0);
    gl.activeTexture(gl.TEXTURE0);

    for (var i = 0; i < stages.length; i++) {
      var s = stages[i];
      if (!s.tex) continue;
      /* Blend state per layer role. */
      if (s.blend === 'opaque') {
        gl.disable(gl.BLEND);
      } else {
        gl.enable(gl.BLEND);
        if (s.blend === 'add') {
          gl.blendFunc(gl.ONE, gl.ONE);
        } else if (s.blend === 'filter') {
          gl.blendFunc(gl.DST_COLOR, gl.ZERO);
        } else { /* 'blend' */
          gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
        }
      }
      var col = s.rgbConst || [1, 1, 1];
      gl.uniform3f(this._skyStageLoc.uColor, col[0], col[1], col[2]);
      gl.uniformMatrix3fv(this._skyStageLoc.uTc, false, skyStageTcMatrix(s, time));
      gl.bindTexture(gl.TEXTURE_2D, s.tex);
      gl.drawElements(gl.TRIANGLES, this._skyStageIndexCount, gl.UNSIGNED_SHORT, 0);
    }

    gl.disableVertexAttribArray(this._skyStageLoc.aPos);
    gl.disableVertexAttribArray(this._skyStageLoc.aUV);
    gl.disable(gl.BLEND);
    gl.depthMask(true);
  };

  BSPViewer.prototype._initCamera = function () {
    this._cam = { pos: [0, 100, 0], yaw: 0, pitch: 0, speed: 300, baseSpeed: 300, speedMul: 1.0 };
    this._keys = {};
    this._pLock = false;
    // First-person ("FPS") walk mode. When enabled:
    //   • horizontal movement only (vertical mouse component ignored for WASD)
    //   • gravity pulls the camera down toward the collision floor
    //   • walls are blocked by axis-separated collision tests against BSP tris
    //   • Space = jump (triple jump – up to 3 jumps in the air), no fly-up
    this._fps     = false;
    this._fpsVY   = 0;      // current vertical velocity (units/s)
    this._fpsGround = this._cam.pos[1];  // resting Y (set at toggle time)
    this._fpsOnGround = false;  // whether player is on a floor surface
    this._fpsGroundNormal = [0, 1, 0]; // GL-space ground plane normal (Y-up)
    this._fpsEyeH   = 56;   // typical ET eye height above feet (units)
    this._fpsRadius = 18;   // player half-width for wall checks (units)
    this._fpsStepUp = 18;   // max step-up height for stairs (units, ET uses 18)
    this._fpsJumpV  = 270;  // jump impulse (units/s)
    this._fpsGravity= 800;  // units/s²
    this._fpsJumpsLeft = 3; // triple jump: max 3 jumps before landing
    this._fpsMaxJumps  = 3;
    this._fpsJumpHeld  = false;  // debounce for Space key
    /* Step smoothing (Q3-style): when stepping up/down stairs the actual
       player position changes instantly but the camera view offset is
       interpolated to make it look smooth. */
    this._fpsStepOffset = 0;       // current visual offset being smoothed out
    this._fpsStepTime   = 0;       // remaining smooth time (seconds)

    var self   = this;
    var canvas = this.canvas;

    this._onCanvasClick = function (event) {
      if (self.onNavigationClick && self.onNavigationClick(event)) return;
      if (self.onNavigationPlace) {
        self.onNavigationPlace(self.navigationFloorPoint(event.clientX, event.clientY));
        return;
      }
      if (self._navigationInspect !== false) {
        self.pickNavigation(event.clientX, event.clientY);
      } else if (!self._pLock && canvas.requestPointerLock) {
        var locked = canvas.requestPointerLock();
        if (locked && locked.catch) locked.catch(function () {});
      }
    };
    canvas.addEventListener('click', this._onCanvasClick);

    this._onPLC = function () {
      self._pLock = (document.pointerLockElement === canvas);
    };
    this._onMM = function (e) {
      if (!self._pLock) return;
      self._cam.yaw   += e.movementX * 0.003;
      self._cam.pitch -= e.movementY * 0.003;
      var MAX = Math.PI / 2 - 0.01;
      if (self._cam.pitch >  MAX) self._cam.pitch =  MAX;
      if (self._cam.pitch < -MAX) self._cam.pitch = -MAX;
    };
    this._onKD = function (e) {
      if ((self._navigationInspect !== false && !self._radiantNavigating) || /^(INPUT|TEXTAREA|SELECT|BUTTON)$/.test((e.target || {}).tagName)) return;
      self._keys[e.code] = true;
      if (/^(Key[WASDQE]|Arrow(Up|Down|Left|Right)|Space|KeyC)$/.test(e.code)) e.preventDefault();
      /* N toggles between free-fly and first-person (walk) mode.
         Mirrors the on-screen "First-person" button. We dispatch a
         custom event so any external UI (button label, help text)
         can re-render. */
      if (e.code === 'KeyN' && !e.repeat && !e.ctrlKey && !e.metaKey && !e.altKey) {
        var on = self.setFirstPerson();
        try {
          self.canvas.dispatchEvent(new CustomEvent('bsp:fpschange', { detail: { on: on } }));
        } catch (_) { /* old browsers */ }
        e.preventDefault();
      }
    };
    this._onKU = function (e) { delete self._keys[e.code];  };

    /* Touch look – drag anywhere on the canvas to rotate the camera. We track
       a single dedicated "look" finger by its identifier so that a second
       finger on the on-screen joystick / buttons can move at the same time. */
    var _lookId = null, _tx = null, _ty = null;
    canvas.addEventListener('touchstart', function (e) {
      e.preventDefault();
      if (_lookId === null) {
        var t = e.changedTouches[0];
        _lookId = t.identifier;
        _tx = t.clientX;
        _ty = t.clientY;
      }
    }, { passive: false });
    canvas.addEventListener('touchmove', function (e) {
      if (_lookId === null) return;
      e.preventDefault();
      for (var i = 0; i < e.changedTouches.length; i++) {
        var t = e.changedTouches[i];
        if (t.identifier !== _lookId) continue;
        var dx = t.clientX - _tx;
        var dy = t.clientY - _ty;
        _tx = t.clientX;
        _ty = t.clientY;
        var MAX = Math.PI / 2 - 0.01;
        self._cam.yaw   += dx * 0.005;
        self._cam.pitch -= dy * 0.005;
        if (self._cam.pitch >  MAX) self._cam.pitch =  MAX;
        if (self._cam.pitch < -MAX) self._cam.pitch = -MAX;
      }
    }, { passive: false });
    function _touchLookEnd(e) {
      for (var i = 0; i < e.changedTouches.length; i++) {
        if (e.changedTouches[i].identifier === _lookId) {
          _lookId = null; _tx = null; _ty = null;
          break;
        }
      }
    }
    canvas.addEventListener('touchend',    _touchLookEnd, { passive: false });
    canvas.addEventListener('touchcancel', _touchLookEnd, { passive: false });

    document.addEventListener('pointerlockchange', this._onPLC);
    document.addEventListener('mousemove',         this._onMM);
    document.addEventListener('keydown',           this._onKD);
    document.addEventListener('keyup',             this._onKU);
  };

  BSPViewer.prototype.setVirtualKey = function (code, pressed) {
    if (pressed) this._keys[code] = true;
    else delete this._keys[code];
  };

  /**
   * Toggle first-person ("FPS") walk mode on or off. When enabled the camera
   * is constrained to walk along a horizontal plane (gravity + jump) instead
   * of free-flying through the level. Returns the new mode (boolean).
   * Pass an explicit boolean to force a specific state.
   */
  BSPViewer.prototype.setFirstPerson = function (on) {
    if (typeof on === 'undefined') on = !this._fps;
    this._fps = !!on;
    if (this._fps) {
      // Snap to the floor below current position using brush trace.
      var PM_MINS = [-this._fpsRadius, -this._fpsRadius, -24];
      var PM_MAXS = [ this._fpsRadius,  this._fpsRadius, 32];
      var boxOriginGL = [this._cam.pos[0], this._cam.pos[1] - this._fpsEyeH + 24, this._cam.pos[2]];
      var downGL = [boxOriginGL[0], boxOriginGL[1] - 4096, boxOriginGL[2]];
      var tr = this._traceBox(boxOriginGL, downGL, PM_MINS, PM_MAXS);
      if (tr.fraction < 1.0) {
        var hitY = boxOriginGL[1] + (downGL[1] - boxOriginGL[1]) * tr.fraction;
        this._cam.pos[1] = hitY + this._fpsEyeH - 24;
        this._fpsOnGround = true;
        this._fpsGroundNormal = tr.normal;
      } else {
        this._fpsOnGround = false;
      }
      this._fpsGround = this._cam.pos[1];
      this._fpsVY = 0;
    }
    return this._fps;
  };
  BSPViewer.prototype.isFirstPerson = function () { return !!this._fps; };

  /**
   * Set movement-speed multiplier (typical range 0.1 – 3.0). Applied on top of
   * the per-map base speed derived from map bounds.
   */
  BSPViewer.prototype.setSpeedMultiplier = function (mul) {
    var m = parseFloat(mul);
    if (!isFinite(m) || m <= 0) m = 1.0;
    this._cam.speedMul = m;
    this._cam.speed = this._cam.baseSpeed * m;
  };

  /* ─────────────── Brush-based collision (Q3/ET engine style) ────────────── */
  /* This implements the same tracing approach used in id-Software/Enemy-Territory:
     walk the BSP tree, test AABB against convex brush hulls using plane-distance
     tests.  This correctly handles clip brushes, playerclip, solid walls, floors,
     and prevents walking through invisible collision geometry or walking in the air. */

  BSPViewer.prototype._buildCollision = function (bsp) {
    /* Store BSP data for brush tracing.  Coordinates stay in Q3 space
       (X=right, Y=forward, Z=up) inside the trace; we convert to/from
       GL space (X=right, Y=up, Z=-forward) at the call boundary. */
    this._cm = null;
    if (!bsp.planes || !bsp.nodes || !bsp.leafs || !bsp.brushes || !bsp.brushsides || !bsp.leafbrushes) return;

    /* Pre-compute which brushes are solid for the player. */
    var solidBrushes = new Uint8Array(bsp.brushes.length);
    for (var i = 0; i < bsp.brushes.length; i++) {
      var br = bsp.brushes[i];
      if (br.texture >= 0 && br.texture < bsp.textures.length) {
        var ct = bsp.textures[br.texture].contents;
        if (ct & (CONTENTS_SOLID | CONTENTS_PLAYERCLIP)) {
          solidBrushes[i] = 1;
        }
      }
    }

    /* ─── Build patch collision (Q3/ET style: tessellate patches into triangles,
       generate surface plane + border planes per facet) ─── */
    var patchSurfaces = new Array(bsp.faces.length); // null for non-patches
    for (var fi = 0; fi < bsp.faces.length; fi++) {
      patchSurfaces[fi] = null;
      var face = bsp.faces[fi];
      if (face.type !== FACE_PATCH) continue;
      /* Only collide with solid/playerclip patches. */
      if (face.texture >= 0 && face.texture < bsp.textures.length) {
        var fct = bsp.textures[face.texture].contents;
        if (!(fct & (CONTENTS_SOLID | CONTENTS_PLAYERCLIP))) continue;
      }
      var W = face.size[0];
      var H = face.size[1];
      if (W < 3 || H < 3) continue;
      var pc = cmGeneratePatchCollide(bsp, face, W, H);
      if (pc) patchSurfaces[fi] = pc;
    }

    this._cm = {
      planes: bsp.planes,
      nodes: bsp.nodes,
      leafs: bsp.leafs,
      leaffaces: bsp.leaffaces || null,
      leafbrushes: bsp.leafbrushes,
      brushes: bsp.brushes,
      brushsides: bsp.brushsides,
      textures: bsp.textures,
      solidBrushes: solidBrushes,
      patchSurfaces: patchSurfaces,
      _checkCount: 0,
      _brushCheck: new Int32Array(bsp.brushes.length),
      _patchCheck: new Int32Array(bsp.faces.length)
    };
  };

  /* ─── Patch collision generation (Q3/ET cm_patch.c style) ─── */
  /* Tessellate a bezier patch into triangles.  For each triangle, store the
     surface plane and 3 border (edge) planes.  This is a simplified version of
     CM_GeneratePatchCollide – we skip bevel planes since the ground trace only
     needs the surface and edge containment to detect floor hits. */

  function cmGeneratePatchCollide(bsp, face, W, H) {
    /* Tessellate all 3×3 sub-patches to gather world-space Q3 triangles. */
    var tris = [];
    var SUBDIV = 3; /* lower than rendering; sufficient for collision */
    var SL = SUBDIV + 1;

    for (var py = 0; py < (H - 1) / 2; py++) {
      for (var px = 0; px < (W - 1) / 2; px++) {
        /* Extract 3×3 control points in Q3 space. */
        var ctrl = [];
        for (var cy = 0; cy < 3; cy++) {
          for (var cx = 0; cx < 3; cx++) {
            var vi = (py * 2 + cy) * W + (px * 2 + cx);
            var sv = bsp.verts[face.vertex + vi];
            if (sv) {
              ctrl.push(sv.pos); /* already in Q3 space */
            } else {
              ctrl.push([0, 0, 0]);
            }
          }
        }
        /* Tessellate in Q3 space (positions only). */
        var pts = [];
        for (var tj = 0; tj <= SUBDIV; tj++) {
          var t = tj / SUBDIV;
          var mt = 1 - t;
          /* Interpolate columns along t (vertical parameter) */
          var c0 = [0, 0, 0], c1 = [0, 0, 0], c2 = [0, 0, 0];
          for (var ax = 0; ax < 3; ax++) {
            c0[ax] = mt * mt * ctrl[0][ax] + 2 * mt * t * ctrl[3][ax] + t * t * ctrl[6][ax];
            c1[ax] = mt * mt * ctrl[1][ax] + 2 * mt * t * ctrl[4][ax] + t * t * ctrl[7][ax];
            c2[ax] = mt * mt * ctrl[2][ax] + 2 * mt * t * ctrl[5][ax] + t * t * ctrl[8][ax];
          }
          for (var si = 0; si <= SUBDIV; si++) {
            var s = si / SUBDIV;
            var ms = 1 - s;
            var p = [0, 0, 0];
            for (var ax2 = 0; ax2 < 3; ax2++) {
              p[ax2] = ms * ms * c0[ax2] + 2 * ms * s * c1[ax2] + s * s * c2[ax2];
            }
            pts.push(p);
          }
        }
        /* Generate triangle indices and store tris. */
        for (var row = 0; row < SUBDIV; row++) {
          for (var col = 0; col < SUBDIV; col++) {
            var i0 = row * SL + col;
            var i1 = i0 + 1;
            var i2 = i0 + SL;
            var i3 = i2 + 1;
            tris.push([pts[i0], pts[i1], pts[i2]]);
            tris.push([pts[i1], pts[i3], pts[i2]]);
          }
        }
      }
    }

    if (tris.length === 0) return null;

    /* Build facets: for each triangle compute surface plane + 3 edge border planes.
       A border plane is perpendicular to the surface and passes through the edge,
       pointing inward. This matches Q3/ET CM_PatchCollideFromGrid logic. */
    var facets = [];
    var PLANE_EPSILON = 0.001;

    for (var ti = 0; ti < tris.length; ti++) {
      var tri = tris[ti];
      var p0 = tri[0], p1 = tri[1], p2 = tri[2];
      /* Surface plane from cross product. */
      var e1 = [p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]];
      var e2 = [p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]];
      var nx = e1[1] * e2[2] - e1[2] * e2[1];
      var ny = e1[2] * e2[0] - e1[0] * e2[2];
      var nz = e1[0] * e2[1] - e1[1] * e2[0];
      var len = Math.sqrt(nx * nx + ny * ny + nz * nz);
      if (len < PLANE_EPSILON) continue; /* degenerate */
      nx /= len; ny /= len; nz /= len;
      var dist = nx * p0[0] + ny * p0[1] + nz * p0[2];

      /* 3 border planes: for each edge (p_a, p_b), the border plane normal is
         cross(surfaceNormal, edgeDir) pointing inward. */
      var edges = [[p0, p1], [p1, p2], [p2, p0]];
      var borders = [];
      var valid = true;
      for (var ei = 0; ei < 3; ei++) {
        var ea = edges[ei][0], eb = edges[ei][1];
        var edx = eb[0] - ea[0], edy = eb[1] - ea[1], edz = eb[2] - ea[2];
        /* border normal = cross(surfaceNormal, edgeDir) */
        var bnx = ny * edz - nz * edy;
        var bny = nz * edx - nx * edz;
        var bnz = nx * edy - ny * edx;
        var blen = Math.sqrt(bnx * bnx + bny * bny + bnz * bnz);
        if (blen < PLANE_EPSILON) { valid = false; break; }
        bnx /= blen; bny /= blen; bnz /= blen;
        var bdist = bnx * ea[0] + bny * ea[1] + bnz * ea[2];
        borders.push([bnx, bny, bnz, bdist]);
      }
      if (!valid) continue;

      facets.push({
        plane: [nx, ny, nz, dist],
        borders: borders /* 3 border planes, each [nx,ny,nz,d] pointing inward */
      });
    }

    if (facets.length === 0) return null;
    return { facets: facets };
  }

  /* Trace a box through a patch's collision facets (Q3/ET CM_TraceThroughPatchCollide).
     Uses the enter/leave fraction interval approach. All coordinates in Q3 space. */
  function cmTraceThroughPatch(tw, pc) {
    var EPSILON = 0.125; /* SURFACE_CLIP_EPSILON */
    var extX = tw.extents[0], extY = tw.extents[1], extZ = tw.extents[2];

    for (var fi = 0; fi < pc.facets.length; fi++) {
      var facet = pc.facets[fi];
      var enterFrac = -1.0;
      var leaveFrac = 1.0;
      var hitPlane = null;

      /* ── Surface plane (offset by box extents) ── */
      var sp = facet.plane;
      var spOff = extX * Math.abs(sp[0]) + extY * Math.abs(sp[1]) + extZ * Math.abs(sp[2]);
      var d1 = tw.start[0] * sp[0] + tw.start[1] * sp[1] + tw.start[2] * sp[2] - sp[3] - spOff;
      var d2 = tw.end[0] * sp[0] + tw.end[1] * sp[1] + tw.end[2] * sp[2] - sp[3] - spOff;

      /* Completely in front (outside) → skip facet. */
      if (d1 > 0 && (d2 >= EPSILON || d2 >= d1)) continue;
      if (!(d1 <= 0 && d2 <= 0)) {
        if (d1 > d2) {
          var f = (d1 - EPSILON) / (d1 - d2);
          if (f < 0) f = 0;
          if (f > enterFrac) { enterFrac = f; hitPlane = sp; }
        } else {
          var f2 = (d1 + EPSILON) / (d1 - d2);
          if (f2 > 1) f2 = 1;
          if (f2 < leaveFrac) leaveFrac = f2;
        }
      }

      /* ── Border planes ── */
      var borders = facet.borders;
      var broke = false;
      for (var bi = 0; bi < borders.length; bi++) {
        var bp = borders[bi];
        /* Border planes point inward; expand by box extents (absolute offset). */
        var bOff = extX * Math.abs(bp[0]) + extY * Math.abs(bp[1]) + extZ * Math.abs(bp[2]);
        var bd1 = tw.start[0] * bp[0] + tw.start[1] * bp[1] + tw.start[2] * bp[2] - bp[3] + bOff;
        var bd2 = tw.end[0] * bp[0] + tw.end[1] * bp[1] + tw.end[2] * bp[2] - bp[3] + bOff;

        if (bd1 > 0 && (bd2 >= EPSILON || bd2 >= bd1)) { broke = true; break; }
        if (bd1 <= 0 && bd2 <= 0) continue;
        if (bd1 > bd2) {
          var bf = (bd1 - EPSILON) / (bd1 - bd2);
          if (bf < 0) bf = 0;
          if (bf > enterFrac) { enterFrac = bf; hitPlane = bp; }
        } else {
          var bf2 = (bd1 + EPSILON) / (bd1 - bd2);
          if (bf2 > 1) bf2 = 1;
          if (bf2 < leaveFrac) leaveFrac = bf2;
        }
      }
      if (broke) continue;

      /* Record hit. */
      if (enterFrac < leaveFrac && enterFrac >= 0 && enterFrac < tw.fraction) {
        if (enterFrac < 0) enterFrac = 0;
        tw.fraction = enterFrac;
        /* Use the surface plane normal for the hit normal (not the border).
           This gives proper floor/wall normals for ground detection. */
        tw.normal = [sp[0], sp[1], sp[2]];
      }
    }
  }


  /* Trace an axis-aligned bounding box from start to end through the BSP.
     mins/maxs define the box extents (relative to origin).
     Returns { fraction: 0..1, normal: [nx,ny,nz], allsolid: bool, startsolid: bool }
     All coordinates in Q3 space. */
  function cmBoxTrace(cm, start, end, mins, maxs) {
    var tw = {
      start: start,
      end: end,
      mins: mins,
      maxs: maxs,
      /* Expanded extents (absolute half-widths for plane offset). */
      extents: [
        Math.max(Math.abs(mins[0]), Math.abs(maxs[0])),
        Math.max(Math.abs(mins[1]), Math.abs(maxs[1])),
        Math.max(Math.abs(mins[2]), Math.abs(maxs[2]))
      ],
      fraction: 1.0,
      normal: [0, 0, 0],
      allsolid: false,
      startsolid: false,
      brushCheck: cm._brushCheck,
      patchCheck: cm._patchCheck,
      checkCount: ++cm._checkCount
    };
    /* Overflow guard on the dedup stamp counter. */
    if (tw.checkCount >= 0x7fffffff) {
      for (var zz = 0; zz < cm._brushCheck.length; zz++) cm._brushCheck[zz] = 0;
      if (cm._patchCheck) { for (var pp = 0; pp < cm._patchCheck.length; pp++) cm._patchCheck[pp] = 0; }
      cm._checkCount = 1;
      tw.checkCount = 1;
    }
    cmTraceThroughTree(cm, tw, 0, 0.0, 1.0, start, end);
    return { fraction: tw.fraction, normal: tw.normal, allsolid: tw.allsolid, startsolid: tw.startsolid };
  }

  function cmTraceThroughTree(cm, tw, nodeIdx, startFrac, endFrac, start, end) {
    if (tw.fraction <= startFrac) return; /* already hit something closer */

    if (nodeIdx < 0) {
      /* Leaf node */
      var leaf = cm.leafs[-(nodeIdx + 1)];
      if (!leaf) return;
      for (var i = 0; i < leaf.n_leafbrushes; i++) {
        var brushIdx = cm.leafbrushes[leaf.leafbrush + i];
        if (!cm.solidBrushes[brushIdx]) continue;
        if (tw.brushCheck[brushIdx] === tw.checkCount) continue;
        tw.brushCheck[brushIdx] = tw.checkCount;
        cmTraceThroughBrush(cm, tw, cm.brushes[brushIdx]);
        if (tw.allsolid) return;
      }
      /* Check patch surfaces in this leaf (Q3/ET CM_TraceThroughLeaf). */
      if (cm.leaffaces && cm.patchSurfaces) {
        for (var pi = 0; pi < leaf.n_leaffaces; pi++) {
          var faceIdx = cm.leaffaces[leaf.leafface + pi];
          if (faceIdx === undefined) continue;
          var pc = cm.patchSurfaces[faceIdx];
          if (!pc) continue;
          if (tw.patchCheck[faceIdx] === tw.checkCount) continue;
          tw.patchCheck[faceIdx] = tw.checkCount;
          cmTraceThroughPatch(tw, pc);
        }
      }
      return;
    }

    var node = cm.nodes[nodeIdx];
    if (!node) return;
    var plane = cm.planes[node.plane];
    if (!plane) return;

    var nx = plane.normal[0], ny = plane.normal[1], nz = plane.normal[2];
    var dist = plane.dist;

    /* Offset plane distance by box extents along the plane normal. */
    var offset = tw.extents[0] * Math.abs(nx) +
                 tw.extents[1] * Math.abs(ny) +
                 tw.extents[2] * Math.abs(nz);

    var sd = start[0]*nx + start[1]*ny + start[2]*nz - dist;
    var ed = end[0]*nx + end[1]*ny + end[2]*nz - dist;

    /* Both sides? */
    if (sd >= offset && ed >= offset) {
      cmTraceThroughTree(cm, tw, node.children[0], startFrac, endFrac, start, end);
      return;
    }
    if (sd < -offset && ed < -offset) {
      cmTraceThroughTree(cm, tw, node.children[1], startFrac, endFrac, start, end);
      return;
    }

    /* The trace crosses the splitting plane – recurse into both children. */
    var side, frac1, frac2;
    var OFFSET = 0.03125; /* 1/32, same as Q3 SURFACE_CLIP_EPSILON */

    if (sd < ed) {
      var invDen = 1.0 / (sd - ed);
      frac1 = (sd - offset + OFFSET) * invDen;
      frac2 = (sd + offset + OFFSET) * invDen;
      side = 1;
    } else if (sd > ed) {
      var invDen2 = 1.0 / (sd - ed);
      frac1 = (sd + offset + OFFSET) * invDen2;
      frac2 = (sd - offset - OFFSET) * invDen2;
      side = 0;
    } else {
      frac1 = 1.0;
      frac2 = 0.0;
      side = 0;
    }

    if (frac1 < 0) frac1 = 0; else if (frac1 > 1) frac1 = 1;
    if (frac2 < 0) frac2 = 0; else if (frac2 > 1) frac2 = 1;

    var mid1Frac = startFrac + (endFrac - startFrac) * frac1;
    var mid1 = [
      start[0] + (end[0] - start[0]) * frac1,
      start[1] + (end[1] - start[1]) * frac1,
      start[2] + (end[2] - start[2]) * frac1
    ];
    cmTraceThroughTree(cm, tw, node.children[side], startFrac, mid1Frac, start, mid1);

    var mid2Frac = startFrac + (endFrac - startFrac) * frac2;
    var mid2 = [
      start[0] + (end[0] - start[0]) * frac2,
      start[1] + (end[1] - start[1]) * frac2,
      start[2] + (end[2] - start[2]) * frac2
    ];
    cmTraceThroughTree(cm, tw, node.children[1 - side], mid2Frac, endFrac, mid2, end);
  }

  function cmTraceThroughBrush(cm, tw, brush) {
    if (brush.n_brushsides <= 0) return;

    var enterFrac = -1.0;
    var leaveFrac = 1.0;
    var startOut = false;
    var getOut = false;
    var hitNormal = null;

    for (var i = 0; i < brush.n_brushsides; i++) {
      var side = cm.brushsides[brush.brushside + i];
      if (!side) continue;
      var plane = cm.planes[side.plane];
      if (!plane) continue;
      var nx = plane.normal[0], ny = plane.normal[1], nz = plane.normal[2];

      /* Adjust plane distance for the AABB. When the normal points in a
         direction, the closest box corner is the one in the negative direction
         from the center. This is the standard Quake "offset" approach. */
      var d = plane.dist;
      d -= (tw.mins[0] * Math.max(0, nx) + tw.maxs[0] * Math.min(0, nx));
      d -= (tw.mins[1] * Math.max(0, ny) + tw.maxs[1] * Math.min(0, ny));
      d -= (tw.mins[2] * Math.max(0, nz) + tw.maxs[2] * Math.min(0, nz));
      /* This can be simplified: for symmetric boxes (mins = -maxs):
         d += extents[0]*|nx| + extents[1]*|ny| + extents[2]*|nz|
         But we use the general form to support asymmetric player boxes. */

      var sd = tw.start[0]*nx + tw.start[1]*ny + tw.start[2]*nz - d;
      var ed = tw.end[0]*nx + tw.end[1]*ny + tw.end[2]*nz - d;

      if (sd > 0) startOut = true;
      if (ed > 0) getOut = true;

      /* If completely in front of this plane, we're outside the brush. */
      if (sd > 0 && ed >= sd) return;
      /* If completely behind this plane, check next plane. */
      if (sd <= 0 && ed <= 0) continue;

      if (sd > ed) {
        /* Entering the brush */
        var f = (sd - 0.03125) / (sd - ed);
        if (f < 0) f = 0;
        if (f > enterFrac) {
          enterFrac = f;
          hitNormal = [nx, ny, nz];
        }
      } else {
        /* Leaving the brush */
        var f2 = (sd + 0.03125) / (sd - ed);
        if (f2 > 1) f2 = 1;
        if (f2 < leaveFrac) leaveFrac = f2;
      }
    }

    if (!startOut) {
      /* Original point was inside the brush. */
      tw.startsolid = true;
      if (!getOut) tw.allsolid = true;
      return;
    }

    if (enterFrac < leaveFrac) {
      if (enterFrac > -1 && enterFrac < tw.fraction) {
        if (enterFrac < 0) enterFrac = 0;
        tw.fraction = enterFrac;
        tw.normal = hitNormal;
      }
    }
  }

  /* ─── High-level trace helpers (convert GL ↔ Q3 at boundary) ─── */

  /* GL→Q3: (x, y, z) → (x, -z, y) */
  function glToQ3(v) { return [v[0], -v[2], v[1]]; }
  /* Q3→GL: (x, y, z) → (x, z, -y) */
  function q3ToGLVec(v) { return [v[0], v[2], -v[1]]; }

  /* Trace the player box from glStart to glEnd (both in GL space).
     playerMins/playerMaxs in Q3 space.  Returns result with GL normal. */
  BSPViewer.prototype._traceBox = function (glStart, glEnd, q3Mins, q3Maxs) {
    if (!this._cm) return { fraction: 1.0, normal: [0,0,0], allsolid: false, startsolid: false };
    var s = glToQ3(glStart);
    var e = glToQ3(glEnd);
    var r = cmBoxTrace(this._cm, s, e, q3Mins, q3Maxs);
    /* Convert the hit normal from Q3 to GL. */
    if (r.normal) r.normal = q3ToGLVec(r.normal);
    return r;
  };


  BSPViewer.prototype._moveCamera = function (dt) {
    var cam = this._cam;
    var k = this._keys;
    /* Sprint: Shift doubles speed (like Q3/ET engine) */
    var sprint = (k['ShiftLeft'] || k['ShiftRight']) ? 2.0 : (k['ControlLeft'] || k['ControlRight']) ? 0.25 : 1.0;
    /* Fly mode is 2x faster than FPS mode */
    var flyMul = this._fps ? 1.0 : 2.0;
    var s   = cam.speed * dt * sprint * flyMul;
    var cy  = Math.cos(cam.yaw),  sy = Math.sin(cam.yaw);
    var cp  = Math.cos(cam.pitch), sp = Math.sin(cam.pitch);
    /* Forward direction in WebGL space */
    var fx, fy, fz;
    if (this._fps) {
      /* In FPS mode WASD only moves on the horizontal plane (ignore pitch),
         so looking up/down doesn't slow or lift you. */
      fx =  sy;  fy = 0;  fz = -cy;
    } else {
      fx =  sy * cp;  fy = sp;  fz = -cy * cp;
    }
    /* Right direction (always horizontal) */
    var rx =  cy,                  rz =  sy;

    var k = this._keys;
    var mvX = 0, mvY = 0, mvZ = 0;
    if (k['KeyW'] || k['ArrowUp'])    { mvX += fx; mvY += fy; mvZ += fz; }
    if (k['KeyS'] || k['ArrowDown'])  { mvX -= fx; mvY -= fy; mvZ -= fz; }
    if (k['KeyA'] || k['ArrowLeft'])  { mvX -= rx;            mvZ -= rz; }
    if (k['KeyD'] || k['ArrowRight']) { mvX += rx;            mvZ += rz; }

    if (this._fps) {
      /* ──── Q3/ET-style player movement with brush collision ──── */
      /* Player bounding box in Q3 coordinates (same as ET: -18..18 x/y, -24..32 z)
         Standing height = 56 units (eye at +32 above feet center, box is -24 to +32). */
      var PM_MINS = [-this._fpsRadius, -this._fpsRadius, -24];
      var PM_MAXS = [ this._fpsRadius,  this._fpsRadius, 32];

      /* ─── Horizontal slide-move (like PM_SlideMove in ET) ─── */
      var wishVelX = mvX * s;
      var wishVelZ = mvZ * s;
      var prevY = cam.pos[1]; /* remember Y before movement for step smoothing */

      if (wishVelX !== 0 || wishVelZ !== 0) {
        /* The camera position in GL space represents the player's eye.
           The box origin (for tracing) is at feet + 24 units up (box center).
           In Q3: box origin is where (0,0,0) of the box mins/maxs sits.
           PM_MINS[2]=-24 means the box goes 24 units below origin.
           So box origin = feet + 24 in Q3-Z = GL-Y: eyePos - eyeH + 24 */
        var boxOriginGL = [cam.pos[0], cam.pos[1] - this._fpsEyeH + 24, cam.pos[2]];

        /* Q3/ET fix for brush transitions: when on ground, project the wish
           velocity onto the ground plane so movement follows the slope surface.
           This prevents falling through brush seams when walking uphill, because
           the trace follows the actual surface instead of moving flat and missing
           the next brush at the seam. (See PM_ClipVelocity in bg_slidemove.c) */
        var slideVelX = wishVelX, slideVelY = 0, slideVelZ = wishVelZ;
        if (this._fpsOnGround) {
          var gn = this._fpsGroundNormal;
          /* Only project if ground is not perfectly flat (optimization).
             Also cap the velocity boost on steep slopes to prevent excessive speed. */
          if (gn[0] !== 0 || gn[1] !== 1 || gn[2] !== 0) {
            var OVERCLIP = 1.001;
            var backoff = slideVelX * gn[0] + slideVelY * gn[1] + slideVelZ * gn[2];
            backoff *= OVERCLIP;
            var projX = slideVelX - gn[0] * backoff;
            var projY = slideVelY - gn[1] * backoff;
            var projZ = slideVelZ - gn[2] * backoff;
            /* Q3/ET PM_ClipVelocity: normalize the projected velocity to prevent
               speed amplification on steep slopes (like bg_slidemove.c does). */
            var origSpeed = Math.sqrt(slideVelX * slideVelX + slideVelZ * slideVelZ);
            var projSpeed = Math.sqrt(projX * projX + projY * projY + projZ * projZ);
            if (projSpeed > origSpeed * 1.2 && projSpeed > 0) {
              /* Clamp projected speed to avoid launch effects on steep transitions */
              var scale = (origSpeed * 1.2) / projSpeed;
              projX *= scale;
              projY *= scale;
              projZ *= scale;
            }
            slideVelX = projX;
            slideVelY = projY;
            slideVelZ = projZ;
          }
        }

        var endGL = [boxOriginGL[0] + slideVelX, boxOriginGL[1] + slideVelY, boxOriginGL[2] + slideVelZ];

        /* PM_StepSlideMove: first try flat, then if blocked try stepping up. */
        var result = this._slideMove(boxOriginGL, endGL, PM_MINS, PM_MAXS);
        var flatDist = (result[0]-boxOriginGL[0])*(result[0]-boxOriginGL[0]) +
                       (result[2]-boxOriginGL[2])*(result[2]-boxOriginGL[2]);
        var totalWish = wishVelX*wishVelX + wishVelZ*wishVelZ;

        /* If we didn't make full progress and we're on the ground, try step-up. */
        if (flatDist < totalWish * 0.99 && this._fpsOnGround) {
          var stepH = this._fpsStepUp;
          /* Trace up */
          var upStart = [boxOriginGL[0], boxOriginGL[1], boxOriginGL[2]];
          var upEnd   = [boxOriginGL[0], boxOriginGL[1] + stepH, boxOriginGL[2]];
          var upTrace = this._traceBox(upStart, upEnd, PM_MINS, PM_MAXS);
          var steppedY = boxOriginGL[1] + (stepH * upTrace.fraction);

          /* Now try horizontal move from the stepped-up position. */
          var stepStart = [boxOriginGL[0], steppedY, boxOriginGL[2]];
          var stepEnd   = [stepStart[0] + wishVelX, steppedY, stepStart[2] + wishVelZ];
          var stepResult = this._slideMove(stepStart, stepEnd, PM_MINS, PM_MAXS);

          /* Trace back down to find the floor after the step. */
          var downStart = [stepResult[0], stepResult[1], stepResult[2]];
          var downEnd   = [stepResult[0], stepResult[1] - stepH - 1, stepResult[2]];
          var downTrace = this._traceBox(downStart, downEnd, PM_MINS, PM_MAXS);

          if (downTrace.fraction < 1.0 && downTrace.normal[1] > 0.7) {
            var stepMoveDist = (stepResult[0]-boxOriginGL[0])*(stepResult[0]-boxOriginGL[0]) +
                               (stepResult[2]-boxOriginGL[2])*(stepResult[2]-boxOriginGL[2]);
            if (stepMoveDist > flatDist) {
              /* The step-up got us further – use it. */
              var finalY = stepResult[1] + (downEnd[1] - stepResult[1]) * downTrace.fraction;
              cam.pos[0] = stepResult[0];
              cam.pos[1] = finalY + this._fpsEyeH - 24;
              cam.pos[2] = stepResult[2];
              result = null; /* skip flat result */
            }
          }
        }

        if (result) {
          cam.pos[0] = result[0];
          /* When on ground with slope-projected velocity, the slide move has a
             Y component – apply it so the player follows the slope upward.
             The ground trace below will still snap to the actual floor. */
          if (this._fpsOnGround) {
            cam.pos[1] = result[1] + this._fpsEyeH - 24;
          }
          cam.pos[2] = result[2];
        }
      }

      /* ─── Jump logic ─── */
      var onGround = this._fpsOnGround;
      if (onGround) {
        this._fpsJumpsLeft = this._fpsMaxJumps;
      }
      if (k['Space'] && !this._fpsJumpHeld && this._fpsJumpsLeft > 0) {
        this._fpsVY = this._fpsJumpV;
        this._fpsJumpsLeft--;
        this._fpsJumpHeld = true;
        onGround = false;
      }
      if (!k['Space']) this._fpsJumpHeld = false;

      /* ─── Gravity + vertical trace ─── */
      if (!onGround) {
        this._fpsVY -= this._fpsGravity * dt;
      }
      var vertDelta = this._fpsVY * dt;
      if (vertDelta !== 0 || !onGround) {
        /* Trace the player box vertically. */
        var curBoxY = cam.pos[1] - this._fpsEyeH + 24;
        var startV = [cam.pos[0], curBoxY, cam.pos[2]];
        var endV   = [cam.pos[0], curBoxY + vertDelta, cam.pos[2]];
        var vTrace = this._traceBox(startV, endV, PM_MINS, PM_MAXS);

        if (vTrace.fraction < 1.0) {
          /* Hit something. */
          var hitY = curBoxY + vertDelta * vTrace.fraction;
          cam.pos[1] = hitY + this._fpsEyeH - 24;
          /* If the normal points upward (floor), we landed. */
          if (vTrace.normal[1] > 0.7) {
            this._fpsVY = 0;
            this._fpsOnGround = true;
            this._fpsGroundNormal = vTrace.normal;
          } else if (vTrace.normal[1] < -0.7) {
            /* Hit ceiling */
            this._fpsVY = 0;
          } else {
            /* Hit a steep wall/slope while moving vertically.
               Q3/ET: clip the velocity against the surface to allow sliding
               along steep slopes instead of getting stuck. */
            var vn = vTrace.normal;
            var velDot = this._fpsVY * vn[1]; /* vertical vel dot normal (GL Y) */
            if (velDot < 0) {
              /* Remove the component going into the surface */
              this._fpsVY -= vn[1] * velDot * 1.001;
            } else {
              this._fpsVY = 0;
            }
          }
        } else {
          cam.pos[1] += vertDelta;
          this._fpsOnGround = false;
        }
      }

      /* ─── Ground trace (snap to floor / step-down for stairs & uneven terrain) ─── */
      /* Use a larger probe distance to stay grounded on slopes and uneven surfaces.
         Q3/ET probes 0.25 units normally; we use a larger value to account for
         frame-rate-dependent movement over bumps. When on ground and not jumping up,
         probe down by stepUp + 2 to handle both small bumps and full stair steps. */
      if (this._fpsVY <= 0) {
        var gBoxY = cam.pos[1] - this._fpsEyeH + 24;
        var gStart = [cam.pos[0], gBoxY, cam.pos[2]];
        /* Probe distance: if we were on ground, check a full step down.
           Otherwise just a small probe to detect landing. */
        var probeDown = this._fpsOnGround ? (this._fpsStepUp + 4) : 4;
        var gEnd   = [cam.pos[0], gBoxY - probeDown, cam.pos[2]];
        var gTrace = this._traceBox(gStart, gEnd, PM_MINS, PM_MAXS);
        if (gTrace.fraction < 1.0 && gTrace.normal[1] > 0.7) {
          /* On the ground */
          var gHitY = gBoxY + (gEnd[1] - gBoxY) * gTrace.fraction;
          cam.pos[1] = gHitY + this._fpsEyeH - 24;
          this._fpsVY = 0;
          this._fpsOnGround = true;
          this._fpsGroundNormal = gTrace.normal;
        } else if (gTrace.fraction < 1.0 && gTrace.normal[1] > 0.3 && this._fpsOnGround) {
          /* Hit a moderately steep surface at a brush transition.
             Snap to it but mark as sliding so gravity still pulls down.
             This prevents "falling out" at steep brush seams while still
             allowing the player to slide off truly vertical walls. */
          var gHitY2 = gBoxY + (gEnd[1] - gBoxY) * gTrace.fraction;
          cam.pos[1] = gHitY2 + this._fpsEyeH - 24;
          this._fpsVY = 0;
          this._fpsOnGround = true;
          this._fpsGroundNormal = gTrace.normal;
        } else if (this._fpsOnGround) {
          /* Was on ground but couldn't find floor – fall. */
          this._fpsOnGround = false;
        }
      }

      /* ─── Step smoothing (Q3-style camera bob for stairs) ─── */
      /* When the actual player position changes height (due to step-up/down or
         ground snapping), add a visual offset that decays smoothly so the camera
         doesn't jerk up/down on every stair step. */
      var stepDelta = prevY - cam.pos[1]; /* positive = stepped down, negative = stepped up */
      if (stepDelta !== 0 && this._fpsOnGround && this._fpsVY === 0) {
        /* Only smooth if the change is within stair range (not a big fall/jump). */
        var absStep = stepDelta < 0 ? -stepDelta : stepDelta;
        if (absStep > 0.5 && absStep <= this._fpsStepUp + 4) {
          this._fpsStepOffset += stepDelta;
          /* Clamp to reasonable range. */
          if (this._fpsStepOffset > this._fpsStepUp) this._fpsStepOffset = this._fpsStepUp;
          if (this._fpsStepOffset < -this._fpsStepUp) this._fpsStepOffset = -this._fpsStepUp;
          this._fpsStepTime = 0.12; /* smooth over 120ms (Q3 uses ~100-200ms) */
        }
      }
      /* Decay the step offset toward zero. */
      if (this._fpsStepOffset !== 0) {
        if (this._fpsStepTime > 0) {
          this._fpsStepTime -= dt;
          if (this._fpsStepTime <= 0) {
            this._fpsStepOffset = 0;
            this._fpsStepTime = 0;
          } else {
            /* Linear decay toward zero. */
            var decay = dt / (this._fpsStepTime + dt);
            this._fpsStepOffset *= (1.0 - decay);
            if (this._fpsStepOffset > -0.1 && this._fpsStepOffset < 0.1) {
              this._fpsStepOffset = 0;
            }
          }
        } else {
          this._fpsStepOffset = 0;
        }
      }

    } else {
      cam.pos[0] += mvX * s; cam.pos[1] += mvY * s; cam.pos[2] += mvZ * s;
      if (k['Space'] || k['KeyE'])  { cam.pos[1] += s; }
      if (k['KeyC'] || k['KeyQ'])   { cam.pos[1] -= s; }
    }
  };

  /* Slide-move: attempt to move from startGL to endGL, sliding along walls.
     Implements the same iterative clip approach as PM_SlideMove in Q3/ET.
     Returns the final GL position [x, y, z]. */
  BSPViewer.prototype._slideMove = function (startGL, endGL, mins, maxs) {
    var OVERCLIP = 1.001; /* Q3 overbounce factor */
    var MAX_CLIP_PLANES = 5;
    var numBumps = 4; /* max iterations */
    var pos = [startGL[0], startGL[1], startGL[2]];
    /* vel = full-frame velocity; we track time_left like Q3 */
    var vel = [endGL[0] - startGL[0], endGL[1] - startGL[1], endGL[2] - startGL[2]];
    var planes = [];
    var time_left = 1.0;
    var i, j, d;

    /* Q3/ET: If the player is grounded, add the ground plane as the first
       clip plane. This ensures we never slide into the floor and properly
       redirect along steep brush transitions. (See PM_SlideMove in
       bg_slidemove.c: "if they are on ground, add a plane to clip against") */
    if (this._fpsOnGround) {
      planes.push([this._fpsGroundNormal[0], this._fpsGroundNormal[1], this._fpsGroundNormal[2]]);
    }

    for (var bump = 0; bump < numBumps; bump++) {
      if (vel[0] === 0 && vel[1] === 0 && vel[2] === 0) break;

      var dest = [pos[0] + vel[0] * time_left,
                  pos[1] + vel[1] * time_left,
                  pos[2] + vel[2] * time_left];
      var tr = this._traceBox(pos, dest, mins, maxs);

      if (tr.allsolid) {
        /* Stuck in solid – try nudging out in small increments (Q3/ET
           CM_SnapVector style recovery). This prevents getting permanently
           stuck at steep brush seams. */
        var nudgeOffsets = [
          [0.125, 0, 0], [-0.125, 0, 0],
          [0, 0.125, 0], [0, -0.125, 0],
          [0, 0, 0.125], [0, 0, -0.125],
          [0, 0.25, 0]   /* try up a bit more */
        ];
        var escaped = false;
        for (var ni = 0; ni < nudgeOffsets.length; ni++) {
          var nudged = [pos[0] + nudgeOffsets[ni][0],
                        pos[1] + nudgeOffsets[ni][1],
                        pos[2] + nudgeOffsets[ni][2]];
          var nudgeTrace = this._traceBox(nudged, nudged, mins, maxs);
          if (!nudgeTrace.allsolid && !nudgeTrace.startsolid) {
            pos = nudged;
            escaped = true;
            break;
          }
        }
        if (!escaped) {
          /* Truly stuck – zero velocity and stop. */
          vel[0] = vel[1] = vel[2] = 0;
        }
        return pos;
      }

      if (tr.fraction > 0) {
        /* Move to the contact point. */
        pos[0] += (dest[0] - pos[0]) * tr.fraction;
        pos[1] += (dest[1] - pos[1]) * tr.fraction;
        pos[2] += (dest[2] - pos[2]) * tr.fraction;
      }

      if (tr.fraction >= 1.0) break; /* made it all the way */

      time_left -= time_left * tr.fraction;

      /* Store the hit plane normal. */
      if (planes.length < MAX_CLIP_PLANES) {
        planes.push(tr.normal);
      }

      /* Clip velocity against all collected planes (Q3 PM_SlideMove approach).
         Try each plane as primary clip; if the result doesn't go into any other
         plane we found a valid slide direction. */
      var clipVel = [0, 0, 0];
      var found = false;
      for (i = 0; i < planes.length; i++) {
        /* PM_ClipVelocity: remove component into plane with overbounce. */
        var backoff = vel[0]*planes[i][0] + vel[1]*planes[i][1] + vel[2]*planes[i][2];
        if (backoff < 0) {
          backoff *= OVERCLIP;
        } else {
          backoff /= OVERCLIP;
        }
        clipVel[0] = vel[0] - planes[i][0] * backoff;
        clipVel[1] = vel[1] - planes[i][1] * backoff;
        clipVel[2] = vel[2] - planes[i][2] * backoff;

        /* Verify clipped velocity doesn't go into any other plane. */
        var valid = true;
        for (j = 0; j < planes.length; j++) {
          if (j === i) continue;
          if (clipVel[0]*planes[j][0] + clipVel[1]*planes[j][1] + clipVel[2]*planes[j][2] < -0.01) {
            valid = false;
            break;
          }
        }
        if (valid) {
          vel[0] = clipVel[0]; vel[1] = clipVel[1]; vel[2] = clipVel[2];
          found = true;
          break;
        }
      }

      if (!found) {
        /* Couldn't find a single-plane clip that works.
           If two planes, slide along the crease (cross product). */
        if (planes.length >= 2) {
          var n1 = planes[planes.length - 2];
          var n2 = planes[planes.length - 1];
          var cx = n1[1]*n2[2] - n1[2]*n2[1];
          var cy2 = n1[2]*n2[0] - n1[0]*n2[2];
          var cz = n1[0]*n2[1] - n1[1]*n2[0];
          var cl = Math.sqrt(cx*cx + cy2*cy2 + cz*cz);
          if (cl > 1e-6) {
            cx /= cl; cy2 /= cl; cz /= cl;
            d = vel[0]*cx + vel[1]*cy2 + vel[2]*cz;
            vel[0] = cx*d; vel[1] = cy2*d; vel[2] = cz*d;

            /* Verify crease slide doesn't go into any plane. */
            var creaseOk = true;
            for (j = 0; j < planes.length; j++) {
              if (vel[0]*planes[j][0] + vel[1]*planes[j][1] + vel[2]*planes[j][2] < -0.01) {
                creaseOk = false; break;
              }
            }
            if (!creaseOk) {
              vel[0] = vel[1] = vel[2] = 0;
              break;
            }
          } else {
            /* Planes are nearly parallel – stop. */
            vel[0] = vel[1] = vel[2] = 0;
            break;
          }
        } else {
          vel[0] = vel[1] = vel[2] = 0;
          break;
        }
      }
    }

    return pos;
  };

  BSPViewer.prototype._viewMatrix = function () {
    var cam = this._cam;
    var cy  = Math.cos(cam.yaw),  sy = Math.sin(cam.yaw);
    var cp  = Math.cos(cam.pitch), sp = Math.sin(cam.pitch);
    var fx  = sy*cp, fy = sp, fz = -cy*cp;
    /* Apply step-smoothing offset so stairs look smooth instead of instant snaps. */
    var eyeY = cam.pos[1] + this._fpsStepOffset;
    return matLookAt(
      [cam.pos[0], eyeY, cam.pos[2]],
      [cam.pos[0]+fx, eyeY+fy, cam.pos[2]+fz],
      [0, 1, 0]
    );
  };

  /* Try to fetch external lightmap TGAs sitting next to the BSP, at
     maps/<bspname>/lm_NNNN.tga. Many ET maps (especially newer ones) ship
     external lightmaps for higher resolution than the 128² ones embedded in
     the BSP. We probe lm_0000 first; if it 404s we assume there are none.
     Resolves to an array of { rgba, width, height } or [] when nothing was
     found. */
  BSPViewer.prototype._tryLoadExternalLightmaps = function (bspName, expectedCount) {
    if (!bspName) return Promise.resolve([]);
    var self = this;
    /* Cap how many we'll attempt to fetch so a malformed manifest can't
       trigger thousands of requests. expectedCount comes from the embedded
       lightmap count and is usually 1–256. */
    var maxN = Math.max(1, Math.min(expectedCount || 256, 1024));
    var bases = ['/api/texture?map=' + encodeURIComponent(bspName) +
      '&name=maps/' + encodeURIComponent(bspName) + '/'];

    function pad4(n) { var s = String(n); while (s.length < 4) s = '0' + s; return s; }

    function fetchOne(base, idx) {
      var url = base + 'lm_' + pad4(idx) + '.tga';
      return fetch(url).then(function (r) {
        if (!r.ok) return null;
        return r.arrayBuffer().then(function (buf) {
          var dec = decodeTGA(buf);
          if (!dec) return null;
          return { rgba: dec.data, width: dec.width, height: dec.height };
        });
      }).catch(function () { return null; });
    }

    function fromBase(base) {
      return fetchOne(base, 0).then(function (first) {
        if (!first) return null;
        var out = [first];
        var stopped = false;
        var chain = Promise.resolve();
        for (var i = 1; i < maxN; i++) (function (idx) {
          chain = chain.then(function () {
            if (stopped) return null;
            return fetchOne(base, idx).then(function (lm) {
              if (lm) out.push(lm);
              else { /* stop on first miss to avoid huge spurious fetches */
                stopped = true;
              }
              return null;
            });
          });
        })(i);
        return chain.then(function () { return out; });
      });
    }

    function tryBase(i) {
      if (i >= bases.length) return Promise.resolve([]);
      return fromBase(bases[i]).then(function (lms) {
        if (lms && lms.length) return lms;
        return tryBase(i + 1);
      });
    }
    return tryBase(0);
  };

  /* Build a GL_TEXTURE_2D from already-decoded RGBA pixel data (used for
     external lightmap TGAs whose size is not necessarily LM_SIZE²). */
  BSPViewer.prototype._makeLMTextureRGBA = function (rgba, width, height) {
    var gl  = this.gl;
    /* Apply ET overbright shift in-place on a copy so the original buffer
       (which may still be referenced elsewhere) is unchanged. */
    var shifted = new Uint8Array(rgba.length);
    shifted.set(rgba);
    var shift = this._overbrightBits | 0;
    var minL  = this._minLight || 0;
    if (shift > 0 || minL > 0) {
      for (var i = 0; i < shifted.length; i += 4) {
        /* Enforce _minlight floor before overbright shift */
        if (minL > 0) {
          if (shifted[i]   < minL) shifted[i]   = minL;
          if (shifted[i+1] < minL) shifted[i+1] = minL;
          if (shifted[i+2] < minL) shifted[i+2] = minL;
        }
        if (shift > 0) colorShiftLightingBytes(shifted, i, shift);
      }
    }
    var tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, shifted);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return tex;
  };

  BSPViewer.prototype._makeLMTexture = function (rgbData) {
    var gl   = this.gl;
    var shift = this._overbrightBits | 0;
    var minL  = this._minLight || 0;
    var rgba = new Uint8Array(LM_SIZE * LM_SIZE * 4);
    for (var i = 0; i < LM_SIZE * LM_SIZE; i++) {
      var r = rgbData[i*3];
      var g = rgbData[i*3+1];
      var b = rgbData[i*3+2];
      /* Enforce _minlight floor before overbright shift */
      if (minL > 0) {
        if (r < minL) r = minL;
        if (g < minL) g = minL;
        if (b < minL) b = minL;
      }
      rgba[i*4]   = r;
      rgba[i*4+1] = g;
      rgba[i*4+2] = b;
      rgba[i*4+3] = 255;
      if (shift > 0) colorShiftLightingBytes(rgba, i*4, shift);
    }
    var tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, LM_SIZE, LM_SIZE, 0,
                  gl.RGBA, gl.UNSIGNED_BYTE, rgba);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return tex;
  };

  BSPViewer.prototype._isPow2 = function (v) {
    return (v & (v - 1)) === 0;
  };

  BSPViewer.prototype._makeImageTexture = function (img) {
    var gl = this.gl;
    var tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
    if (this._isPow2(img.width) && this._isPow2(img.height)) {
      gl.generateMipmap(gl.TEXTURE_2D);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    } else {
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    }
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    return {
      tex: tex,
      /* Match requested viewer behavior: only blend materials when pixels are actually non-opaque. */
      hasAlpha: imageHasAlpha(img)
    };
  };

  BSPViewer.prototype._makeRawTexture = function (rgba, width, height) {
    var gl  = this.gl;
    var tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, rgba);
    if (this._isPow2(width) && this._isPow2(height)) {
      gl.generateMipmap(gl.TEXTURE_2D);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    } else {
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    }
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    return {
      tex: tex,
      hasAlpha: hasNonOpaqueAlpha(rgba)
    };
  };

  /* Fetch the per-map texture manifest (only etmain + map folder).
   * The manifest maps normalised texture names (no extension, lowercase) to
   * actual URL paths.  Only textures from bsp/etmain/ and bsp/<map>/ are
   * included – no textures from unrelated maps can bleed in.
   * Re-fetches when the map name changes. */
  BSPViewer.prototype._loadManifest = function () {
    var mapName = (this._bspName || '').toLowerCase();
    /* Invalidate cached manifest if the map changed since last load. */
    if (this._manifest && this._manifestMap === mapName) {
      return Promise.resolve(this._manifest);
    }
    if (this._manifestPromise && this._manifestMap === mapName) {
      return this._manifestPromise;
    }
    var self = this;
    var url = LOCAL_BSP_MANIFEST + (mapName ? '?map=' + encodeURIComponent(mapName) : '');
    /* Append a cache-busting parameter so browsers always get a fresh manifest
       when textures may have been updated on the server. */
    url += (url.indexOf('?') !== -1 ? '&' : '?') + '_t=' + Date.now();
    this._manifestMap = mapName;
    this._manifestPromise = fetch(url)
      .then(function (r) { return r.ok ? r.json() : {}; })
      .catch(function () { return {}; })
      .then(function (m) {
        self._manifest = (m && typeof m === 'object') ? m : {};
        return self._manifest;
      });
    return this._manifestPromise;
  };

  BSPViewer.prototype._shaderToCandidates = function (name) {
    if (!name) return [];
    var cleaned = normalizeAssetPath(name);
    if (!cleaned) return [];
    var noExt = cleaned.replace(/\.(jpg|jpeg|png|tga|webp)$/i, '');
    var variants = [noExt];
    var prefixes = ['textures/', 'models/', 'gfx/'];
    for (var pi = 0; pi < prefixes.length; pi++) {
      var prefix = prefixes[pi];
      var lowerNoExt = noExt.toLowerCase();
      var lowerPrefix = prefix.toLowerCase();
      var stripped = (lowerNoExt.indexOf(lowerPrefix) === 0) ? noExt.slice(lowerPrefix.length) : noExt;
      if (stripped && stripped !== noExt) variants.push(stripped);
    }
    var out = [];
    var seen = {};

    /* Manifest lookup – the manifest is scoped to this map + etmain only,
     * so any match is guaranteed to be the correct texture. */
    if (this._manifest) {
      for (var mi = 0; mi < variants.length; mi++) {
        var mUrl = this._manifest[variants[mi].toLowerCase()];
        if (mUrl) { uniquePush(out, seen, mUrl); break; }
      }
    }

    // Local asset resolver searches this map plus TCE2/ET assets; never use a CDN.
    if (out.length) return out;
    for (var e = 0; e < IMAGE_EXTS.length; e++) {
      out.push('/api/texture?map=' + encodeURIComponent(this._bspName || '') +
        '&name=' + encodeURIComponent(noExt + IMAGE_EXTS[e]));
    }
    return out;
  };

  BSPViewer.prototype._loadImageFromUrls = function (urls) {
    var self = this;
    if (!urls.length) return Promise.resolve(null);

    function tryIdx(i) {
      if (i >= urls.length) return Promise.resolve(null);
      var url = urls[i];
      if (self._baseTexCache[url]) return self._baseTexCache[url];

      var p;
      if (/\.tga(?:[?&#]|$)/i.test(decodeURIComponent(url))) {
        p = fetch(url).then(function (r) {
          if (!r.ok) return null;
          return r.arrayBuffer().then(function (buf) {
            var decoded = decodeTGA(buf);
            return decoded ? self._makeRawTexture(decoded.data, decoded.width, decoded.height) : null;
          });
        }).catch(function () { return null; });
      } else {
        p = new Promise(function (resolve) {
          var img = new Image();
          if (/^https?:\/\//i.test(url)) img.crossOrigin = 'anonymous';
          img.onload  = function () { resolve(self._makeImageTexture(img)); };
          img.onerror = function () { resolve(null); };
          img.src = url;
        });
      }
      self._baseTexCache[url] = p;
      return p.then(function (tex) {
        if (tex) return tex;
        return tryIdx(i + 1);
      });
    }

    return tryIdx(0);
  };

  BSPViewer.prototype._fetchTextFromCandidates = function (urls) {
    function tryIdx(i) {
      if (i >= urls.length) return Promise.resolve(null);
      return fetch(urls[i]).then(function (r) {
        if (!r.ok) return tryIdx(i + 1);
        return r.text();
      }).catch(function () {
        return tryIdx(i + 1);
      });
    }

    return tryIdx(0);
  };

  BSPViewer.prototype._loadShaderDefinitions = function () {
    if (this._shaderDefs) return Promise.resolve(this._shaderDefs);
    if (this._shaderDefsPromise) return this._shaderDefsPromise;

    var self = this;
    /* Per-map roots: assets/bsp/mapname/scripts/ has highest priority,
       then the base assets/bsp/scripts/. Used as a fallback if the
       shaders manifest can't be reached. */
    var perMapRoots = self._bspName
      ? [LOCAL_BSP_ROOT + self._bspName + '/scripts/',
         LOCAL_BSP_ROOT + self._bspName + '/shaders/']
      : [];

    /* Fallback discovery via shaderlist.txt (used only if the server-side
       shaders manifest is unavailable). The game itself doesn't require a
       shaderlist; we just use it as a hint for the remote source where we
       can't list a directory. */
    var shaderListUrls = [
      LOCAL_BSP_ROOT + 'scripts/shaderlist.txt',
      LOCAL_BSP_ROOT + 'shaders/shaderlist.txt',
      REMOTE_ETMAIN + 'scripts/shaderlist.txt'
    ];
    for (var pi = perMapRoots.length - 1; pi >= 0; pi--) {
      shaderListUrls.unshift(perMapRoots[pi] + 'shaderlist.txt');
    }

    /* Helper: parse a list of shader file basenames out of any number of
       shaderlist.txt bodies. */
    function collectFromShaderlists(results) {
      var files = [];
      var seen = {};
      for (var si = 0; si < results.length; si++) {
        if (!results[si]) continue;
        parseShaderList(results[si]).forEach(function (name) {
          var f = normalizeAssetPath(name).replace(/\.shader$/i, '') + '.shader';
          if (!seen[f]) { seen[f] = true; files.push(f); }
        });
      }
      return files;
    }

    /* Fetch the server-side shader manifest for this specific map. Passing the
       map name ensures only shaders relevant to this map are returned, preventing
       textures from other maps being loaded via unrelated shader definitions. */
    var shaderUrl = LOCAL_BSP_SHADERS + (self._bspName ? '?map=' + encodeURIComponent(self._bspName) : '');
    shaderUrl += (shaderUrl.indexOf('?') !== -1 ? '&' : '?') + '_t=' + Date.now();
    var manifestPromise = fetch(shaderUrl).then(function (r) {
      return r.ok ? r.json() : null;
    }).catch(function () { return null; });

    this._shaderDefsPromise = manifestPromise.then(function (manifest) {
      /* Build the ordered list of shader URLs to fetch. Per-map shaders
         come first so they win on definition conflicts. */
      var allShaderUrls = [];
      var seenUrl = {};
      function pushUrl(u) {
        if (u && !seenUrl[u]) { seenUrl[u] = true; allShaderUrls.push(u); }
      }

      if (manifest && typeof manifest === 'object') {
        if (self._bspName && manifest.maps) {
          var perMap = manifest.maps[String(self._bspName).toLowerCase()];
          if (perMap && perMap.length) {
            for (var pi2 = 0; pi2 < perMap.length; pi2++) pushUrl(perMap[pi2]);
          }
        }
        if (manifest.base && manifest.base.length) {
          for (var bi = 0; bi < manifest.base.length; bi++) pushUrl(manifest.base[bi]);
        }
      }

      /* If the manifest was unavailable, fall back to shaderlist.txt + the
         conventional source folders. */
      var shaderlistFallback;
      if (allShaderUrls.length === 0) {
        shaderlistFallback = Promise.all(shaderListUrls.map(function (url) {
          return fetch(url).then(function (r) {
            return r.ok ? r.text() : null;
          }).catch(function () { return null; });
        })).then(function (results) {
          var shaderFiles = collectFromShaderlists(results);
          shaderFiles.forEach(function (shaderFile) {
            for (var pj = 0; pj < perMapRoots.length; pj++) {
              pushUrl(perMapRoots[pj] + shaderFile);
            }
            pushUrl(LOCAL_BSP_ROOT + 'scripts/' + shaderFile);
            pushUrl(LOCAL_BSP_ROOT + 'shaders/' + shaderFile);
            pushUrl(REMOTE_ETMAIN  + 'scripts/' + shaderFile);
          });
        });
      } else {
        /* Manifest is available – do NOT load unfiltered remote shaders.
           The manifest already contains the correct set of shader files
           for this map. Loading all remote shaders would pull in definitions
           from other maps, causing wrong textures. */
        shaderlistFallback = Promise.resolve();
      }

      return shaderlistFallback.then(function () {
        /* Fetch all shader URLs in parallel; merge in order so per-map and
           per-manifest-priority sources win on definition conflicts. */
        return Promise.all(allShaderUrls.map(function (u) {
          return fetch(u).then(function (r) {
            return r.ok ? r.text() : null;
          }).catch(function () { return null; });
        })).then(function (results) {
          var merged = {};
          for (var ri = 0; ri < results.length; ri++) {
            if (!results[ri]) continue;
            var parsed = parseShaderDefinitions(results[ri]);
            for (var dk in parsed) {
              if (Object.prototype.hasOwnProperty.call(parsed, dk) && !merged[dk]) {
                merged[dk] = parsed[dk];
              }
            }
          }
          self._shaderDefs = merged;
          return merged;
        });
      });
    }).catch(function () {
      self._shaderDefs = {};
      return self._shaderDefs;
    });

    return this._shaderDefsPromise;
  };

  /* Fetch a single .shader file and merge its definitions into _shaderDefs.
     Each file is only fetched once; subsequent calls return the same promise. */
  BSPViewer.prototype._fetchAndMergeShaderFile = function (shaderFile) {
    if (!this._triedShaderFiles) this._triedShaderFiles = {};
    if (this._triedShaderFiles[shaderFile]) return this._triedShaderFiles[shaderFile];

    var self = this;
    var allSources = [];
    if (self._bspName) {
      allSources.push(LOCAL_BSP_ROOT + self._bspName + '/scripts/' + shaderFile);
      allSources.push(LOCAL_BSP_ROOT + self._bspName + '/shaders/' + shaderFile);
    }
    allSources.push(
      LOCAL_BSP_ROOT + 'scripts/' + shaderFile,
      LOCAL_BSP_ROOT + 'shaders/' + shaderFile,
      REMOTE_ETMAIN + 'scripts/' + shaderFile
    );
    this._triedShaderFiles[shaderFile] = Promise.all(allSources.map(function (u) {
      return fetch(u).then(function (r) {
        return r.ok ? r.text() : null;
      }).catch(function () { return null; });
    })).then(function (results) {
      var target = self._shaderDefs || {};
      for (var ri = 0; ri < results.length; ri++) {
        if (!results[ri]) continue;
        var defs = parseShaderDefinitions(results[ri]);
        for (var k in defs) {
          if (Object.prototype.hasOwnProperty.call(defs, k) && !target[k]) target[k] = defs[k];
        }
      }
    }).catch(function () {});

    return this._triedShaderFiles[shaderFile];
  };

  BSPViewer.prototype._resolveShaderEntry = function (defs, key) {
    var altKey = key.replace(/^textures\//i, '');
    return normalizeShaderDefEntry(defs[key] || defs[altKey]);
  };

  BSPViewer.prototype._guessShaderFiles = function (key) {
    var parts = key.split('/');
    var out = [];
    var seen = {};
    function push(name) {
      var file = normalizeAssetPath(name).replace(/\.shader$/i, '') + '.shader';
      if (file && !seen[file]) { seen[file] = true; out.push(file); }
    }

    if (parts.length >= 2) push(parts[1]);
    for (var i = 2; i < parts.length - 1; i++) push(parts[i]);
    push('common');
    push('models');
    push('sfx');
    push('liquids');
    push('sky');

    return out;
  };

  BSPViewer.prototype._loadShaderMaterial = function (shaderName) {
    var key = normalizeAssetPath(shaderName).replace(/\.(jpg|jpeg|png|tga|webp)$/i, '');
    var EMPTY = { tex: null, transparent: false, noLightmap: false, additive: false, hidden: false,
                  isSky: false, skyFarBox: '', skyStages: [],
                  tcScrollS: 0, tcScrollT: 0, tcRotate: 0 };
    if (!key) return Promise.resolve(EMPTY);
    if (this._shaderTexCache[key]) return this._shaderTexCache[key];

    var self = this;
    var toolHidden = isHiddenToolTexture(key);

    function resolveFromDefs(defs) {
      var entry = self._resolveShaderEntry(defs, key);
      if (!entry) return Promise.resolve({
        tex: null, transparent: false, noLightmap: false, additive: false,
        hidden: toolHidden, isSky: false, skyFarBox: '', skyStages: [],
        tcScrollS: 0, tcScrollT: 0, tcRotate: 0
      });

      if (!entry.refs.length) {
        return Promise.resolve({
          tex: null,
          /* No image to inspect – transparency will be determined by the
           * baseTex path (baseHasAlpha) when this entry has no loaded refs. */
          transparent: false,
          noLightmap: entry.noLightmap,
          additive: entry.additive,
          hidden: entry.hidden || toolHidden,
          isSky: entry.isSky,
          skyFarBox: entry.skyFarBox || '',
          skyStages: entry.skyStages || [],
          tcScrollS: entry.tcScrollS,
          tcScrollT: entry.tcScrollT,
          tcRotate:  entry.tcRotate
        });
      }

      var urls = [];
      var seen = {};
      for (var i = 0; i < entry.refs.length; i++) {
        var candidates = self._shaderToCandidates(entry.refs[i]);
        for (var j = 0; j < candidates.length; j++) uniquePush(urls, seen, candidates[j]);
      }
      return self._loadImageFromUrls(urls).then(function (texInfo) {
        if (!texInfo) self._logAsset('imageFailed', key, urls);
        /* Use pixel data to decide transparency: only if the loaded image
         * has fully-transparent (alpha===0) pixels is the surface transparent. */
        return {
          tex: texInfo ? texInfo.tex : null,
          transparent: !!(texInfo && texInfo.hasAlpha),
          noLightmap: entry.noLightmap,
          additive: entry.additive,
          hidden: entry.hidden || toolHidden,
          isSky: entry.isSky,
          skyFarBox: entry.skyFarBox || '',
          skyStages: entry.skyStages || [],
          tcScrollS: entry.tcScrollS,
          tcScrollT: entry.tcScrollT,
          tcRotate:  entry.tcRotate
        };
      });
    }

    var baseUrls = this._shaderToCandidates(key);
    this._shaderTexCache[key] = this._loadImageFromUrls(baseUrls).then(function (baseTexInfo) {
      var baseTex      = baseTexInfo ? baseTexInfo.tex  : null;
      var baseHasAlpha = !!(baseTexInfo && baseTexInfo.hasAlpha);
      return self._loadShaderDefinitions().then(function (defs) {
        var direct = self._resolveShaderEntry(defs, key);
        if (direct) {
          return resolveFromDefs(defs).then(function (m) {
            if (m.tex) return m;
            if (!baseTex && !m.isSky && !toolHidden) {
              self._logAsset('shaderNoImage', key, (direct.refs || []).slice());
            }
            return {
              tex: baseTex,
              transparent: baseHasAlpha,
              noLightmap: m.noLightmap,
              additive: m.additive,
              hidden: m.hidden || toolHidden,
              isSky: m.isSky,
              skyFarBox: m.skyFarBox || '',
              skyStages: m.skyStages || [],
              tcScrollS: m.tcScrollS,
              tcScrollT: m.tcScrollT,
              tcRotate:  m.tcRotate
            };
          });
        }

        var guessedFiles = self._guessShaderFiles(key);
        return Promise.all(guessedFiles.map(function (f) {
          return self._fetchAndMergeShaderFile(f);
        })).then(function () {
          return resolveFromDefs(self._shaderDefs || {}).then(function (m2) {
            if (m2.tex || m2.noLightmap || m2.isSky) return m2;
            /* Heuristic fallback: if the BSP texture name indicates a sky
               surface (e.g. textures/skies/…) treat it as sky even when no
               .shader definition was found. This prevents sky face geometry
               from being drawn as opaque surfaces over the skybox. */
            if (isLikelySkyTexture(key)) {
              return { tex: null, transparent: false, noLightmap: true, additive: false,
                       hidden: false, isSky: true, skyFarBox: self._worldspawnSkyBox || '', skyStages: [],
                       tcScrollS: 0, tcScrollT: 0, tcRotate: 0 };
            }
            if (!baseTex && !toolHidden) {
              self._logAsset('shaderMissing', key, guessedFiles);
            }
            return { tex: baseTex, transparent: baseHasAlpha, noLightmap: false, additive: false,
                     hidden: toolHidden, isSky: false, skyFarBox: '', skyStages: [],
                     tcScrollS: 0, tcScrollT: 0, tcRotate: 0 };
          });
        });
      });
    });

    return this._shaderTexCache[key];
  };

  BSPViewer.prototype._loadBaseTexture = function (shaderName) {
    return this._loadShaderMaterial(shaderName);
  };

  BSPViewer.prototype._bspToCandidates = function (url) {
    var out = [url];
    var m = String(url || '').match(/([^\/?#]+)\.bsp(?:[?#].*)?$/i);
    if (m && m[1]) {
      var bspFile = m[1] + '.bsp';
      var mapSpecific = LOCAL_BSP_ROOT + m[1] + '/maps/' + bspFile;
      if (mapSpecific !== url) out.push(mapSpecific);
      var local = LOCAL_BSP_MAPS + bspFile;
      if (local !== url && local !== mapSpecific) out.push(local);
      out.push(REMOTE_ETMAIN + 'maps/' + bspFile);
    }
    return out;
  };

  BSPViewer.prototype._uploadBatch = function (batch, lmTex, hasLM, baseTex, hasBase, transparent, additive, tcScrollS, tcScrollT, tcRotate) {
    var gl = this.gl;
    var n  = batch.pos.length / 3;
    if (n === 0 || batch.idx.length === 0) return null;

    /* Interleaved buffer: pos(3) + tc(2) + lm(2) + col(4) = 11 floats per vertex */
    var STRIDE = 11;
    var buf = new Float32Array(n * STRIDE);
    /* Track the axis-aligned bounding box of this batch for frustum culling. */
    var minX =  Infinity, minY =  Infinity, minZ =  Infinity;
    var maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;
    for (var i = 0; i < n; i++) {
      var px = batch.pos[i*3];
      var py = batch.pos[i*3+1];
      var pz = batch.pos[i*3+2];
      if (px < minX) minX = px;  if (px > maxX) maxX = px;
      if (py < minY) minY = py;  if (py > maxY) maxY = py;
      if (pz < minZ) minZ = pz;  if (pz > maxZ) maxZ = pz;
      buf[i*STRIDE]   = px;
      buf[i*STRIDE+1] = py;
      buf[i*STRIDE+2] = pz;
      buf[i*STRIDE+3] = batch.tc[i*2];
      buf[i*STRIDE+4] = batch.tc[i*2+1];
      buf[i*STRIDE+5] = batch.lm[i*2];
      buf[i*STRIDE+6] = batch.lm[i*2+1];
      buf[i*STRIDE+7] = batch.col[i*4];
      buf[i*STRIDE+8] = batch.col[i*4+1];
      buf[i*STRIDE+9] = batch.col[i*4+2];
      buf[i*STRIDE+10] = batch.col[i*4+3];
    }

    var vbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.bufferData(gl.ARRAY_BUFFER, buf, gl.STATIC_DRAW);

    var ibo     = gl.createBuffer();
    var maxIdx  = maxOfArray(batch.idx);
    var idxType;
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
    if (maxIdx < 65536) {
      gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(batch.idx), gl.STATIC_DRAW);
      idxType = gl.UNSIGNED_SHORT;
    } else if (this._u32ext) {
      gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint32Array(batch.idx), gl.STATIC_DRAW);
      idxType = gl.UNSIGNED_INT;
    } else {
      /* No Uint32 support – skip oversized batch (rare) */
      gl.deleteBuffer(vbo);
      gl.deleteBuffer(ibo);
      console.warn('BSPViewer: skipping large batch – OES_element_index_uint not available');
      return null;
    }

    /* Build a dedicated line index buffer so wireframe rendering shows the
       triangle edges instead of arbitrary GL_LINES pairs of triangle indices.
       Each triangle (i0, i1, i2) becomes 3 line segments (i0-i1, i1-i2, i2-i0).
       Duplicate edges are tolerated – the cost is negligible compared to
       building an edge-deduplication table on every batch. */
    var triCount = (batch.idx.length / 3) | 0;
    var lineIdx;
    if (idxType === gl.UNSIGNED_SHORT) {
      lineIdx = new Uint16Array(triCount * 6);
    } else {
      lineIdx = new Uint32Array(triCount * 6);
    }
    for (var ti = 0, li = 0; ti < triCount; ti++) {
      var a = batch.idx[ti * 3];
      var b2 = batch.idx[ti * 3 + 1];
      var c2 = batch.idx[ti * 3 + 2];
      lineIdx[li++] = a;  lineIdx[li++] = b2;
      lineIdx[li++] = b2; lineIdx[li++] = c2;
      lineIdx[li++] = c2; lineIdx[li++] = a;
    }
    var lineIbo = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, lineIbo);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, lineIdx, gl.STATIC_DRAW);

    return {
      vbo: vbo,
      ibo: ibo,
      lineIbo: lineIbo,
      lineCount: lineIdx.length,
      count: batch.idx.length,
      lmTex: lmTex,
      hasLM: hasLM,
      baseTex: baseTex,
      hasBase: hasBase,
      transparent: !!transparent,
      additive: !!additive,
      tcScrollS: +tcScrollS || 0,
      tcScrollT: +tcScrollT || 0,
      tcRotate:  +tcRotate  || 0,
      idxType: idxType,
      /* AABB in GL world space – used for view-frustum culling. */
      aabbMin: [minX, minY, minZ],
      aabbMax: [maxX, maxY, maxZ]
    };
  };

  /**
   * Load and parse a BSP file from a URL.
   * Returns a Promise that resolves when geometry is uploaded to the GPU.
   */
  BSPViewer.prototype.load = function (url) {
    var self = this;
    var urls = this._bspToCandidates(url);
    /* Per-map bookmark storage key. Persists camera positions per BSP. */
    var bspName = this._navigationMap || String(url || '').split('/').pop().replace(/\.bsp.*$/i, '');
    this._bookmarksKey = 'bsp_bookmarks:' + bspName;
    /* Remember the basename so we can look up external lightmap TGAs in
       maps/<bspname>/lm_NNNN.tga next to the BSP. */
    this._bspName = bspName;
    try {
      var raw = window.localStorage && localStorage.getItem(this._bookmarksKey);
      this._bookmarks = raw ? (JSON.parse(raw) || {}) : {};
    } catch (e) { this._bookmarks = {}; }

    function tryIdx(i) {
      if (i >= urls.length) throw new Error('HTTP fetch failed for BSP: ' + url);
      var tryUrl = urls[i];
      return fetch(tryUrl).then(function (r) {
        if (!r.ok) {
          if (i + 1 >= urls.length) throw new Error('HTTP ' + r.status + ' for BSP: ' + url);
          return tryIdx(i + 1);
        }
        return r.arrayBuffer().then(function (buf) {
          return self._loadBuffer(buf);
        });
      }).catch(function () {
        return tryIdx(i + 1);
      });
    }

    return tryIdx(0);
  };

  BSPViewer.prototype._loadBuffer = function (buf) {
    var self    = this;
    var bsp     = parseBSP(buf, this._overbrightBits);
    var batches = buildBatches(bsp);

    /* Reset any sky cubemap from a previous load – the new map may have
       a different farbox (or none at all). */
    if (this._skyCubeTex) { this.gl.deleteTexture(this._skyCubeTex); this._skyCubeTex = null; }
    this._skyFarBox = '';
    this._skyIsReal = false;
    this._skyPromise = null;
    this._hasSkyFaces = false;
    this._skyFaceCount = 0;
    /* Free any stage-sky textures from the previous map. */
    this._disposeSkyStageTextures();
    this._skyStagePromise = null;

    /* Extract skybox path from worldspawn entity – used as fallback when the
       shader definition's skyparms isn't available or can't be loaded.
       Also extract lighting parameters (_minlight, _mingridlight, etc.) and
       ambient color (_color) for improved rendering fidelity. */
    this._worldspawnSkyBox = '';
    this._minLight = 0;
    this._skyColorTop = null;
    this._skyColorHorizon = null;
    if (bsp.entities && bsp.entities.length > 0) {
      for (var wsi = 0; wsi < bsp.entities.length; wsi++) {
        if (bsp.entities[wsi].classname === 'worldspawn') {
          var ws = bsp.entities[wsi];
          var skyVal = ws._skybox || ws.skybox || '';
          if (skyVal) {
            this._worldspawnSkyBox = normalizeAssetPath(skyVal).replace(/\.(jpg|jpeg|png|tga|webp)$/i, '');
          }
          /* _minlight / _mingridlight / _minvertexlight: the highest of these
             values is used as the minimum lighting floor (0-255 range). */
          var ml  = parseInt(ws._minlight || '0', 10) || 0;
          var mgl = parseInt(ws._mingridlight || '0', 10) || 0;
          var mvl = parseInt(ws._minvertexlight || '0', 10) || 0;
          this._minLight = Math.max(ml, mgl, mvl);
          /* _color: worldspawn ambient color (space-separated floats 0..1).
             Used to tint the procedural sky gradient when no cubemap is found. */
          if (ws._color) {
            var cp = ws._color.split(/\s+/).map(parseFloat);
            if (cp.length >= 3 && isFinite(cp[0]) && isFinite(cp[1]) && isFinite(cp[2])) {
              /* Derive sky gradient from ambient: zenith is a deeper blue tint,
                 horizon is lighter and warmer. Scale so the sky is visible. */
              this._skyColorTop     = [cp[0] * 0.2, cp[1] * 0.25, cp[2] * 0.55];
              this._skyColorHorizon = [cp[0] * 0.5, cp[1] * 0.55, cp[2] * 0.7];
            }
          }
          break;
        }
      }
    }

    /* Reset shader caches so each map loads its own shader definitions.
       Without this, sky/water/liquid shaders from the first map would be
       reused for every subsequent map load. */
    this._shaderDefs = null;
    this._shaderDefsPromise = null;
    this._shaderTexCache = {};
    this._baseTexCache = {};
    this._triedShaderFiles = {};
    /* Invalidate the manifest cache so the fresh manifest (with updated
       cache-busting URLs) is always fetched for each map load. */
    this._manifest = null;
    this._manifestPromise = null;
    this._manifestMap = null;

    /* Reset asset diagnostics log so each map starts from a clean slate. */
    this._assetLog = {
      shaderMissing: {}, shaderNoImage: {}, imageFailed: {}, skyMissing: {},
      modelMissing: {}
    };
    if (typeof this.onAssetUpdate === 'function') {
      try { this.onAssetUpdate(this.getAssetReport()); } catch (e) { /* swallow */ }
    }

    /* Per-map statistics – exposed via getStats() for the UI info panel.
       Texture count is the number of distinct, non-tool shader names that
       actually contribute geometry (matches what the user perceives). */
    var usedTex = {};
    Object.keys(batches).forEach(function (k) {
      var tIdx = batches[k].texIdx;
      if (tIdx >= 0 && tIdx < bsp.textures.length) usedTex[tIdx] = true;
    });
    self._stats = {
      faces:     bsp.faces ? bsp.faces.length : 0,
      vertices:  bsp.verts ? bsp.verts.length : 0,
      textures:  Object.keys(usedTex).length,
      lightmaps: bsp.lightmaps ? bsp.lightmaps.length : 0,
      entities:  bsp.entities ? bsp.entities.length : 0,
      batches:   Object.keys(batches).length
    };

    /* Load texture manifest first so _shaderToCandidates can use it for every
     * texture resolution in this BSP.  The manifest is re-fetched when the map
     * name changes so that per-map texture priority is applied correctly. */
    return self._loadManifest().then(function () {
      /* Probe for external lightmaps next to the BSP (higher resolution than
         the 128² ones in the LIGHTMAPS lump). If any are found we use them
         instead of the embedded ones; otherwise we fall back to the latter. */
      return self._tryLoadExternalLightmaps(self._bspName, bsp.lightmaps.length)
        .then(function (extLms) { return extLms || []; });
    }).then(function (extLms) {

    /* Upload lightmap textures */
    var lmTexs;
    /* Only use external lightmaps when a COMPLETE set was found (at least as
       many as the BSP's embedded count). A partial set would leave some faces
       without a lightmap, causing inconsistent brightness across brushes. */
    if (extLms && extLms.length > 0 && extLms.length >= bsp.lightmaps.length) {
      lmTexs = extLms.map(function (lm) {
        return this._makeLMTextureRGBA(lm.rgba, lm.width, lm.height);
      }, self);
      self._stats = self._stats || {};
      self._stats.externalLightmaps = lmTexs.length;
      self._stats.lightmaps = lmTexs.length;
    } else {
      lmTexs = bsp.lightmaps.map(function (d) {
        return this._makeLMTexture(d);
      }, self);
    }

    /* Upload geometry batches */
    self._drawCalls = [];
    var keys = Object.keys(batches);
    var materialPromiseByIdx = {};

    for (var ki = 0; ki < keys.length; ki++) {
      var b = batches[keys[ki]];
      if (b.texIdx >= 0 && b.texIdx < bsp.textures.length && !materialPromiseByIdx[b.texIdx]) {
        materialPromiseByIdx[b.texIdx] = self._loadBaseTexture(bsp.textures[b.texIdx].name);
      }
    }

    /* Progress tracking – one tick per unique texture index */
    var matKeys = Object.keys(materialPromiseByIdx);
    var totalMaterials = matKeys.length;
    var loadedMaterials = 0;
    self._reportProgress(0, totalMaterials, '');

    var trackedByIdx = {};
    matKeys.forEach(function (tidx) {
      var rawName = (bsp.textures[+tidx] && bsp.textures[+tidx].name) || String(tidx);
      var shortName = rawName.split('/').pop() || rawName;
      trackedByIdx[tidx] = materialPromiseByIdx[tidx].then(function (mat) {
        loadedMaterials++;
        self._reportProgress(loadedMaterials, totalMaterials, shortName);
        return mat;
      });
    });

    return Promise.all(keys.map(function (k) {
      var batch = batches[k];
      var lmTex = (batch.lmIdx >= 0 && batch.lmIdx < lmTexs.length) ? lmTexs[batch.lmIdx] : null;
      var hasLM = lmTex ? 1.0 : 0.0;

      if (batch.texIdx >= 0 && batch.texIdx < bsp.textures.length && trackedByIdx[batch.texIdx]) {
        return trackedByIdx[batch.texIdx].then(function (mat) {
          if (mat && mat.hidden) return;
          /* Sky surfaces – kick off cubemap load on first encounter, then
             skip the geometry from the world batch list (the skybox is
             drawn separately by _drawSky). */
          if (mat && mat.isSky) {
            self._hasSkyFaces = true;
            /* Count the triangles in this sky batch as faces so the UI can
               report how much sky geometry the map actually has. */
            self._skyFaceCount += (batch.idx.length / 3) | 0;
            var skyBox = mat.skyFarBox || self._worldspawnSkyBox || '';
            if (skyBox && !self._skyCubeTex && !self._skyPromise) {
              self._skyPromise = self._loadSkyCubemap(skyBox);
            }
            /* Stage-based skies (scrolling clouds / masks) – load the layer
               textures on first encounter so _drawSky can composite them. */
            if (mat.skyStages && mat.skyStages.length && !self._skyStages && !self._skyStagePromise) {
              self._skyStagePromise = self._loadSkyStages(mat.skyStages).then(function (loaded) {
                self._skyStages = loaded;
                return loaded;
              });
            }
            return;
          }
          var baseTex = mat && mat.tex ? mat.tex : null;
          var hasBase = baseTex ? 1.0 : 0.0;
          var isTransparent = !!(mat && mat.transparent);
          var isAdditive = !!(mat && mat.additive);
          var useLM = (mat && mat.noLightmap) ? 0.0 : hasLM;
          var dc = self._uploadBatch(
            batch,
            lmTex || self._greyTex,
            useLM,
            baseTex || self._whiteTex,
            hasBase,
            isTransparent,
            isAdditive,
            mat && mat.tcScrollS,
            mat && mat.tcScrollT,
            mat && mat.tcRotate
          );
          if (dc) self._drawCalls.push(dc);
        });
      }

      var dcNoBase = self._uploadBatch(batch, lmTex || self._greyTex, hasLM, self._whiteTex, 0.0, false, false, 0, 0, 0);
      if (dcNoBase) self._drawCalls.push(dcNoBase);
      return Promise.resolve();
    })).then(function () {
      /* Compute map bounds for speed calculation */
      var minX =  1e9, maxX = -1e9;
      var minY =  1e9, maxY = -1e9;
      var minZ =  1e9, maxZ = -1e9;
      for (var vi = 0; vi < bsp.verts.length; vi++) {
        var p = bsp.verts[vi].pos;
        /* Q3→GL: (x, z, −y) */
        var wx =  p[0], wy = p[2], wz = -p[1];
        if (wx < minX) minX = wx;  if (wx > maxX) maxX = wx;
        if (wy < minY) minY = wy;  if (wy > maxY) maxY = wy;
        if (wz < minZ) minZ = wz;  if (wz > maxZ) maxZ = wz;
      }

      /* ─── Find a valid spawn point from entities (Q3/ET style) ─── */
      /* ET spawn entity classnames in priority order. */
      var spawnClasses = [
        'info_player_deathmatch',
        'team_CTF_redplayer', 'team_CTF_blueplayer',
        'info_player_allied', 'info_player_axis',
        'team_WOLF_objective',
        'info_player_start',
        'info_player_intermission'
      ];
      var spawnEnt = null;
      if (bsp.entities && bsp.entities.length > 0) {
        for (var si = 0; si < spawnClasses.length && !spawnEnt; si++) {
          for (var se = 0; se < bsp.entities.length; se++) {
            if (bsp.entities[se].classname === spawnClasses[si] && bsp.entities[se].origin) {
              spawnEnt = bsp.entities[se];
              break;
            }
          }
        }
      }

      var cx, cy, cz;
      if (spawnEnt && spawnEnt.origin) {
        /* Parse origin "x y z" (Q3 space) → convert to GL space */
        var parts = spawnEnt.origin.split(/\s+/);
        var q3x = parseFloat(parts[0]) || 0;
        var q3y = parseFloat(parts[1]) || 0;
        var q3z = parseFloat(parts[2]) || 0;
        /* Q3→GL: (x, z, -y) */
        cx = q3x;
        cy = q3z;
        cz = -q3y;
        /* Add eye height offset (spawn origin is at feet in ET) */
        cy += 56;
        /* Parse angle if available for initial yaw */
        if (spawnEnt.angle) {
          /* ET angle: 0=east, 90=north, etc. Convert to our yaw. */
          var angleDeg = parseFloat(spawnEnt.angle) || 0;
          /* Our yaw: 0 = looking along -Z (north in Q3), rotates CW.
             ET angle: 0 = +X (east), 90 = +Y (north), CCW.
             GL forward is -Z = Q3 +Y when yaw=0. 
             yaw = -(angle - 90) in radians = (90 - angle) * pi/180 */
          self._cam.yaw = (90 - angleDeg) * Math.PI / 180;
        }
      } else {
        /* Fallback: position at map centre slightly above floor */
        cx = (minX + maxX) * 0.5;
        cy = minY + (maxY - minY) * 0.15;
        cz = (minZ + maxZ) * 0.5;
      }

      self._cam.pos   = [cx, cy, cz];
      var baseSpd = 300;
      self._cam.baseSpeed = baseSpd;
      self._cam.speed     = baseSpd * (self._cam.speedMul || 1.0);

      /* Build the spawn-marker overlay buffer from all spawn entities. */
      try { self._buildSpawnMarkers(bsp); } catch (e) { /* non-fatal */ }

      /* Build model-marker overlay and log missing models to the console. */
      try { self._buildModelMarkers(bsp); } catch (e) { /* non-fatal */ }

      /* Load external .md3/.mdc models referenced by entities. */
      try { self._loadExternalModels(bsp); } catch (e) { /* non-fatal */ }

      /* Build collision mesh now that geometry is finalised. */
      try { self._buildCollision(bsp); } catch (e) { self._cm = null; }
    });
  }); /* end _loadManifest().then */
  };

  BSPViewer.prototype.start = function () {
    if (this._running) return;
    this._running = true;
    this._lastT   = performance.now();
    var self = this;
    var frame = function (now) {
      if (!self._running) return;
      var dt = Math.min((now - self._lastT) / 1000.0, 0.1);
      self._lastT = now;
      self._moveCamera(dt);
      self._draw();
      self._raf = requestAnimationFrame(frame);
    };
    this._raf = requestAnimationFrame(frame);
  };

  BSPViewer.prototype.stop = function () {
    this._running = false;
    if (this._raf) { cancelAnimationFrame(this._raf); this._raf = null; }
  };

  BSPViewer.prototype.destroy = function () {
    this.stop();
    this._disposeNavigation();
    if(this._navigationTextLayer){this._navigationTextLayer.remove();this._navigationTextLayer=null;}
    this.canvas.removeEventListener('click', this._onCanvasClick);
    if (document.pointerLockElement === this.canvas) document.exitPointerLock();
    document.removeEventListener('pointerlockchange', this._onPLC);
    document.removeEventListener('mousemove',         this._onMM);
    document.removeEventListener('keydown',           this._onKD);
    document.removeEventListener('keyup',             this._onKU);
    /* Free GPU resources */
    var gl = this.gl;
    if (this._navigationProgram) { gl.deleteProgram(this._navigationProgram); this._navigationProgram = null; }
    for (var i = 0; i < this._drawCalls.length; i++) {
      gl.deleteBuffer(this._drawCalls[i].vbo);
      gl.deleteBuffer(this._drawCalls[i].ibo);
      if (this._drawCalls[i].lineIbo) gl.deleteBuffer(this._drawCalls[i].lineIbo);
    }
    this._drawCalls = [];
    if (this._spawnVbo) { gl.deleteBuffer(this._spawnVbo); this._spawnVbo = null; }
    this._spawnVertexCount = 0;
    this._spawnCount       = 0;
    if (this._modelVbo) { gl.deleteBuffer(this._modelVbo); this._modelVbo = null; }
    this._modelVertexCount = 0;
    this._modelCount       = 0;
    /* Free loaded MD3 model GPU buffers */
    for (var mi = 0; mi < this._md3Models.length; mi++) {
      if (this._md3Models[mi].vbo) gl.deleteBuffer(this._md3Models[mi].vbo);
      if (this._md3Models[mi].ibo) gl.deleteBuffer(this._md3Models[mi].ibo);
    }
    this._md3Models = [];
    if (this._skyCubeTex) { gl.deleteTexture(this._skyCubeTex); this._skyCubeTex = null; }
    if (this._skyVbo)     { gl.deleteBuffer(this._skyVbo);     this._skyVbo = null; }
    if (this._skyIbo)     { gl.deleteBuffer(this._skyIbo);     this._skyIbo = null; }
    this._disposeSkyStageTextures();
    if (this._skyStageVbo) { gl.deleteBuffer(this._skyStageVbo); this._skyStageVbo = null; }
    if (this._skyStageIbo) { gl.deleteBuffer(this._skyStageIbo); this._skyStageIbo = null; }
  };

  BSPViewer.prototype.setBrightness = function (v) {
    this._brightness = Math.max(0.1, Math.min(20.0, parseFloat(v) || 1.5));
  };

  /** Output gamma (matches ET's r_gamma cvar; default 1.2). Higher = brighter. */
  BSPViewer.prototype.setGamma = function (v) {
    var g = parseFloat(v);
    if (!isFinite(g) || g <= 0) g = 1.2;
    this._gamma = Math.max(0.5, Math.min(3.0, g));
  };
  BSPViewer.prototype.getGamma = function () { return this._gamma; };

  /** Overbright shift applied to lightmaps and BSP vertex colors at upload
   *  time (mirrors ET's r_overBrightBits). Changing this only takes effect
   *  on the *next* BSP load — existing GPU textures are not re-uploaded. */
  BSPViewer.prototype.setOverbrightBits = function (v) {
    var n = parseInt(v, 10);
    if (!isFinite(n) || n < 0) n = 1;
    this._overbrightBits = Math.min(4, n);
  };
  BSPViewer.prototype.getOverbrightBits = function () { return this._overbrightBits; };

  /* ── Public configuration setters used by the host UI ───────────────────── */

  /** Toggle (or force) wireframe rendering via gl.LINES. */
  BSPViewer.prototype.setWireframe = function (on) {
    if (typeof on === 'undefined') on = !this._wireframe;
    this._wireframe = !!on;
    return this._wireframe;
  };
  BSPViewer.prototype.isWireframe = function () { return !!this._wireframe; };

  /** Distance-fog density in 1 / world-units (0 disables fog). */
  BSPViewer.prototype.setFog = function (density) {
    var d = parseFloat(density);
    if (!isFinite(d) || d < 0) d = 0;
    this._fogDensity = Math.min(d, 0.01);
  };

  /** Set background color (and matching fog color). Accepts [r,g,b] in 0..1. */
  BSPViewer.prototype.setBackgroundColor = function (rgb) {
    if (!rgb || rgb.length < 3) return;
    this._bgColor = [
      Math.max(0, Math.min(1, +rgb[0] || 0)),
      Math.max(0, Math.min(1, +rgb[1] || 0)),
      Math.max(0, Math.min(1, +rgb[2] || 0))
    ];
    this.gl.clearColor(this._bgColor[0], this._bgColor[1], this._bgColor[2], 1.0);
  };

  /* ── Spawn-point overlay ──────────────────────────────────────────────────
     Build a GL_LINES buffer from all `info_player_*` / `team_*` spawn entities
     in the BSP. Each spawn is rendered as a vertical pillar (≈ 56 units tall –
     ET eye height) plus a small ground-level "+" so it remains visible from
     above. Color encodes the team: Allies = blue, Axis = red, neutral = yellow.
  */
  var SPAWN_CLASSES = {
    'info_player_allied':       [0.30, 0.55, 1.00],
    'team_ctf_blueplayer':      [0.30, 0.55, 1.00],
    'team_ctf_bluespawn':       [0.30, 0.55, 1.00],
    'info_player_axis':         [1.00, 0.35, 0.30],
    'team_ctf_redplayer':       [1.00, 0.35, 0.30],
    'team_ctf_redspawn':        [1.00, 0.35, 0.30],
    'info_player_deathmatch':   [1.00, 0.90, 0.30],
    'info_player_start':        [1.00, 0.90, 0.30],
    'info_player_intermission': [0.70, 0.70, 0.70],
    'team_wolf_objective':      [0.70, 0.70, 0.70]
  };

  BSPViewer.prototype._buildSpawnMarkers = function (bsp) {
    var gl = this.gl;
    /* Discard any previous buffer (e.g. when loading a second BSP). */
    if (this._spawnVbo) { gl.deleteBuffer(this._spawnVbo); this._spawnVbo = null; }
    this._spawnVertexCount = 0;
    this._spawnCount       = 0;
    if (!bsp || !bsp.entities || !bsp.entities.length) return;

    var verts = [];   /* interleaved x,y,z,r,g,b */
    var count = 0;
    for (var i = 0; i < bsp.entities.length; i++) {
      var ent = bsp.entities[i];
      if (!ent || !ent.classname || !ent.origin) continue;
      var cn = String(ent.classname).toLowerCase();
      var color = SPAWN_CLASSES[cn];
      if (!color) continue;
      var parts = String(ent.origin).split(/\s+/);
      var q3x = parseFloat(parts[0]);
      var q3y = parseFloat(parts[1]);
      var q3z = parseFloat(parts[2]);
      if (!isFinite(q3x) || !isFinite(q3y) || !isFinite(q3z)) continue;
      /* Q3 → GL: (x, z, -y) */
      var gx =  q3x;
      var gy =  q3z;
      var gz = -q3y;
      var r = color[0], g = color[1], b = color[2];
      var H = 56;   /* pillar height (ET eye height in units) */
      var X = 18;   /* arm half-length of ground cross */
      /* Vertical pillar: foot → head */
      verts.push(gx, gy,     gz, r, g, b,
                 gx, gy + H, gz, r, g, b);
      /* Ground cross along world X */
      verts.push(gx - X, gy + 1, gz,     r, g, b,
                 gx + X, gy + 1, gz,     r, g, b);
      /* Ground cross along world Z */
      verts.push(gx, gy + 1, gz - X,     r, g, b,
                 gx, gy + 1, gz + X,     r, g, b);
      /* Top tick (helps spotting from far away) */
      verts.push(gx - 6, gy + H, gz,     r, g, b,
                 gx + 6, gy + H, gz,     r, g, b);
      count++;
    }
    if (!verts.length) return;

    this._spawnVbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this._spawnVbo);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.STATIC_DRAW);
    this._spawnVertexCount = verts.length / 6;
    this._spawnCount       = count;
  };

  /** Toggle (or force) the spawn-point overlay. Returns the new state. */
  BSPViewer.prototype.setSpawnMarkers = function (on) {
    if (typeof on === 'undefined') on = !this._spawnMarkers;
    this._spawnMarkers = !!on;
    return this._spawnMarkers;
  };
  /** Whether the spawn-point overlay is currently enabled. */
  BSPViewer.prototype.hasSpawnMarkers = function () { return !!this._spawnMarkers; };
  /** Number of spawn entities discovered in the current BSP (0 if none). */
  BSPViewer.prototype.getSpawnCount   = function () { return this._spawnCount | 0; };


  /* ─── Missing-model markers ──────────────────────────────────────────────
     Entity classnames that reference external .md3/.mdc models via a "model"
     key. BSP inline models (e.g. "*1") are not external and are already part
     of the world geometry, so we skip those. */
  var MODEL_ENTITY_CLASSES = [
    'misc_model', 'misc_gamemodel', 'misc_model_static',
    'props_chair', 'props_bench', 'props_crate_32',
    'props_crate_64', 'props_flamebarrel', 'props_locker_tall'
  ];

  /**
     Build a GL_TRIANGLES buffer of small RGB triangles at each entity position
     that references an external model file. Each triangle cycles through
     R/G/B channels so they are easy to spot against any surface.
  */
  BSPViewer.prototype._buildModelMarkers = function (bsp) {
    var gl = this.gl;
    if (this._modelVbo) { gl.deleteBuffer(this._modelVbo); this._modelVbo = null; }
    this._modelVertexCount = 0;
    this._modelCount       = 0;
    if (!bsp || !bsp.entities || !bsp.entities.length) return;

    var verts = [];   /* interleaved x,y,z,r,g,b */
    var count = 0;
    /* Cycle through bright colors so markers are visible. */
    var colors = [
      [1.0, 0.2, 0.2],  /* red */
      [0.2, 1.0, 0.2],  /* green */
      [0.3, 0.4, 1.0]   /* blue */
    ];

    for (var i = 0; i < bsp.entities.length; i++) {
      var ent = bsp.entities[i];
      if (!ent || !ent.origin) continue;

      /* Determine if this entity references an external model. */
      var modelPath = ent.model || '';
      if (!modelPath) continue;
      /* Skip BSP inline models (start with "*") – those are already rendered. */
      if (modelPath.charAt(0) === '*') continue;
      /* Accept entities with a model key pointing to .md3/.mdc or any known
         model class, or any entity with a model key ending in a model extension. */
      var cn = String(ent.classname || '').toLowerCase();
      var isModelClass = false;
      for (var mc = 0; mc < MODEL_ENTITY_CLASSES.length; mc++) {
        if (cn === MODEL_ENTITY_CLASSES[mc]) { isModelClass = true; break; }
      }
      var hasModelExt = /\.(md3|mdc|mdx|mds|mdm|obj|ase)$/i.test(modelPath);
      if (!isModelClass && !hasModelExt) continue;

      /* Log this missing model to the asset diagnostics console. */
      this._logAsset('modelMissing', modelPath, [cn + ' @ ' + (ent.origin || '?')]);
      if (typeof console !== 'undefined' && console.warn) {
        console.warn('[BSP] Missing model: ' + modelPath + ' (' + cn + ' @ ' + (ent.origin || '?') + ')');
      }

      /* Parse origin "x y z" (Q3 space) and convert to GL space. */
      var parts = String(ent.origin).split(/\s+/);
      var q3x = parseFloat(parts[0]);
      var q3y = parseFloat(parts[1]);
      var q3z = parseFloat(parts[2]);
      if (!isFinite(q3x) || !isFinite(q3y) || !isFinite(q3z)) continue;
      /* Q3 → GL: (x, z, -y) */
      var gx =  q3x;
      var gy =  q3z;
      var gz = -q3y;

      var col = colors[count % colors.length];
      var r = col[0], g = col[1], b = col[2];
      var S = 16;  /* half-size of the triangle marker */

      /* Draw an upward-pointing triangle (3 verts) */
      verts.push(gx,     gy + S * 2, gz,     r, g, b);  /* top */
      verts.push(gx - S, gy,         gz,     r, g, b);  /* bottom-left */
      verts.push(gx + S, gy,         gz,     r, g, b);  /* bottom-right */

      /* Draw a second triangle rotated 90° around Y for visibility from the side */
      verts.push(gx, gy + S * 2, gz,     r, g, b);  /* top */
      verts.push(gx, gy,         gz - S, r, g, b);  /* bottom-left */
      verts.push(gx, gy,         gz + S, r, g, b);  /* bottom-right */

      count++;
    }
    if (!verts.length) return;

    this._modelVbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this._modelVbo);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.STATIC_DRAW);
    this._modelVertexCount = verts.length / 6;
    this._modelCount       = count;
  };

  /** Toggle (or force) the missing-model marker overlay. Returns the new state. */
  BSPViewer.prototype.setModelMarkers = function (on) {
    if (typeof on === 'undefined') on = !this._modelMarkers;
    this._modelMarkers = !!on;
    return this._modelMarkers;
  };
  /** Whether the missing-model marker overlay is currently enabled. */
  BSPViewer.prototype.hasModelMarkers = function () { return !!this._modelMarkers; };
  /** Number of missing-model entities discovered in the current BSP (0 if none). */
  BSPViewer.prototype.getModelCount   = function () { return this._modelCount | 0; };


  /* ─── MD3/MDC Model Loading ─────────────────────────────────────────────
     Parse external .md3/.mdc model files referenced by BSP entities, build
     GL buffers, and render them at the entity's position/rotation.
     MD3 format: https://icculus.org/homepages/phaethon/q3a/formats/md3format.html */

  var MD3_MAGIC = 0x33504449;  /* "IDP3" little-endian */
  var MDC_MAGIC = 0x43504449;  /* "IDPC" little-endian */

  /**
   * Parse an MD3 binary ArrayBuffer.
   * Returns { surfaces: [{ name, verts, normals, texcoords, triangles, shaderName }] }
   * Only frame 0 is used (static placement).
   */
  function parseMD3(buf) {
    var dv = new DataView(buf);
    var magic = dv.getUint32(0, true);
    if (magic !== MD3_MAGIC && magic !== MDC_MAGIC) return null;

    var isMDC = (magic === MDC_MAGIC);

    /* Main header layout (offsets in bytes):
       MD3:  ident(4) version(4) name(64) flags(4) numFrames(4) numTags(4)
             numSurfaces(4)@84 numSkins(4) ofsFrames(4) ofsTags(4)
             ofsSurfaces(4)@100 ofsEnd(4)
       MDC:  ident(4) version(4) name(64) flags(4) numFrames(4) numTags(4)
             numSurfaces(4)@84 numSkins(4) ofsFrames(4) ofsTagNames(4)
             ofsTags(4) ofsSurfaces(4)@104 ofsEnd(4) */
    var numSurfaces = dv.getInt32(84, true);
    var ofsSurfaces = isMDC ? dv.getInt32(104, true) : dv.getInt32(100, true);

    var surfaces = [];
    var surfOfs = ofsSurfaces;

    for (var s = 0; s < numSurfaces; s++) {
      /* Read surface name (64 bytes at surfOfs+4) */
      var nameBytes = new Uint8Array(buf, surfOfs + 4, 64);
      var surfName = '';
      for (var nb = 0; nb < 64 && nameBytes[nb] !== 0; nb++) {
        surfName += String.fromCharCode(nameBytes[nb]);
      }

      /* Surface header offsets differ between MD3 and MDC.
         MD3 surface: ident(4) name(64) flags(4) numFrames(4) numShaders(4)@76
                      numVerts(4)@80 numTris(4)@84 ofsTriangles(4)@88
                      ofsShaders(4)@92 ofsST(4)@96 ofsXyzNormals(4)@100 ofsEnd(4)@104
         MDC surface: ident(4) name(64) flags(4) numCompFrames(4) numBaseFrames(4)@76
                      numShaders(4)@80 numVerts(4)@84 numTris(4)@88
                      ofsTriangles(4)@92 ofsShaders(4)@96 ofsST(4)@100
                      ofsXyzNormals(4)@104 ofsXyzCompressed(4) ofsFrameBaseFrames(4)
                      ofsFrameCompFrames(4) ofsEnd(4)@120 */
      var numShaders, numVerts, numTris, ofsTriangles, ofsShaders, ofsTexCoords, ofsXyzNorms, ofsEnd;
      if (isMDC) {
        numShaders   = dv.getInt32(surfOfs + 80, true);
        numVerts     = dv.getInt32(surfOfs + 84, true);
        numTris      = dv.getInt32(surfOfs + 88, true);
        ofsTriangles = dv.getInt32(surfOfs + 92, true);
        ofsShaders   = dv.getInt32(surfOfs + 96, true);
        ofsTexCoords = dv.getInt32(surfOfs + 100, true);
        ofsXyzNorms  = dv.getInt32(surfOfs + 104, true);
        ofsEnd       = dv.getInt32(surfOfs + 120, true);
      } else {
        numShaders   = dv.getInt32(surfOfs + 76, true);
        numVerts     = dv.getInt32(surfOfs + 80, true);
        numTris      = dv.getInt32(surfOfs + 84, true);
        ofsTriangles = dv.getInt32(surfOfs + 88, true);
        ofsShaders   = dv.getInt32(surfOfs + 92, true);
        ofsTexCoords = dv.getInt32(surfOfs + 96, true);
        ofsXyzNorms  = dv.getInt32(surfOfs + 100, true);
        ofsEnd       = dv.getInt32(surfOfs + 104, true);
      }

      /* Read shader name (first shader entry: 64-byte name + 4-byte index) */
      var shaderName = '';
      if (numShaders > 0) {
        var shNameBytes = new Uint8Array(buf, surfOfs + ofsShaders, 64);
        for (var sn = 0; sn < 64 && shNameBytes[sn] !== 0; sn++) {
          shaderName += String.fromCharCode(shNameBytes[sn]);
        }
      }

      /* Read triangles (3 int32 indices each) */
      var triangles = [];
      for (var t = 0; t < numTris; t++) {
        var tOfs = surfOfs + ofsTriangles + t * 12;
        triangles.push(
          dv.getInt32(tOfs, true),
          dv.getInt32(tOfs + 4, true),
          dv.getInt32(tOfs + 8, true)
        );
      }

      /* Read tex coords (2 float32 each) */
      var texcoords = new Float32Array(numVerts * 2);
      for (var tc = 0; tc < numVerts; tc++) {
        var tcOfs = surfOfs + ofsTexCoords + tc * 8;
        texcoords[tc * 2]     = dv.getFloat32(tcOfs, true);
        texcoords[tc * 2 + 1] = dv.getFloat32(tcOfs + 4, true);
      }

      /* Read vertices: frame 0 only. Each vert = 3 int16 (xyz) + 1 int16 (normal packed).
         xyz are scaled by 1/64 to get world units. */
      var verts = new Float32Array(numVerts * 3);
      for (var v = 0; v < numVerts; v++) {
        var vOfs = surfOfs + ofsXyzNorms + v * 8; /* frame 0 starts at ofsXyzNorms */
        verts[v * 3]     = dv.getInt16(vOfs, true) / 64.0;
        verts[v * 3 + 1] = dv.getInt16(vOfs + 2, true) / 64.0;
        verts[v * 3 + 2] = dv.getInt16(vOfs + 4, true) / 64.0;
      }

      surfaces.push({
        name: surfName,
        shaderName: shaderName,
        verts: verts,
        texcoords: texcoords,
        triangles: triangles,
        numVerts: numVerts,
        numTris: numTris
      });

      surfOfs += ofsEnd;
    }

    return { surfaces: surfaces };
  }

  /**
   * Build GL draw data for a parsed MD3 model placed at a specific entity position/angle.
   * Returns array of { vbo, ibo, count, idxType, tex } or null on failure.
   */
  BSPViewer.prototype._buildMD3DrawCalls = function (md3, origin, angle, scale) {
    var gl = this.gl;
    if (!md3 || !md3.surfaces || !md3.surfaces.length) return null;

    /* Parse entity origin "x y z" and angle. */
    var parts = String(origin).split(/\s+/);
    var q3x = parseFloat(parts[0]) || 0;
    var q3y = parseFloat(parts[1]) || 0;
    var q3z = parseFloat(parts[2]) || 0;
    /* Q3 → GL coordinate transform: (x, z, -y) */
    var ox =  q3x;
    var oy =  q3z;
    var oz = -q3y;

    var ang = (parseFloat(angle) || 0) * Math.PI / 180.0;
    var cosA = Math.cos(ang);
    var sinA = Math.sin(ang);
    var sc = parseFloat(scale) || 1.0;

    var calls = [];
    for (var s = 0; s < md3.surfaces.length; s++) {
      var surf = md3.surfaces[s];
      if (!surf.numVerts || !surf.numTris) continue;

      /* Build interleaved vertex buffer: pos(3) + tex(2) + lm(2) + col(4) = 11 floats
         This matches the world shader vertex layout exactly. */
      var stride = 11;
      var vData = new Float32Array(surf.numVerts * stride);
      for (var v = 0; v < surf.numVerts; v++) {
        /* MD3 vertices are in Q3 model space; transform by entity angle then translate. */
        var mx = surf.verts[v * 3]     * sc;
        var my = surf.verts[v * 3 + 1] * sc;
        var mz = surf.verts[v * 3 + 2] * sc;

        /* Rotate around Z axis in Q3 space (yaw), then convert to GL. */
        var rx = mx * cosA - my * sinA;
        var ry = mx * sinA + my * cosA;
        var rz = mz;

        /* Q3 model → GL world: apply (x, z, -y) and add entity offset. */
        var wx = ox + rx;
        var wy = oy + rz;
        var wz = oz - ry;

        var off = v * stride;
        vData[off]     = wx;
        vData[off + 1] = wy;
        vData[off + 2] = wz;
        /* Texture coordinates */
        vData[off + 3] = surf.texcoords[v * 2];
        vData[off + 4] = surf.texcoords[v * 2 + 1];
        /* LM coords (unused for models – default 0,0) */
        vData[off + 5] = 0;
        vData[off + 6] = 0;
        /* Vertex color: white, full alpha (model uses texture only) */
        vData[off + 7]  = 1.0;
        vData[off + 8]  = 1.0;
        vData[off + 9]  = 1.0;
        vData[off + 10] = 1.0;
      }

      /* Index buffer */
      var useUint32 = surf.numVerts > 65535;
      var iData = useUint32
        ? new Uint32Array(surf.triangles)
        : new Uint16Array(surf.triangles);

      var vbo = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
      gl.bufferData(gl.ARRAY_BUFFER, vData, gl.STATIC_DRAW);

      var ibo = gl.createBuffer();
      gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
      gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, iData, gl.STATIC_DRAW);

      calls.push({
        vbo: vbo,
        ibo: ibo,
        count: surf.triangles.length,
        idxType: useUint32 ? gl.UNSIGNED_INT : gl.UNSIGNED_SHORT,
        tex: null,  /* will be resolved later */
        shaderName: surf.shaderName,
        hasBase: 0.0,
        hasLM: 0.0
      });
    }
    return calls.length ? calls : null;
  };

  /**
   * Load all external models referenced by BSP entities.
   * Fetches .md3/.mdc files, parses them, and creates draw calls.
   */
  BSPViewer.prototype._loadExternalModels = function (bsp) {
    var self = this;
    if (!bsp || !bsp.entities || !bsp.entities.length) return;
    this._md3Loading = true;

    /* Collect unique model paths and their entities. */
    var modelEnts = {};  /* path → [{origin, angle}] */
    for (var i = 0; i < bsp.entities.length; i++) {
      var ent = bsp.entities[i];
      if (!ent || !ent.origin) continue;
      var modelPath = ent.model || '';
      if (!modelPath) continue;
      if (modelPath.charAt(0) === '*') continue; /* BSP inline model */

      var cn = String(ent.classname || '').toLowerCase();
      var isModelClass = false;
      for (var mc = 0; mc < MODEL_ENTITY_CLASSES.length; mc++) {
        if (cn === MODEL_ENTITY_CLASSES[mc]) { isModelClass = true; break; }
      }
      var hasModelExt = /\.(md3|mdc|mdx|mds|mdm)$/i.test(modelPath);
      if (!isModelClass && !hasModelExt) continue;

      if (!modelEnts[modelPath]) modelEnts[modelPath] = [];
      modelEnts[modelPath].push({
        origin: ent.origin,
        angle: ent.angle || ent.angles || '0',
        scale: ent.modelscale || ent.modelscale_vec || '1'
      });
    }

    var paths = Object.keys(modelEnts);
    if (!paths.length) { this._md3Loading = false; return; }

    /* Build candidate URLs for each model path using the same roots as textures. */
    var roots = [];
    if (this._bspName) {
      roots.push(LOCAL_BSP_ROOT + this._bspName + '/');
    }
    roots.push(
      LOCAL_BSP_ROOT + 'etmain/',
      REMOTE_ETMAIN
    );

    var pending = paths.length;
    var cb = this._cacheBust ? '?_v=' + this._cacheBust : '';

    function done() {
      pending--;
      if (pending <= 0) {
        self._md3Loading = false;
      }
    }

    for (var p = 0; p < paths.length; p++) {
      (function (modelPath) {
        var placements = modelEnts[modelPath];
        /* Normalise path: strip leading "models/" if present, since roots already include it. */
        var relPath = modelPath;

        /* Try each root prefix. */
        var urls = ['/api/model?map=' + encodeURIComponent(self._bspName || '') + '&name=' + encodeURIComponent(relPath)];

        self._fetchModelBinary(urls, 0, function (arrayBuf) {
          if (!arrayBuf) { done(); return; }

          var md3 = parseMD3(arrayBuf);
          if (!md3) { done(); return; }

          /* Create draw calls for each placement of this model. */
          for (var pl = 0; pl < placements.length; pl++) {
            var drawCalls = self._buildMD3DrawCalls(md3, placements[pl].origin, placements[pl].angle, placements[pl].scale);
            if (drawCalls) {
              for (var dc = 0; dc < drawCalls.length; dc++) {
                var call = drawCalls[dc];
                /* Resolve texture for this model surface. */
                self._loadModelTexture(call);
                self._md3Models.push(call);
              }
            }
          }
          done();
        });
      })(paths[p]);
    }
  };

  /**
   * Fetch a binary file from a list of candidate URLs (tries each in sequence).
   */
  BSPViewer.prototype._fetchModelBinary = function (urls, idx, callback) {
    if (idx >= urls.length) { callback(null); return; }
    var self = this;
    var xhr = new XMLHttpRequest();
    xhr.open('GET', urls[idx], true);
    xhr.responseType = 'arraybuffer';
    xhr.onload = function () {
      if (xhr.status >= 200 && xhr.status < 300 && xhr.response && xhr.response.byteLength > 0) {
        callback(xhr.response);
      } else {
        self._fetchModelBinary(urls, idx + 1, callback);
      }
    };
    xhr.onerror = function () {
      self._fetchModelBinary(urls, idx + 1, callback);
    };
    xhr.send();
  };

  /**
   * Load texture for a model draw call using the same texture resolution system
   * as the BSP world geometry (manifest lookup, shader definitions, prefix
   * stripping, and full candidate URL generation).
   */
  BSPViewer.prototype._loadModelTexture = function (dc) {
    var self = this;
    if (!dc.shaderName) return;

    /* The shader name in MD3 files is typically a full path like
       "models/mapobjects/light/bel_lamp_2k" (without extension). */
    var texName = dc.shaderName.replace(/\\/g, '/');
    /* Strip any file extension that may be present. */
    texName = texName.replace(/\.(tga|jpg|png|jpeg|bmp)$/i, '');

    /* Use the same full shader/material resolution pipeline as BSP world
       textures – this consults the manifest, shader definitions, and
       generates all necessary candidate URLs with prefix stripping. */
    this._loadShaderMaterial(texName).then(function (mat) {
      if (mat && mat.tex) {
        dc.tex = mat.tex;
        dc.hasBase = 1.0;
      }
    });
  };

  /** Save / load camera bookmarks (slot is any string – typically "1".."5"). */
  BSPViewer.prototype.saveBookmark = function (slot) {
    if (!this._bookmarksKey) return false;
    var s = String(slot);
    this._bookmarks[s] = {
      pos:   [this._cam.pos[0], this._cam.pos[1], this._cam.pos[2]],
      yaw:   this._cam.yaw,
      pitch: this._cam.pitch
    };
    try {
      localStorage.setItem(this._bookmarksKey, JSON.stringify(this._bookmarks));
    } catch (e) { /* private mode / quota – tolerate */ }
    return true;
  };
  BSPViewer.prototype.loadBookmark = function (slot) {
    var bm = this._bookmarks && this._bookmarks[String(slot)];
    if (!bm || !bm.pos) return false;
    this._cam.pos   = [bm.pos[0], bm.pos[1], bm.pos[2]];
    this._cam.yaw   = +bm.yaw   || 0;
    this._cam.pitch = +bm.pitch || 0;
    /* Re-sync FPS ground reference so the player doesn't jolt on landing. */
    this._fpsVY = 0;
    this._fpsStepOffset = 0;
    this._fpsGround = this._cam.pos[1];
    return true;
  };
  BSPViewer.prototype.hasBookmark = function (slot) {
    return !!(this._bookmarks && this._bookmarks[String(slot)]);
  };

  /** Return per-map statistics, or null until a BSP is loaded. */
  BSPViewer.prototype.getStats = function () {
    return this._stats ? {
      faces:     this._stats.faces,
      vertices:  this._stats.vertices,
      textures:  this._stats.textures,
      lightmaps: this._stats.lightmaps,
      entities:  this._stats.entities,
      batches:   this._stats.batches,
      drawCalls: this._drawCalls.length,
      skyFaces:  this._skyFaceCount || 0,
      hasSky:    !!(this._skyCubeTex || this._hasSkyFaces)
    } : null;
  };

  /**
   * Capture the current canvas to a PNG data URL.
   * WebGL clears the drawing buffer after compositing (we don't ask for
   * preserveDrawingBuffer to save memory bandwidth), so we MUST read the
   * pixels in the same tick as the draw. Returns a Promise resolving to a
   * PNG data URL, or null if reading the canvas fails (e.g. tainted).
   */
  BSPViewer.prototype.takeScreenshot = function () {
    var self = this;
    return new Promise(function (resolve) {
      var doCapture = function () {
        self._draw();
        try { resolve(self.canvas.toDataURL('image/png')); }
        catch (e) { resolve(null); }
      };
      if (typeof requestAnimationFrame === 'function') {
        requestAnimationFrame(doCapture);
      } else {
        doCapture();
      }
    });
  };

  /* Extract six view-frustum planes from a column-major MVP matrix. Each plane
   * is [a, b, c, d] such that a point is inside when a*x + b*y + c*z + d >= 0
   * (after re-normalisation).  Standard Gribb/Hartmann method. */
  function frustumFromMVP(m) {
    function plane(a, b, c, d) {
      var l = Math.sqrt(a*a + b*b + c*c) || 1;
      return [a/l, b/l, c/l, d/l];
    }
    /* m[col*4+row] indexing */
    return [
      plane(m[3]+m[0],  m[7]+m[4],  m[11]+m[8],  m[15]+m[12]), /* left   */
      plane(m[3]-m[0],  m[7]-m[4],  m[11]-m[8],  m[15]-m[12]), /* right  */
      plane(m[3]+m[1],  m[7]+m[5],  m[11]+m[9],  m[15]+m[13]), /* bottom */
      plane(m[3]-m[1],  m[7]-m[5],  m[11]-m[9],  m[15]-m[13]), /* top    */
      plane(m[3]+m[2],  m[7]+m[6],  m[11]+m[10], m[15]+m[14]), /* near   */
      plane(m[3]-m[2],  m[7]-m[6],  m[11]-m[10], m[15]-m[14])  /* far    */
    ];
  }

  /* AABB vs frustum test: returns true if the box may still be visible. */
  function aabbInFrustum(planes, mn, mx) {
    for (var i = 0; i < 6; i++) {
      var p = planes[i];
      /* Pick the AABB corner furthest along the plane normal ("p-vertex"). */
      var px = p[0] >= 0 ? mx[0] : mn[0];
      var py = p[1] >= 0 ? mx[1] : mn[1];
      var pz = p[2] >= 0 ? mx[2] : mn[2];
      if (p[0]*px + p[1]*py + p[2]*pz + p[3] < 0) return false;
    }
    return true;
  }

  BSPViewer.prototype._draw = function () {
    var gl = this.gl;
    var c  = this.canvas;

    /* Keep canvas pixel size in sync with its CSS layout size */
    var w = c.clientWidth  | 0;
    var h = c.clientHeight | 0;
    if (c.width !== w || c.height !== h) {
      c.width  = w;
      c.height = h;
      gl.viewport(0, 0, w, h);
    }

    // World batches are deliberately two-sided (see _initGL). Debug/model
    // overlays must not leave face culling enabled for the following frame.
    gl.disable(gl.CULL_FACE);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    if (!this._drawCalls.length && !this._skyCubeTex && !this._hasSkyFaces && !this._md3Models.length && !this._navigation) return;

    var aspect = w / Math.max(1, h);
    var proj   = matPerspective(Math.PI / 3.0, aspect, 1.0, 80000.0);
    var view   = this._viewMatrix();
    var mvp    = matMul(proj, view);
    var planes = frustumFromMVP(mvp);
    this._navigationMVP = mvp;
    if (this._navigationOptions && this._navigationOptions.bsp === false) {
      this._drawNavigation(mvp);
      return;
    }

    /* Skybox first – depth writes disabled, sits at the far plane. */
    this._drawSky(proj, view);

    if (!this._drawCalls.length) { this._drawNavigation(mvp); return; }

    /* Monotonic per-viewer time in seconds, used for tcMod scroll/rotate. */
    if (!this._t0) this._t0 = performance.now();
    var nowSec = (performance.now() - this._t0) * 0.001;

    gl.useProgram(this._prog);
    gl.uniformMatrix4fv(this._loc.uMVP, false, mvp);
    if (this._loc.uView) gl.uniformMatrix4fv(this._loc.uView, false, view);
    gl.uniform1i(this._loc.uBaseTex, 1);
    gl.uniform1i(this._loc.uLMTex, 0);
    gl.uniform1f(this._loc.uBrightness, this._brightness);
    if (this._loc.uGamma) gl.uniform1f(this._loc.uGamma, this._gamma || 1.0);
    if (this._loc.uFogDensity) gl.uniform1f(this._loc.uFogDensity, this._fogDensity || 0);
    if (this._loc.uFogColor)   gl.uniform3f(this._loc.uFogColor,
                                            this._bgColor[0], this._bgColor[1], this._bgColor[2]);
    if (this._loc.uTime) gl.uniform1f(this._loc.uTime, nowSec);

    var STRIDE_B = 11 * 4; /* 11 floats × 4 bytes */
    /* Wireframe is now an overlay pass on top of the textured map, so the
       map stays visible. The first pass always renders triangles normally;
       a second pass draws line buffers in pure white when enabled. */
    var wire     = false;
    if (this._loc.uWireOverlay) gl.uniform1f(this._loc.uWireOverlay, 0.0);

    function drawCall(dc) {
      gl.bindBuffer(gl.ARRAY_BUFFER,         dc.vbo);
      gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, wire ? dc.lineIbo : dc.ibo);

      gl.enableVertexAttribArray(this._loc.aPos);
      gl.enableVertexAttribArray(this._loc.aTex);
      gl.enableVertexAttribArray(this._loc.aLM);
      gl.enableVertexAttribArray(this._loc.aCol);

      gl.vertexAttribPointer(this._loc.aPos, 3, gl.FLOAT, false, STRIDE_B, 0);
      gl.vertexAttribPointer(this._loc.aTex, 2, gl.FLOAT, false, STRIDE_B, 3*4);
      gl.vertexAttribPointer(this._loc.aLM,  2, gl.FLOAT, false, STRIDE_B, 5*4);
      gl.vertexAttribPointer(this._loc.aCol, 4, gl.FLOAT, false, STRIDE_B, 7*4);

      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, dc.lmTex);
      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, dc.baseTex);
      gl.uniform1f(this._loc.uHasBase, dc.hasBase);
      gl.uniform1f(this._loc.uHasLM, dc.hasLM);
      if (this._loc.uTcScroll) gl.uniform2f(this._loc.uTcScroll, dc.tcScrollS || 0, dc.tcScrollT || 0);
      if (this._loc.uTcRotate) gl.uniform1f(this._loc.uTcRotate, dc.tcRotate || 0);

      if (wire) {
        gl.drawElements(gl.LINES, dc.lineCount, dc.idxType, 0);
      } else {
        gl.drawElements(gl.TRIANGLES, dc.count, dc.idxType, 0);
      }
    }

    /* View-frustum culling: skip batches whose AABB lies entirely outside the
     * frustum. The AABB test is conservative but very cheap, and cuts draw
     * calls dramatically on large outdoor maps. */
    gl.disable(gl.BLEND);
    gl.depthMask(true);
    for (var i = 0; i < this._drawCalls.length; i++) {
      var dc = this._drawCalls[i];
      if (dc.transparent) continue;
      if (dc.aabbMin && !aabbInFrustum(planes, dc.aabbMin, dc.aabbMax)) continue;
      drawCall.call(this, dc);
    }

    gl.enable(gl.BLEND);
    gl.depthMask(false);
    for (var j = 0; j < this._drawCalls.length; j++) {
      var dt = this._drawCalls[j];
      if (!dt.transparent) continue;
      if (dt.aabbMin && !aabbInFrustum(planes, dt.aabbMin, dt.aabbMax)) continue;
      if (dt.additive) {
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
      } else {
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
      }
      drawCall.call(this, dt);
    }
    gl.depthMask(true);
    gl.disable(gl.BLEND);

    /* ── Render loaded MD3/MDC models ──────────────────────────────────── */
    if (this._md3Models.length > 0) {
      gl.useProgram(this._prog);
      gl.uniformMatrix4fv(this._loc.uMVP, false, mvp);
      if (this._loc.uView) gl.uniformMatrix4fv(this._loc.uView, false, view);
      gl.uniform1f(this._loc.uBrightness, this._brightness);
      if (this._loc.uGamma) gl.uniform1f(this._loc.uGamma, this._gamma || 1.0);
      if (this._loc.uFogDensity) gl.uniform1f(this._loc.uFogDensity, this._fogDensity || 0);
      if (this._loc.uTcScroll) gl.uniform2f(this._loc.uTcScroll, 0, 0);
      if (this._loc.uTcRotate) gl.uniform1f(this._loc.uTcRotate, 0);
      if (this._loc.uWireOverlay) gl.uniform1f(this._loc.uWireOverlay, 0.0);

      for (var mi = 0; mi < this._md3Models.length; mi++) {
        var mdl = this._md3Models[mi];
        if (!mdl.count) continue;

        gl.bindBuffer(gl.ARRAY_BUFFER, mdl.vbo);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, mdl.ibo);

        gl.enableVertexAttribArray(this._loc.aPos);
        gl.enableVertexAttribArray(this._loc.aTex);
        gl.enableVertexAttribArray(this._loc.aLM);
        gl.enableVertexAttribArray(this._loc.aCol);

        gl.vertexAttribPointer(this._loc.aPos, 3, gl.FLOAT, false, STRIDE_B, 0);
        gl.vertexAttribPointer(this._loc.aTex, 2, gl.FLOAT, false, STRIDE_B, 3*4);
        gl.vertexAttribPointer(this._loc.aLM,  2, gl.FLOAT, false, STRIDE_B, 5*4);
        gl.vertexAttribPointer(this._loc.aCol, 4, gl.FLOAT, false, STRIDE_B, 7*4);

        /* Bind model texture if available, otherwise use white. */
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, this._whiteTex || null);
        gl.activeTexture(gl.TEXTURE1);
        if (mdl.tex) {
          gl.bindTexture(gl.TEXTURE_2D, mdl.tex);
        } else {
          gl.bindTexture(gl.TEXTURE_2D, this._whiteTex || null);
        }
        gl.uniform1f(this._loc.uHasBase, mdl.hasBase);
        gl.uniform1f(this._loc.uHasLM, 0.0);

        gl.drawElements(gl.TRIANGLES, mdl.count, mdl.idxType, 0);
      }
    }

    /* Wireframe overlay: when enabled, draw white triangle-edges on top of
       the textured map so both are visible. Uses the same VAO setup and
       polygon-offset trickery is unnecessary because we draw lines with the
       same depth values (gl.LEQUAL ensures they win against equal-depth
       triangles drawn in pass 1). */
    if (this._wireframe && this._loc.uWireOverlay) {
      gl.uniform1f(this._loc.uWireOverlay, 1.0);
      gl.depthFunc(gl.LEQUAL);
      wire = true;
      for (var w1 = 0; w1 < this._drawCalls.length; w1++) {
        var dw = this._drawCalls[w1];
        if (dw.transparent) continue; /* skip alpha surfaces – avoids clutter */
        if (dw.aabbMin && !aabbInFrustum(planes, dw.aabbMin, dw.aabbMax)) continue;
        drawCall.call(this, dw);
      }
      wire = false;
      gl.uniform1f(this._loc.uWireOverlay, 0.0);
      gl.depthFunc(gl.LESS);
    }

    /* Spawn-point overlay: tiny solid-color line program drawn last so the
       pillars are not occluded by transparent surfaces' depth-write. */
    if (this._spawnMarkers && this._spawnVbo && this._spawnVertexCount > 0) {
      gl.useProgram(this._spawnProg);
      gl.uniformMatrix4fv(this._spawnLoc.uMVP, false, mvp);
      gl.bindBuffer(gl.ARRAY_BUFFER, this._spawnVbo);
      gl.enableVertexAttribArray(this._spawnLoc.aPos);
      gl.enableVertexAttribArray(this._spawnLoc.aCol);
      var SSTRIDE = 6 * 4;
      gl.vertexAttribPointer(this._spawnLoc.aPos, 3, gl.FLOAT, false, SSTRIDE, 0);
      gl.vertexAttribPointer(this._spawnLoc.aCol, 3, gl.FLOAT, false, SSTRIDE, 3 * 4);
      gl.drawArrays(gl.LINES, 0, this._spawnVertexCount);
      gl.disableVertexAttribArray(this._spawnLoc.aPos);
      gl.disableVertexAttribArray(this._spawnLoc.aCol);
      /* Restore the world program so caller-state expectations hold. */
      gl.useProgram(this._prog);
    }

    /* Missing-model marker overlay: RGB triangles at entity positions. */
    if (this._modelMarkers && this._modelVbo && this._modelVertexCount > 0) {
      gl.useProgram(this._spawnProg);
      gl.uniformMatrix4fv(this._spawnLoc.uMVP, false, mvp);
      gl.bindBuffer(gl.ARRAY_BUFFER, this._modelVbo);
      gl.enableVertexAttribArray(this._spawnLoc.aPos);
      gl.enableVertexAttribArray(this._spawnLoc.aCol);
      var MSTRIDE = 6 * 4;
      gl.vertexAttribPointer(this._spawnLoc.aPos, 3, gl.FLOAT, false, MSTRIDE, 0);
      gl.vertexAttribPointer(this._spawnLoc.aCol, 3, gl.FLOAT, false, MSTRIDE, 3 * 4);
      gl.disable(gl.CULL_FACE);
      gl.drawArrays(gl.TRIANGLES, 0, this._modelVertexCount);
      gl.enable(gl.CULL_FACE);
      gl.disableVertexAttribArray(this._spawnLoc.aPos);
      gl.disableVertexAttribArray(this._spawnLoc.aCol);
      gl.useProgram(this._prog);
    }
    this._drawNavigation(mvp);
    this._drawNavigationText();
  };

  /* ─────────────────────────── EXPORT ────────────────────────────────── */
  /* Navigation data is supplied by TCE2's exporter/native validator. Browser
     collision is only a visual aid, never a replacement for that validation. */
  BSPViewer.prototype.setNavigation = function (data) {
    var previous = this._navigationSelection;
    this._disposeNavigation();
    this._navigation = data || {nodes: [], edges: [], meshes: {}};
    this._navigationOptions = Object.assign({bsp: true, stand: false, duck: false,
      collision: false, nodes: true, accepted: true, rejected: false, goals: true, labels: true,
      edges: 'all', style: 'ingame', maxDistance: 2000, xray: false, minZ: -Infinity, maxZ: Infinity}, this._navigationOptions || {});
    this._navigationSelection = null;
    this._navigationNodeMap = {};
    var self = this;
    (this._navigation.nodes || []).forEach(function (node, index) {
      node._navIndex = index;
      node._navId = node.id != null ? node.id : index;
      self._navigationNodeMap[String(node._navId)] = node;
    });
    // Preserve inspection while reloading meshes or a fresh report. Edge array
    // order can change; recover it by endpoints rather than the old index.
    if (previous && previous.kind === 'node') {
      var restored = this._navigationNodeMap[String(previous.id)];
      if (restored) this._navigationSelection = {kind: 'node', id: previous.id, item: restored};
    } else if (previous && previous.kind === 'edge') {
      var edgeIndex = (this._navigation.edges || []).findIndex(function (edge) {
        return String(edge.from) === String(previous.item.from) && String(edge.to) === String(previous.item.to);
      });
      if (edgeIndex >= 0) this._navigationSelection = {kind: 'edge', id: edgeIndex, item: this._navigation.edges[edgeIndex]};
    } else if (previous && previous.kind === 'pair') {
      var pair = (this._navigation.pairs || []).find(function (p) {
        return String(p.from) === String(previous.item.from) && String(p.to) === String(previous.item.to);
      });
      if (pair) this._navigationSelection = {kind: 'pair', id: previous.id, item: pair};
    } else if(previous?.kind==='goal'){
      var goal=this._navigationGoalMap?.[String(previous.id)];
      if(goal)this._navigationSelection={kind:'goal',id:previous.id,item:goal};
    }
    this._rebuildNavigation();
    return this;
  };

  BSPViewer.prototype.setNavigationOptions = function (options) {
    this._navigationOptions = Object.assign(this._navigationOptions || {}, options || {});
    this._rebuildNavigation();
    return this;
  };

  BSPViewer.prototype.setNavigationInspect = function (inspect) {
    this._navigationInspect = !!inspect;
    this._keys = {};
    if (inspect && document.pointerLockElement === this.canvas) document.exitPointerLock();
    return this;
  };

  BSPViewer.prototype.setAssetMap = function (map) { this._navigationMap = String(map || ''); };

  BSPViewer.prototype.getCameraState = function () {
    return {position: glToQ3(this._cam.pos), yaw: this._cam.yaw, pitch: this._cam.pitch,
      mode: this._navigationInspect === false ? 'fly' : 'inspect'};
  };

  BSPViewer.prototype.focusNavigation = function (target) {
    var p;
    if (Array.isArray(target)) p = target;
    else {
      var node = this._navigationNodeMap && this._navigationNodeMap[String(target)];
      p = node && (node.position || node.pos);
    }
    if (!p) {
      var nodes = (this._navigation || {}).nodes || [];
      if (!nodes.length) return false;
      var reachable = nodes.filter(function (node) { return navAccepted(node) && node.spawn_reachable !== false; });
      var pool = reachable.length ? reachable : nodes.filter(navAccepted);
      if (!pool.length) pool = nodes;
      p = navPosition(pool[Math.floor(pool.length / 2)]);
    }
    if (!p || !p.every(Number.isFinite)) return false;
    var look = q3ToGLVec(p);
    this._cam.pos = [look[0], look[1] + 220, look[2] + 300];
    this._cam.yaw = 0;
    this._cam.pitch = -Math.atan2(220, 300);
    this._fps = false;
    this._fpsStepOffset = 0;
    return true;
  };

  BSPViewer.prototype.enterNavigation = function (target) {
    var map = this._navigationNodeMap || {}, node = map[String(target)];
    var nodes = (this._navigation || {}).nodes || [];
    var links = {}, neighbor = null;
    function standing(n) { return n && navAccepted(n) && n.spawn_reachable !== false && !n.crouch && (navFlagBits(n.nav_flags)&(64n|512n|134217728n))===0n; }
    ((this._navigation || {}).edges || []).forEach(function (edge) {
      var from = map[String(edge.from)], to = map[String(edge.to)];
      if (!navAccepted(edge) || !standing(from) || !standing(to) || edge.crouch) return;
      var a = navPosition(from), b = navPosition(to);
      if (!a || !b || Math.abs(a[2]-b[2]) > 18.25) return;
      var length = Math.hypot(b[0]-a[0], b[1]-a[1]);
      if (length < 48) return;
      (links[String(edge.from)] || (links[String(edge.from)] = [])).push({node: to, length: length});
    });
    if (!node) {
      // File order starts at the map perimeter on many exports. Prefer a
      // central standing point on the principal floor, with a proven forward
      // walking segment, rather than looking outward from that first sample.
      var pool = nodes.filter(function (n) { return standing(n) && links[String(n._navId)]; });
      if (!pool.length) pool = nodes.filter(standing);
      if (pool.length) {
        function median(axis) { var values = pool.map(function (n) { return navPosition(n)[axis]; }).sort(function (a, b) { return a-b; }); return values[Math.floor(values.length/2)]; }
        var cx = median(0), cy = median(1), floors = {};
        pool.forEach(function (n) { var floor = Math.round(navPosition(n)[2]/16); floors[floor] = (floors[floor] || 0)+1; });
        var floorZ = Number(Object.keys(floors).sort(function (a, b) { return floors[b]-floors[a]; })[0])*16;
        var best = Infinity;
        pool.forEach(function (candidate) {
          var p = navPosition(candidate), degree = (links[String(candidate._navId)] || []).length;
          var score = Math.hypot(p[0]-cx, p[1]-cy) + Math.abs(p[2]-floorZ)*4 - Math.min(degree, 4)*24;
          if (score < best) { node = candidate; best = score; }
        });
      }
    }
    if (!node) node = nodes.find(function (n) { return navAccepted(n) && n.spawn_reachable !== false; });
    var p = navPosition(node);
    if (!p) return false;
    this._cam.pos = q3ToGLVec([p[0], p[1], p[2] + (node.crouch ? 42 : 64)]);
    var choices = links[String(node._navId)] || [];
    if (choices.length) neighbor = navPosition(choices.sort(function (a, b) { return b.length-a.length; })[0].node);
    else {
      var edge = ((this._navigation || {}).edges || []).find(function (e) { return navAccepted(e) && String(e.from) === String(node._navId); });
      neighbor = edge && navPosition(map[String(edge.to)]);
    }
    if (neighbor) this._cam.yaw = Math.atan2(neighbor[0]-p[0], neighbor[1]-p[1]);
    this._cam.pitch = -0.16;
    // Eye-height inspection, not a promise of engine-accurate walking physics.
    this._fps = false; this._fpsStepOffset = 0;
    return true;
  };

  BSPViewer.prototype.selectNavigation = function (kind, id) {
    var nav = this._navigation || {}, item = null;
    if (kind === 'node') item = this._navigationNodeMap[String(id)] || null;
    else if (kind === 'edge') item = (nav.edges || [])[Number(id)] || null;
    else if (kind === 'spawn') item = (this._navigationSpawnMap || {})[String(id)] || null;
    else if (kind === 'goal') item = (this._navigationGoalMap || {})[String(id)] || null;
    this._navigationSelection = item ? {kind: kind, id: id, item: item} : null;
    this._rebuildNavigation();
    if (this.onNavigationSelect) this.onNavigationSelect(this._navigationSelection);
    return this._navigationSelection;
  };

  BSPViewer.prototype.selectNavigationPair = function (pair) {
    this._navigationSelection = pair ? {kind: 'pair', id: pair.from + '-' + pair.to, item: pair} : null;
    this._rebuildNavigation();
    if (this.onNavigationSelect) this.onNavigationSelect(this._navigationSelection);
    return this._navigationSelection;
  };

  function navPosition(node) { return node && (node.position || node.pos); }
  function navFlagBits(value){try{return BigInt(value||'0');}catch(_){return 0n;}}
  BSPViewer.prototype.setNavigationGoals = function (overlay) {
    this._navigationGoalOverlay=overlay||{goals:[]};this._navigationGoalMap={};
    (this._navigationGoalOverlay.goals||[]).forEach(goal=>{this._navigationGoalMap[String(goal.id)]=goal;});
    if(this._navigationSelection?.kind==='goal'){
      var item=this._navigationGoalMap[String(this._navigationSelection.id)];
      this._navigationSelection=item?{kind:'goal',id:item.id,item:item}:null;
    }
    this._rebuildNavigation();return this;
  };
  BSPViewer.prototype.navigationGoalMarkerPosition = function (goal) {
    // GoalCreate uses AABB.CenterBottom, while explicit Position changes and
    // generated mission targets may use another world-space reference. Match
    // MapGoal::RenderDefault: draw at GetPosition(), never guess a body offset.
    return goal.position?goal.position.slice():null;
  };
  BSPViewer.prototype.navigationSavedGoalArrows = function (goal) {
    // mapgoal_switch.gm::Render: absolute AimPosition takes precedence over
    // the Facing direction. These are saved vectors, not current runtime aim.
    if(String(goal.type||goal.properties?.GoalType||'').toUpperCase()!=='SWITCH')return [];
    var props=goal.effective_properties||goal.properties||{},position=goal.position;
    var vector=v=>Array.isArray(v)&&v.length===3&&v.every(Number.isFinite);
    if(!vector(position))return [];
    var stanceOffset=props.Stance==='crouch'?-24:props.Stance==='prone'?-50:0;
    var from=[position[0],position[1],position[2]+64+stanceOffset],aim=props.AimPosition,facing=props.Facing;
    if(vector(aim)&&aim.some(v=>v!==0))return [{kind:'AimPosition',from:from,to:aim.slice(),color:[1,1,0]}];
    if(vector(facing)&&facing.some(v=>v!==0))return [{kind:'Facing',from:from,to:from.map((v,i)=>v+facing[i]*32),color:[0,0,1]}];
    return [];
  };
  BSPViewer.prototype.navigationLabelLines = function (kind,item) {
    if(kind==='goal'){
      var properties=item.effective_properties||item.properties||{},defaults=item.schema_defaults||{},lines=[item.name||item.id,'Saved GM · Active: live runtime only'];
      ['GroupName','Stance','Timeout','Use','Radius','MinRadius','MinCampTime','MaxCampTime','TeamAvailability','CreateOnLoad','Roles','Range','RandomUsePoint','Facing','AimPosition'].forEach(function(key){
        if(properties[key]!=null)lines.push(key+': '+(typeof properties[key]==='object'?JSON.stringify(properties[key]):properties[key])+(Object.hasOwn(defaults,key)?' (Schema default)':''));
      });return lines;
    }
    var source=item._navId??item.id,nativeIndex=Number.isInteger(item.index)&&item.index>=0?item.index:null;
    var lines=[(nativeIndex!=null?'Waypoint '+nativeIndex+' (native index), ':'')+'Source-ID '+source+', Radius '+(item.radius??0)+(item.way_guid!=null?', UID '+item.way_guid:'')];
    if(item.name)lines.push('Name: '+item.name);
    if(item.nav_flags!=null){
      var bits=navFlagBits(item.nav_flags),catalog=global.navstudioFlags?.catalog||[],known=0n;
      var flags=catalog.filter(function(f){var bit=navFlagBits(f.value);known|=bit;return(bits&bit)!==0n;}).map(f=>f.name);
      if(bits&~known)flags.push('0x'+(bits&~known).toString(16));
      if(flags.length)lines.push('Flags: '+flags.join(', '));
    }
    Object.entries(item.action_properties||{}).forEach(function(pair){lines.push(pair[0]+': '+pair[1]);});
    return lines;
  };
  BSPViewer.prototype._drawNavigationText = function () {
    var o=this._navigationOptions||{};
    if(!this._navigationTextLayer){
      var layer=document.createElement('div');layer.className='navigation-text-overlay';layer.setAttribute('aria-hidden','true');
      this.canvas.parentElement.appendChild(layer);this._navigationTextLayer=layer;
    }
    var layer=this._navigationTextLayer,rect=this.canvas.getBoundingClientRect(),parent=this.canvas.parentElement.getBoundingClientRect();
    layer.style.left=(rect.left-parent.left)+'px';layer.style.top=(rect.top-parent.top)+'px';
    layer.style.width=rect.width+'px';layer.style.height=rect.height+'px';
    if(!o.labels||!this._navigationMVP){layer.replaceChildren();delete layer.dataset.signature;return;}
    var self=this,records=[],selected=this._navigationSelection;
    if(o.nodes&&selected?.kind==='node')records.push({kind:'node',item:selected.item,position:navPosition(selected.item),lift:72});
    if(o.goals)(this._navigationGoalOverlay?.goals||[]).forEach(function(goal){
      if(!self._navigationVisible(goal,goal.position))return;
      var glPos=q3ToGLVec(goal.position),distance=Math.hypot(...glPos.map((v,i)=>v-self._cam.pos[i]));
      if(o.maxDistance>0&&distance>o.maxDistance)return;
      records.push({kind:'goal',item:goal,position:self.navigationGoalMarkerPosition(goal),lift:36,distance:distance,selected:selected?.kind==='goal'&&selected.id===goal.id});
    });
    records.sort((a,b)=>(b.selected?1:0)-(a.selected?1:0)||(a.distance||0)-(b.distance||0));
    var shown=[];
    records.slice(0,48).forEach(function(record){
      if(!self._navigationVisible(record.item,record.position))return;
      var cameraDelta=q3ToGLVec(record.position).map((v,i)=>v-self._cam.pos[i]);
      if(o.maxDistance>0&&Math.hypot(...cameraDelta)>o.maxDistance)return;
      var point=self.projectNavigationPoint(record.position,record.lift);if(!point)return;
      var x=point[0]-rect.left,y=point[1]-rect.top;if(x<0||y<0||x>rect.width||y>rect.height)return;
      // The overlay follows actual camera projection. X-ray is explicit; BSP
      // point occlusion is display-only and never a native route certificate.
      if(!o.xray&&self._cm){
        var target=q3ToGLVec([record.position[0],record.position[1],record.position[2]+record.lift]);
        var trace=self._traceBox(self._cam.pos,target,[0,0,0],[0,0,0]);
        if(trace.startsolid||trace.allsolid||trace.fraction<.995)return;
      }
      var detail=record.kind==='node'||record.selected,lines=self.navigationLabelLines(record.kind,record.item);
      if(!detail)lines=lines.slice(0,1);
      shown.push({id:record.kind+':'+record.item.id,x:x,y:y,lines:lines,kind:record.kind});
    });
    var signature=JSON.stringify(shown.map(r=>[r.id,r.lines]));
    if(layer.dataset.signature!==signature){
      layer.replaceChildren();shown.forEach(function(record){var el=document.createElement('div');el.className='navigation-label '+record.kind;el.textContent=record.lines.join('\n');layer.appendChild(el);});
      layer.dataset.signature=signature;
    }
    shown.forEach(function(record,index){var el=layer.children[index];el.style.left=record.x+'px';el.style.top=record.y+'px';});
  };
  BSPViewer.prototype.navigationEdgeLift = function () {
    // Omni-bot: WAY origin + (-24 + (40 - -24) * 0.5) = origin + 8.
    // Putting every edge at eye height collapses a flat graph to the horizon.
    return this._navigationOptions.style==='ingame'?(Number(this._navigation.origin_above_feet)||24)+8:7;
  };
  BSPViewer.prototype.setNavigationSpawns = function (spawns) {
    this._navigationSpawns = (spawns || []).map(function (s) { return Object.assign({},s,{position:s.position.slice()}); });
    this._navigationSpawnMap = {};
    var self=this;this._navigationSpawns.forEach(function(s){self._navigationSpawnMap[String(s.id)]=s;});
    if(this._navigationSelection?.kind==='spawn'){
      var item=this._navigationSpawnMap[String(this._navigationSelection.id)];
      this._navigationSelection=item?{kind:'spawn',id:item.id,item:item}:null;
    }
    this._rebuildNavigation();
  };
  BSPViewer.prototype.previewNavigationSpawn = function (id,point) {
    var spawn=(this._navigationSpawnMap||{})[String(id)];if(!spawn||!spawn.editable)return false;
    spawn.position=point.slice();this._rebuildNavigation();return true;
  };
  function navAccepted(item) {
    return item.accepted !== false && item.valid !== false && item.status !== 'rejected' && item.state !== 'rejected';
  }
  function navColor(item) {
    if (!navAccepted(item)) return [1, 0.29, 0.28];
    if (item.crouch || item.duck || item.stance === 'crouch') return [1, 0.74, 0.22];
    if (item.verified === false || item.native_checked === false || item.status === 'candidate') return [0.68, 0.57, 1];
    return [0.19, 0.87, 0.79];
  }

  BSPViewer.prototype._navigationVisible = function (item, pos) {
    var o = this._navigationOptions || {};
    return pos && pos.length === 3 && pos.every(Number.isFinite) &&
      pos[2] >= o.minZ && pos[2] <= o.maxZ &&
      (navAccepted(item) ? o.accepted !== false : o.rejected === true);
  };

  BSPViewer.prototype._navigationEdgeNodes = function (edge) {
    var map = this._navigationNodeMap || {};
    return [map[String(edge.from != null ? edge.from : edge.source)],
      map[String(edge.to != null ? edge.to : edge.target)]];
  };

  BSPViewer.prototype._disposeNavigation = function () {
    var gl = this.gl, freed = new Set();
    var buffers = (this._navigationBuffers || []).slice();
    Object.values(this._navigationMeshCache || {}).forEach(function (entry) { buffers = buffers.concat(entry.buffers); });
    buffers.forEach(function (b) { if (!freed.has(b.buffer)) { gl.deleteBuffer(b.buffer); freed.add(b.buffer); } });
    this._navigationBuffers = [];
    this._navigationMeshCache = {};
  };

  BSPViewer.prototype._navigationBuffer = function (values, primitive, alpha, pointSize) {
    if (!values.length) return;
    var gl = this.gl, buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(values), gl.STATIC_DRAW);
    this._navigationBuffers.push({buffer: buffer, count: values.length / 6,
      primitive: primitive, alpha: alpha, pointSize: pointSize || 1});
  };

  BSPViewer.prototype.navigationPolygonTriangles = function (mesh,poly) {
    function valid(point){return Array.isArray(point)&&point.length===3&&point.every(Number.isFinite);}
    if(poly && !Array.isArray(poly) && Array.isArray(poly.detail_triangles)){
      // Recast's interior heights are not a fan interpolation of the boundary.
      return poly.detail_triangles.filter(function(t){return Array.isArray(t)&&t.length===3&&t.every(valid);});
    }
    var ids=Array.isArray(poly)?poly:poly?.vertices;
    if(!ids||ids.length<3)return[];
    var pts=ids.map(function(i){return mesh.vertices[i];});if(!pts.every(valid))return[];
    var result=[];for(var t=1;t<pts.length-1;t++)result.push([pts[0],pts[t],pts[t+1]]);
    return result;
  };
  BSPViewer.prototype._rebuildNavigation = function () {
    // Selections only rebuild the small graph. Keep potentially large collision
    // and Recast VBOs until the mesh or height slice changes.
    var targetGL = this.gl;
    (this._navigationBuffers || []).forEach(function (b) { if (!b.mesh) targetGL.deleteBuffer(b.buffer); });
    this._navigationBuffers = [];
    this._navigationMeshCache = this._navigationMeshCache || {};
    if (!this._navigation) return;
    var self = this, gl = this.gl, o = this._navigationOptions, nav = this._navigation;
    var selected = this._navigationSelection;
    var meshes = nav.meshes || {}, lines = [], posts = [], points = [], highlight = [];
    var ingame = o.style === 'ingame', edgeLift = this.navigationEdgeLift();
    function vertex(out, p, color, lift) {
      var v = q3ToGLVec([p[0], p[1], p[2] + (lift || 0)]);
      out.push(v[0], v[1], v[2], color[0], color[1], color[2]);
    }
    ['collision', 'stand', 'duck'].forEach(function (kind) {
      var mesh = meshes[kind];
      if (!o[kind] || !mesh || !mesh.vertices) return;
      var cached = self._navigationMeshCache[kind];
      if (cached && cached.mesh === mesh && cached.minZ === o.minZ && cached.maxZ === o.maxZ) {
        self._navigationBuffers = self._navigationBuffers.concat(cached.buffers);
        return;
      }
      if (cached) cached.buffers.forEach(function (item) { gl.deleteBuffer(item.buffer); });
      var bufferStart = self._navigationBuffers.length;
      var tris = [], borders = [], color = kind === 'stand' ? [0.15, 0.72, 0.68] : kind === 'duck' ? [1, 0.67, 0.22] : [0.65, 0.70, 0.8];
      var polyList = mesh.polygons || mesh.triangles || [];
      polyList.forEach(function (poly) {
        var ids = Array.isArray(poly) ? poly : poly.vertices;
        if (!ids || ids.length < 3) return;
        var pts = ids.map(function (idx) { return mesh.vertices[idx]; });
        if (pts.some(function (p) { return !p || !p.every(Number.isFinite); })) return;
        var triangles=self.navigationPolygonTriangles(mesh,poly),shown=false;
        triangles.forEach(function(triangle){
          var z=triangle.reduce(function(sum,p){return sum+p[2];},0)/3;
          if(z<o.minZ||z>o.maxZ)return;
          shown=true;triangle.forEach(function(p){vertex(tris,p,color,1);});
        });
        if(!shown)return;
        if (kind !== 'collision') for (var e = 0; e < pts.length; e++) {
          vertex(borders, pts[e], color, 1.5); vertex(borders, pts[(e+1) % pts.length], color, 1.5);
        }
      });
      self._navigationBuffer(tris, gl.TRIANGLES, kind === 'collision' ? 0.17 : 0.22);
      self._navigationBuffer(borders, gl.LINES, 0.52);
      var buffers = self._navigationBuffers.slice(bufferStart);
      buffers.forEach(function (item) { item.mesh = true; });
      self._navigationMeshCache[kind] = {mesh: mesh, minZ: o.minZ, maxZ: o.maxZ, buffers: buffers};
    });
    (nav.edges || []).forEach(function (edge, index) {
      var ends = self._navigationEdgeNodes(edge), a = navPosition(ends[0]), b = navPosition(ends[1]);
      if (!self._navigationVisible(edge, a) || !b || b[2] < o.minZ || b[2] > o.maxZ) return;
      var chosen = selected && (selected.kind === 'edge' && Number(selected.id) === index ||
        selected.kind === 'node' && ends.some(function (n) { return n && String(n._navId) === String(selected.id); }) ||
        selected.kind === 'pair' && ends.every(function (n) { return n && (String(n._navId) === String(selected.item.from) || String(n._navId) === String(selected.item.to)); }));
      if (o.edges === 'none' || o.edges === 'selected' && !chosen) return;
      var out = chosen ? highlight : lines, color = chosen ? [1, 0.25, 0.2] : ingame && navAccepted(edge) ? [1, 1, 1] : navColor(edge);
      vertex(out, a, color, edgeLift); vertex(out, b, color, edgeLift);
      // Short arrow at 60%: direction remains readable without the old line web.
      var dx = b[0]-a[0], dy = b[1]-a[1], len = Math.hypot(dx, dy);
      if (len > 1) {
        var tip = [a[0]+dx*0.6, a[1]+dy*0.6, a[2]+(b[2]-a[2])*0.6];
        var size = Math.min(12, len * 0.16);
        [-1, 1].forEach(function (side) {
          vertex(out, tip, color, edgeLift);
          vertex(out, [tip[0]-dx/len*size-dy/len*size*0.5*side,
            tip[1]-dy/len*size+dx/len*size*0.5*side, tip[2]], color, edgeLift);
        });
      }
    });
    (nav.nodes || []).forEach(function (node) {
      var p = navPosition(node);
      if (!o.nodes || !self._navigationVisible(node, p)) return;
      if (ingame) {
        var postColor = navAccepted(node) ? [1, 0.3, 1] : navColor(node);
        vertex(posts, p, postColor, 1); vertex(posts, p, postColor, 64);
        vertex(points, p, postColor, 64);
        if(Array.isArray(node.facing)&&node.facing.length===3&&node.facing.every(Number.isFinite)&&node.facing.some(v=>v!==0)){
          var facingLift=(Number(nav.origin_above_feet)||24)+18;
          vertex(posts,p,[.2,.45,1],facingLift);vertex(posts,p.map((v,i)=>v+node.facing[i]*32),[.2,.45,1],facingLift);
        }
      } else vertex(points, p, navColor(node), 7);
      if (selected && selected.kind === 'node' && String(selected.id) === String(node._navId)) {
        var radius = Math.max(1, Number(node.radius) || 12), color = ingame ? [1, 0.2, 0.15] : [1, 1, 0.65];
        for (var i = 0; i < (ingame?7:24); i++) {
          var u = i*Math.PI/12, v = (i+1)*Math.PI/12;
          if(ingame){u=i*Math.PI/7;vertex(highlight,[p[0]+Math.cos(u)*radius,p[1]+Math.sin(u)*radius,p[2]],color,2);vertex(highlight,[p[0]-Math.cos(u)*radius,p[1]-Math.sin(u)*radius,p[2]],color,2);}
          else{vertex(highlight, [p[0]+Math.cos(u)*radius, p[1]+Math.sin(u)*radius, p[2]], color, 2);
            vertex(highlight, [p[0]+Math.cos(v)*radius, p[1]+Math.sin(v)*radius, p[2]], color, 2);}
        }
        vertex(highlight, p, color, 7); vertex(highlight, p, color, 50);
      }
    });
    if(o.goals)(this._navigationGoalOverlay?.goals||[]).forEach(function(goal){
      var p=self.navigationGoalMarkerPosition(goal);if(!self._navigationVisible(goal,p))return;
      var chosen=selected?.kind==='goal'&&String(selected.id)===String(goal.id),out=chosen?highlight:posts,color=chosen?[1,0.65,0.1]:[0.2,1,0.25];
      vertex(out,p,color);vertex(out,p,color,32);vertex(points,p,color,32);
      var radius=Number(goal.radius);if(Number.isFinite(radius)&&radius>0&&radius<10000){
        for(var i=0;i<7;i++){var angle=i*Math.PI/7;vertex(out,[p[0]+Math.cos(angle)*radius,p[1]+Math.sin(angle)*radius,p[2]],color);vertex(out,[p[0]-Math.cos(angle)*radius,p[1]-Math.sin(angle)*radius,p[2]],color);}
      }
      Object.values(goal.properties?.AimVectors||{}).forEach(function(aim){
        if(!Array.isArray(aim)||aim.length!==3||!aim.every(Number.isFinite))return;
        vertex(out,p,[0.3,0.65,1],8);vertex(out,p.map((v,i)=>v+aim[i]*64),[0.3,0.65,1],8);
      });
      self.navigationSavedGoalArrows(goal).forEach(function(arrow){
        vertex(out,arrow.from,arrow.color);vertex(out,arrow.to,arrow.color);
      });
    });
    var preview = this._navigationEditPreview;
    (this._navigationSpawns || []).forEach(function(spawn){
      var p=spawn.position;if(!self._navigationVisible(spawn,p))return;
      var chosen=selected?.kind==='spawn'&&String(selected.id)===String(spawn.id);
      var color=chosen?[1,0.85,0.25]:spawn.kind==='bsp'?[0.25,0.65,1]:[0.3,1,0.55];
      var out=chosen?highlight:posts, angle=(Number(spawn.angle)||0)*Math.PI/180;
      var feetOffset=Number(spawn.origin_above_feet)||24;
      // Spawn coordinates are player origins. Mark the exact origin and body.
      [[-16,-16],[16,-16],[16,16],[-16,16]].forEach(function(c,i,a){
        var next=a[(i+1)%a.length];
        vertex(out,[p[0]+c[0],p[1]+c[1],p[2]-feetOffset],color);
        vertex(out,[p[0]+next[0],p[1]+next[1],p[2]-feetOffset],color);
        vertex(out,[p[0]+c[0],p[1]+c[1],p[2]-feetOffset],color);
        vertex(out,[p[0]+c[0],p[1]+c[1],p[2]+40],color);
      });
      vertex(out,p,color);vertex(out,[p[0]+Math.cos(angle)*48,p[1]+Math.sin(angle)*48,p[2]],color);
      vertex(points,p,color);
    });
    if (preview && preview.point) {
      var start = navPosition(this._navigationNodeMap[String(preview.from)]);
      var color = [0.4, 1, 0.55];
      if (start) { vertex(highlight, start, color, edgeLift); vertex(highlight, preview.point, color, edgeLift); }
      vertex(highlight, preview.point, color, 1); vertex(highlight, preview.point, color, 64);
    }
    this._navigationBuffer(lines, gl.LINES, ingame ? 1 : 0.7);
    if(lines.length)this._navigationBuffers[this._navigationBuffers.length-1].graphEdges=true;
    this._navigationBuffer(posts, gl.LINES, 1);
    this._navigationBuffer(points, gl.POINTS, 1, ingame ? 4 : 7);
    this._navigationBuffer(highlight, gl.LINES, 1);
  };

  BSPViewer.prototype._drawNavigation = function (mvp) {
    if (!this._navigationBuffers || !this._navigationBuffers.length) return;
    var gl = this.gl, cullingEnabled = gl.isEnabled(gl.CULL_FACE);
    if (!this._navigationProgram) {
      this._navigationProgram = linkProgram(gl,
        compileShader(gl, gl.VERTEX_SHADER, 'attribute vec3 aPos; attribute vec3 aCol; uniform mat4 uMVP; uniform float uSize; uniform vec2 uScreenShift; varying vec3 vCol; varying vec3 vPos; void main(){vCol=aCol;vPos=aPos;gl_Position=uMVP*vec4(aPos,1.0);gl_Position.xy+=uScreenShift*gl_Position.w;gl_PointSize=uSize;}'),
        compileShader(gl, gl.FRAGMENT_SHADER, 'precision mediump float; varying vec3 vCol; varying vec3 vPos; uniform vec3 uCamera; uniform float uDistance; uniform float uAlpha; uniform float uPoint; uniform float uOutline; void main(){if(uDistance>0.0 && length((vPos-uCamera)*0.001)>uDistance*0.001)discard;if(uPoint>0.5 && distance(gl_PointCoord,vec2(0.5))>0.5)discard;gl_FragColor=vec4(mix(vCol,vec3(0.025),uOutline),uAlpha);}'));
      this._navigationLoc = {};
      var self = this;
      ['uMVP', 'uSize', 'uAlpha', 'uPoint', 'uCamera', 'uDistance','uScreenShift','uOutline'].forEach(function (key) { self._navigationLoc[key] = gl.getUniformLocation(self._navigationProgram, key); });
      this._navigationLoc.aPos = gl.getAttribLocation(this._navigationProgram, 'aPos');
      this._navigationLoc.aCol = gl.getAttribLocation(this._navigationProgram, 'aCol');
    }
    var loc = this._navigationLoc;
    gl.useProgram(this._navigationProgram);
    gl.uniformMatrix4fv(loc.uMVP, false, mvp);
    gl.uniform3fv(loc.uCamera, this._cam.pos);
    gl.uniform1f(loc.uDistance, this._navigationOptions.maxDistance || 0);
    gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    gl.depthMask(false); gl.depthFunc(gl.LEQUAL); gl.disable(gl.CULL_FACE);
    if (this._navigationOptions.xray) gl.disable(gl.DEPTH_TEST); else gl.enable(gl.DEPTH_TEST);
    gl.enableVertexAttribArray(loc.aPos); gl.enableVertexAttribArray(loc.aCol);
    this._navigationBuffers.forEach(function (item) {
      gl.bindBuffer(gl.ARRAY_BUFFER, item.buffer);
      gl.vertexAttribPointer(loc.aPos, 3, gl.FLOAT, false, 24, 0);
      gl.vertexAttribPointer(loc.aCol, 3, gl.FLOAT, false, 24, 12);
      gl.uniform1f(loc.uAlpha, item.alpha); gl.uniform1f(loc.uSize, item.pointSize);
      gl.uniform1f(loc.uPoint, item.primitive === gl.POINTS ? 1 : 0);
      if(item.graphEdges){
        // A one-pixel dark contour remains visible against bright BSP textures;
        // retain depth testing and distance clipping, not an X-ray shortcut.
        gl.uniform1f(loc.uOutline,1);
        [[-1,0],[1,0],[0,-1],[0,1]].forEach(function(offset){
          gl.uniform2f(loc.uScreenShift,offset[0]*2/gl.drawingBufferWidth,offset[1]*2/gl.drawingBufferHeight);
          gl.drawArrays(item.primitive,0,item.count);
        });
      }
      gl.uniform1f(loc.uOutline,0);gl.uniform2f(loc.uScreenShift,0,0);
      gl.drawArrays(item.primitive, 0, item.count);
    });
    gl.disableVertexAttribArray(loc.aPos); gl.disableVertexAttribArray(loc.aCol);
    gl.enable(gl.DEPTH_TEST);
    if (cullingEnabled) gl.enable(gl.CULL_FACE); else gl.disable(gl.CULL_FACE);
    gl.depthMask(true); gl.disable(gl.BLEND);
  };

  BSPViewer.prototype.navigationFloorPoint = function (clientX, clientY) {
    // Placement aid using the viewer's BSP brushes/patches, never a native
    // hull certificate. The mandatory server pass also checks map models.
    var rect = this.canvas.getBoundingClientRect(), cam = this._cam;
    if (!this._cm || !rect.width || !rect.height) return null;
    var nx = 2*(clientX-rect.left)/rect.width-1, ny = 1-2*(clientY-rect.top)/rect.height;
    var cy = Math.cos(cam.yaw), sy = Math.sin(cam.yaw), cp = Math.cos(cam.pitch), sp = Math.sin(cam.pitch);
    var f = [sy*cp, sp, -cy*cp], right = [cy, 0, sy], up = [-sy*sp, cp, cy*sp];
    var scale = Math.tan(Math.PI/6), aspect = rect.width/rect.height;
    var direction = norm3(f.map(function (v, i) { return v + right[i]*nx*scale*aspect + up[i]*ny*scale; }));
    var end = cam.pos.map(function (v, i) { return v + direction[i]*8192; });
    var hit = this._traceBox(cam.pos, end, [0, 0, 0], [0, 0, 0]);
    if (hit.startsolid || hit.allsolid || hit.fraction >= 1 || !hit.normal || hit.normal[1] < 0.55) return null;
    var result = glToQ3(cam.pos.map(function (v, i) { return v + direction[i]*8192*hit.fraction; }));
    result[2] += 0.125;
    return result;
  };

  // Display-only helpers. The backend revision-checks persisted edits after a
  // gesture, and native validation remains mandatory for the resulting graph.
  BSPViewer.prototype.previewNavigationNode = function (id, point) {
    var node = (this._navigationNodeMap || {})[String(id)];
    if (!node || !point || point.length !== 3 || !point.every(Number.isFinite)) return false;
    node.position = point.slice();
    this._rebuildNavigation();
    return true;
  };
  BSPViewer.prototype.setNavigationEditPreview = function (preview) {
    this._navigationEditPreview = preview;
    this._rebuildNavigation();
  };
  BSPViewer.prototype.projectNavigationPoint = function (position, lift) {
    var m = this._navigationMVP, rect = this.canvas.getBoundingClientRect();
    if (!m || !position) return null;
    var p = q3ToGLVec([position[0], position[1], position[2] + (lift || 0)]);
    var w = m[3]*p[0]+m[7]*p[1]+m[11]*p[2]+m[15];
    if (w <= 0) return null;
    var depth=(m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14])/w;
    if(depth < -1 || depth > 1)return null;
    return [rect.left+(1+(m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12])/w)*rect.width/2,
      rect.top+(1-(m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13])/w)*rect.height/2];
  };
  BSPViewer.prototype.navigationCameraGesture = function (dx, dy, pan) {
    var c = this._cam;
    if (pan) {
      var cy = Math.cos(c.yaw), sy = Math.sin(c.yaw);
      c.pos[0] -= dx*cy; c.pos[2] -= dx*sy; c.pos[1] += dy;
    } else {
      c.yaw += dx*0.004;
      c.pitch = Math.max(-Math.PI/2+0.01, Math.min(Math.PI/2-0.01, c.pitch-dy*0.004));
    }
  };
  BSPViewer.prototype.navigationCameraZoom = function (delta) {
    var c = this._cam, amount = Math.max(-200, Math.min(200, delta))*0.75;
    c.pos[0] -= Math.sin(c.yaw)*Math.cos(c.pitch)*amount;
    c.pos[1] -= Math.sin(c.pitch)*amount;
    c.pos[2] += Math.cos(c.yaw)*Math.cos(c.pitch)*amount;
  };

  BSPViewer.prototype.pickNavigation = function (clientX, clientY, options) {
    options = options || {};
    var nav = this._navigation, m = this._navigationMVP;
    if (!nav || !m) return null;
    var rect = this.canvas.getBoundingClientRect(), x = clientX-rect.left, y = clientY-rect.top;
    var self = this, best = null, distance = 13;
    function project(pos, lift) {
      var p = q3ToGLVec([pos[0], pos[1], pos[2]+(lift == null ? (self._navigationOptions.style === 'ingame' ? 64 : 7) : lift)]);
      var limit = self._navigationOptions.maxDistance;
      if (limit > 0 && Math.hypot(p[0]-self._cam.pos[0], p[1]-self._cam.pos[1], p[2]-self._cam.pos[2]) > limit) return null;
      var w = m[3]*p[0]+m[7]*p[1]+m[11]*p[2]+m[15];
      if (w <= 0) return null;
      var z = (m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14])/w;
      if (z < -1 || z > 1) return null;
      return [(1+(m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12])/w)*rect.width/2,
        (1-(m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13])/w)*rect.height/2];
    }
    if(!options.nodesOnly)(this._navigationSpawns||[]).forEach(function(spawn){
      if(!self._navigationVisible(spawn,spawn.position))return;
      var p=project(spawn.position,0);if(!p)return;
      var d=Math.hypot(x-p[0],y-p[1]);
      if(d<distance){distance=d;best={kind:'spawn',id:spawn.id};}
    });
    if(options.spawnsOnly)return options.select===false?(best?{kind:'spawn',id:best.id,item:this._navigationSpawnMap[String(best.id)]}:null):this.selectNavigation(best?.kind,best?.id);
    if(!options.nodesOnly&&this._navigationOptions.goals)(this._navigationGoalOverlay?.goals||[]).forEach(function(goal){
      var marker=self.navigationGoalMarkerPosition(goal);if(!self._navigationVisible(goal,marker))return;var p=project(marker,32);if(!p)return;
      var d=Math.hypot(x-p[0],y-p[1]);if(d<distance){distance=d;best={kind:'goal',id:goal.id};}
    });
    // Screen-space inspection deliberately also selects occluded nodes. The UI
    // labels this and offers an explicit X-ray layer; no visibility claim is made.
    if (this._navigationOptions.nodes) (nav.nodes || []).forEach(function (node) {
      var pos = navPosition(node);
      if (!self._navigationVisible(node, pos)) return;
      var p = project(pos); if (!p) return;
      var d = Math.hypot(x-p[0], y-p[1]);
      if (self._navigationOptions.style === 'ingame') {
        var foot = project(pos, 1);
        if (foot) {
          var dx = p[0]-foot[0], dy = p[1]-foot[1], l2 = dx*dx+dy*dy;
          var t = l2 ? Math.max(0, Math.min(1, ((x-foot[0])*dx+(y-foot[1])*dy)/l2)) : 0;
          d = Math.min(d, Math.hypot(x-foot[0]-t*dx, y-foot[1]-t*dy));
        }
      }
      if (d < distance) { distance = d; best = {kind: 'node', id: node._navId}; }
    });
    if (!best && !options.nodesOnly && this._navigationOptions.edges !== 'none') (nav.edges || []).forEach(function (edge, index) {
      var ends = self._navigationEdgeNodes(edge), pa = navPosition(ends[0]), pb = navPosition(ends[1]);
      if (!self._navigationVisible(edge, pa) || !pb || pb[2] < self._navigationOptions.minZ || pb[2] > self._navigationOptions.maxZ) return;
      if (self._navigationOptions.edges === 'selected') {
        var selected = self._navigationSelection;
        if (!selected || selected.kind !== 'node' || !ends.some(function (n) { return n && String(n._navId) === String(selected.id); })) return;
      }
      var a = project(pa,self.navigationEdgeLift()), b = project(pb,self.navigationEdgeLift()); if (!a || !b) return;
      var dx = b[0]-a[0], dy = b[1]-a[1], l2 = dx*dx+dy*dy;
      var t = l2 ? Math.max(0, Math.min(1, ((x-a[0])*dx+(y-a[1])*dy)/l2)) : 0;
      var d = Math.hypot(x-a[0]-t*dx, y-a[1]-t*dy);
      if (d < Math.min(distance, 7)) { distance = d; best = {kind: 'edge', id: index}; }
    });
    if (options.select === false) return best ? {kind: best.kind, id: best.id,
      item: best.kind === 'node' ? this._navigationNodeMap[String(best.id)] : best.kind === 'spawn' ? this._navigationSpawnMap[String(best.id)] : best.kind === 'goal' ? this._navigationGoalMap[String(best.id)] : nav.edges[best.id]} : null;
    return this.selectNavigation(best && best.kind, best && best.id);
  };

  global.BSPViewer = BSPViewer;

})(window);
