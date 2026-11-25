"""Build and instantiate binary128 arithmetic without host compiler helpers."""
import argparse
import json
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--emsdk',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--module',type=Path,action='append',default=[])
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    sdk=a.emsdk.resolve()
    source=out/'quad.c'
    source.write_text('double quad_product(double a,double b){volatile long double r=(long double)a*(long double)b;return (double)r;}\n')
    compiler=sdk/'upstream/emscripten/emcc.exe'
    if not compiler.exists():compiler=sdk/'upstream/emscripten/emcc'
    library=Path(subprocess.check_output([str(compiler),'-print-libgcc-file-name'],text=True).strip()).name
    if not library.startswith('lib') or not library.endswith('.a'):raise RuntimeError('Cannot identify compiler builtins')
    library=library[3:-2]
    subprocess.run([str(compiler),'-sSIDE_MODULE=1','-Wl,--export=quad_product',str(source),'-l'+library,'-o',str(out/'quad.wasm')],check=True)
    nodes=sorted((sdk/'node').rglob('node.exe')) or sorted((sdk/'node').rglob('node'))
    if not nodes:raise RuntimeError('No SDK Node runtime found')
    check=out/'check.cjs'
    check.write_text('''const fs=require('fs');
const m=new WebAssembly.Module(fs.readFileSync('quad.wasm'));
const imports=WebAssembly.Module.imports(m);
if(imports.some(x=>x.kind==='function'))throw Error('Unexpected host arithmetic import');
const env={memory:new WebAssembly.Memory({initial:2}),
__stack_pointer:new WebAssembly.Global({value:'i32',mutable:true},65536),
__memory_base:new WebAssembly.Global({value:'i32',mutable:false},1024),
__table_base:new WebAssembly.Global({value:'i32',mutable:false},0),
__indirect_function_table:new WebAssembly.Table({initial:1,element:'anyfunc'})};
const i=new WebAssembly.Instance(m,{env});
if(i.exports.quad_product(1.5,2)!==3||i.exports.quad_product(-2,4)!==-8)throw Error('Wrong arithmetic');
const checked=[];
for(const file of JSON.parse(process.argv[2]||'[]')){
 const module=new WebAssembly.Module(fs.readFileSync(file));
 const bad=WebAssembly.Module.imports(module).filter(x=>/^__(multf3|addtf3|subtf3|divtf3)$/.test(x.name));
 if(bad.length)throw Error('Host binary128 arithmetic imports in '+file+': '+bad.map(x=>x.name));
 checked.push(require('path').basename(file));
}
console.log(JSON.stringify({instantiated:true,hostArithmeticImports:0,products:[3,-8],checkedModules:checked}));
''')
    r=subprocess.run([str(nodes[-1]),str(check),json.dumps([str(x.resolve()) for x in a.module])],cwd=out,text=True,capture_output=True)
    (out/'node.log').write_text(r.stdout+r.stderr)
    print(r.stdout+r.stderr,end='');return r.returncode

if __name__=='__main__':raise SystemExit(main())
