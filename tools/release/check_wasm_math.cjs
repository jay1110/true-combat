// The ET Legacy web host can export long-double math with a different Wasm ABI.
const fs = require('node:fs');
const manifest = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const localMath = new Set(['sqrtl', 'floorl', 'sinl', 'cosl', 'atan2l']);

for (const role of ['ui', 'cgame', 'qagame']) {
    const module = new WebAssembly.Module(fs.readFileSync(manifest.modules[role]));
    for (const entry of WebAssembly.Module.imports(module)) {
        if (localMath.has(entry.name)) {
            throw new Error(role + ' imports host long-double math: ' + entry.name);
        }
        if (role !== 'qagame' && entry.module === 'GOT.mem' &&
            ['__stack_low', '__stack_high', '__THREW__', '__threwValue'].includes(entry.name)) {
            throw new Error(role + ' requires an internal engine global: ' + entry.name);
        }
    }
}
