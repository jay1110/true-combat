"""Actual gmAABB.cpp/gmBind and GetEntWorldAABB binding under native GC.

The entity coordinates/handle decoding are independent fixture inputs. The
productive optional-output read and native object lifetime run unchanged.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_traffic_yield_clearance_vm import ROOT,NATIVE_DRIVER,run_pair


@unittest.skipUnless(os.name=='nt' and NATIVE_DRIVER.is_file(),'Offline native AABB VM required.')
class AABBBindingVMTests(unittest.TestCase):
    def vm(self,script,old=False):
        driver=NATIVE_DRIVER.with_name('gm-aabb-vm-before-guard.exe') if old else NATIVE_DRIVER
        receipt=json.loads(driver.with_name('before-guard-build.json' if old else 'build.json').read_text())
        self.assertEqual(hashlib.sha256(driver.read_bytes()).hexdigest(),receipt['executable_sha256'])
        if not old:
            for name,value in receipt['native_sources'].items():
                self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),value)
        with tempfile.TemporaryDirectory(prefix='tce-native-aabb-')as folder:
            path=Path(folder)/'probe.gm';path.write_text(script,encoding='ascii')
            return subprocess.run([str(driver),str(path)],capture_output=True,text=True,timeout=15,
                                  creationflags=subprocess.CREATE_NO_WINDOW)

    def test_native_one_argument_stale_stack_null_output_and_invalid_entities(self):
        result=self.vm(r'''
global a=GetEntWorldAABB(3);global b=GetEntWorldAABB(5);
print("PAIR:"+typeName(a)+":"+typeName(b)+":"+b.GetAxisLength("x"));
global output=AABB(Vec3(0,0,0),Vec3(1,1,1));
global written=GetEntWorldAABB(5,output);
print("OUTPUT:"+written+":"+typeName(output)+":"+output.GetAxisLength("x"));
global explicitNull=GetEntWorldAABB(3,null);
print("NULL:"+typeName(explicitNull)+":"+explicitNull.GetAxisLength("y"));
print("INVALID:"+typeName(GetEntWorldAABB(-1)));
global wrongType=GetEntWorldAABB(3,42);print("OTHER:"+typeName(wrongType));
''')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        for expected in ('PAIR:AABB:AABB:34','OUTPUT:1:AABB:34','NULL:AABB:34','INVALID:null','OTHER:AABB'):
            self.assertIn(expected,result.stdout)

    def test_native_repeated_allocations_and_explicit_gc_keep_method_receivers(self):
        result=self.vm(r'''
global passed=0;
for(i=0;i<1000;i+=1){
 a=GetEntWorldAABB(3);NativeGC();b=GetEntWorldAABB(5);NativeGC();
 width=(a.GetAxisLength("x")+b.GetAxisLength("x"))*.5-2;
 if(width==32){passed+=1;}
}print("GC_PAIRS:"+passed);
''')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('GC_PAIRS:1000',result.stdout)

    def test_actual_traffic_yield_uses_native_aabbs_with_gc_and_moves_both_bodies(self):
        result=run_pair(native=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('YIELDED:-32.25:32.125:4:0.35',result.stdout)
        self.assertIn('PASSED:94:-32.25',result.stdout)

    def test_historical_binding_reproduces_integer_receiver_and_live_error(self):
        result=self.vm('global a=GetEntWorldAABB(3);global b=GetEntWorldAABB(5);'
                       'print("BAD:"+typeName(b));b.GetAxisLength("x");',old=True)
        self.assertEqual(result.returncode,3,result.stdout+result.stderr)
        self.assertIn('BAD:int',result.stdout)
        self.assertIn('attempt to call non function type',result.stderr)


if __name__=='__main__':unittest.main()
