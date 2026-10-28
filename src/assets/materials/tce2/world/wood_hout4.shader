// Native ET detail material. Install via DynamicShader before LoadWorldMap.
// Outside scripts/: no automatic override, no runtime remap/lightmap loss.

textures/liq/hout4
{
	qer_editorimage textures/liq/hout4.tga
	surfaceparm mat_wood
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/liq/hout4.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/tce2/details/wood.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		rgbGen identity
		tcMod scale 4 4
		detail
	}
}
