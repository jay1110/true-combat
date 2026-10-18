"""Fresh native Windows identity/duplicate/UDP-owner guard without any game.

A tiny private executable binds an ephemeral loopback UDP socket and exits on
stdin. No ET process, game port, HTTP/API or RCON endpoint is changed or used.
"""
import ctypes
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
from types import SimpleNamespace
from unittest import TestCase, main, skipUnless
from unittest.mock import patch

from watch import Windows, ProcessIdentityChanged, _UdpOwnerRow4, _UdpOwnerRow6

ROOT = Path(__file__).resolve().parents[2]

HELPER = r'''
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
 WSADATA data;SOCKET fd;int length,port,family=(argc>1&&atoi(argv[1])==6)?AF_INET6:AF_INET;
 struct sockaddr_in four;struct sockaddr_in6 six;
 if(WSAStartup(MAKEWORD(2,2),&data))return 1;
 fd=socket(family,SOCK_DGRAM,IPPROTO_UDP);if(fd==INVALID_SOCKET)return 2;
 if(family==AF_INET) {
  ZeroMemory(&four,sizeof(four));four.sin_family=AF_INET;four.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(bind(fd,(struct sockaddr *)&four,sizeof(four)))return 3;
  length=sizeof(four);if(getsockname(fd,(struct sockaddr *)&four,&length))return 4;port=ntohs(four.sin_port);
 } else {
  ZeroMemory(&six,sizeof(six));six.sin6_family=AF_INET6;six.sin6_addr=in6addr_loopback;
  if(bind(fd,(struct sockaddr *)&six,sizeof(six)))return 5;
  length=sizeof(six);if(getsockname(fd,(struct sockaddr *)&six,&length))return 6;port=ntohs(six.sin6_port);
 }
 printf("%d\n",port);fflush(stdout);getchar();closesocket(fd);WSACleanup();return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows native process test required')
class ProbeOwnerNativeTests(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='tce-owner-test-', dir=ROOT.parent/'build')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        (cls.directory/'main.c').write_text(HELPER)
        (cls.directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\n'
            'project(OwnerFixture LANGUAGES C)\nadd_executable(owner_probe main.c)\n'
            'target_link_libraries(owner_probe PRIVATE ws2_32)\n')
        for command in (['cmake','-S',str(cls.directory),'-B',str(cls.directory/'build'),'-A','Win32'],
                        ['cmake','--build',str(cls.directory/'build'),'--config','Release']):
            result = subprocess.run(command,capture_output=True,text=True,timeout=90,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            if result.returncode:
                raise AssertionError(result.stdout+result.stderr)
        cls.executable = cls.directory/'build/Release/owner_probe.exe'

    def helper(self, executable=None, family=4):
        executable = executable or self.executable
        child = subprocess.Popen([str(executable),str(family)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
        self.addCleanup(self.stop_helper,child)
        line = child.stdout.readline()
        self.assertTrue(line.strip().isdigit(),line+child.stderr.read() if child.poll() is not None else line)
        port = int(line);self.assertNotEqual(port,27986)
        api = Windows();identity = api.identity(child.pid)
        self.assertIsNotNone(identity)
        return child,port,identity

    @staticmethod
    def stop_helper(child):
        if child.poll() is None:
            child.communicate('\n',timeout=5)

    def test_actual_private_udp_process_both_families_and_uncached_rechecks(self):
        for family in (4,6):
            with self.subTest(family=family):
                child,port,identity = self.helper(family=family)
                api = Windows()
                with patch.object(api,'_same_image_pids',wraps=api._same_image_pids) as images,\
                     patch.object(api,'_udp_owner_pids',wraps=api._udp_owner_pids) as ports:
                    for unused in range(2):
                        result = api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)
                        self.assertEqual(result,dict(pid=child.pid,identity=list(identity),owns_port=True,
                            verification='native_process_snapshot_udp_table_v1'))
                    self.assertEqual(images.call_count,2);self.assertEqual(ports.call_count,2)
                self.stop_helper(child)

    def test_actual_duplicate_exact_image_rejects_then_fresh_read_recovers(self):
        first,port,identity = self.helper();api = Windows()
        second,_,_ = self.helper()
        with self.assertRaisesRegex(ProcessIdentityChanged,'eindeutige Instanz'):
            api.probe_owner(first.pid,identity,expected_exe=self.executable,port=port)
        self.stop_helper(second)
        self.assertTrue(api.probe_owner(first.pid,identity,expected_exe=self.executable,port=port)['owns_port'])

    def test_same_basename_different_exact_image_is_not_a_duplicate(self):
        first,port,identity = self.helper()
        foreign_dir = self.directory/'foreign';foreign_dir.mkdir(exist_ok=True)
        foreign = foreign_dir/self.executable.name;shutil.copy2(self.executable,foreign)
        other,_,_ = self.helper(foreign)
        api = Windows()
        self.assertEqual(api._same_image_pids(identity[0]),{first.pid})
        self.assertTrue(api.probe_owner(first.pid,identity,expected_exe=self.executable,port=port)['owns_port'])
        self.stop_helper(other)

    def test_actual_creation_executable_pid_and_current_port_are_required(self):
        child,port,identity = self.helper();api = Windows()
        with self.assertRaises(ProcessIdentityChanged):
            api.probe_owner(child.pid,[identity[0],identity[1]+1],expected_exe=self.executable,port=port)
        with self.assertRaises(ProcessIdentityChanged):
            api.probe_owner(child.pid,identity,expected_exe=self.executable.parent/'other.exe',port=port)
        with self.assertRaises(ProcessIdentityChanged):
            api.probe_owner(os.getpid(),identity,expected_exe=self.executable,port=port)
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as own_socket:
            own_socket.bind(('127.0.0.1',0));unrelated = own_socket.getsockname()[1]
            with self.assertRaisesRegex(ProcessIdentityChanged,'Testport'):
                api.probe_owner(child.pid,identity,expected_exe=self.executable,port=unrelated)
        self.stop_helper(child)
        with self.assertRaises(ProcessIdentityChanged):
            api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)

    def test_exit_during_udp_read_and_ambiguous_owner_cannot_pass(self):
        child,port,identity = self.helper();api = Windows()
        with patch.object(api,'_udp_owner_pids',return_value={child.pid,os.getpid()}):
            with self.assertRaisesRegex(ProcessIdentityChanged,'Testport'):
                api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)
        def exit_then_old_pid(unused):
            self.stop_helper(child);return {child.pid}
        with patch.object(api,'_udp_owner_pids',side_effect=exit_then_old_pid):
            with self.assertRaisesRegex(ProcessIdentityChanged,'während'):
                api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)

    def test_access_or_snapshot_uncertainty_never_falls_back_to_pid(self):
        child,port,identity = self.helper();api = Windows()
        with patch.object(api.k,'OpenProcess',return_value=None):
            with self.assertRaises(ProcessIdentityChanged):
                api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)
        for failure in (None,ctypes.c_void_p(-1).value):
            with self.subTest(snapshot=failure),patch.object(api.k,'CreateToolhelp32Snapshot',return_value=failure):
                with self.assertRaises(ProcessIdentityChanged):
                    api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)
        with patch.object(api,'_same_image_pids',side_effect=ProcessIdentityChanged('unknown candidate')):
            with self.assertRaisesRegex(ProcessIdentityChanged,'unknown candidate'):
                api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)

    def test_invalid_identity_and_port_do_not_open_any_process(self):
        child,port,identity = self.helper();api = Windows()
        variants = [(True,identity,port),(child.pid,None,port),(child.pid,[identity[0],True],port),
                    (child.pid,identity,0),(child.pid,identity,65536),(child.pid,identity,True)]
        with patch.object(api.k,'OpenProcess',side_effect=AssertionError('invalid request must not inspect')):
            for pid,binding,requested_port in variants:
                with self.subTest(pid=pid,binding=binding,port=requested_port),self.assertRaises(ProcessIdentityChanged):
                    api.probe_owner(pid,binding,expected_exe=self.executable,port=requested_port)

    def test_native_guard_microbenchmark_on_one_actual_owned_process(self):
        child,port,identity = self.helper();api = Windows();samples=[]
        for unused in range(10):
            start=time.perf_counter()
            api.probe_owner(child.pid,identity,expected_exe=self.executable,port=port)
            samples.append(time.perf_counter()-start)
        print('NATIVE_PROBE_OWNER_TIMINGS '+json.dumps(dict(pid=child.pid,samples=samples)))


@skipUnless(os.name == 'nt','Windows native table layouts required')
class ProbeOwnerTableTests(TestCase):
    def test_both_native_table_layouts_and_network_byte_order(self):
        api = Windows();calls=[];port=54321
        def table(buffer,size,ordered,family,kind,reserved):
            calls.append((family,kind,reserved));row_type=_UdpOwnerRow4 if family==2 else _UdpOwnerRow6
            row=row_type();row.port=socket.htons(port);row.pid=100+family
            payload=bytes(W.DWORD(1))+bytes(row)
            ctypes.cast(size,ctypes.POINTER(W.DWORD)).contents.value=len(payload)
            if buffer is None:return 122
            ctypes.memmove(buffer,payload,len(payload));return 0
        api.ip=SimpleNamespace(GetExtendedUdpTable=table)
        self.assertEqual(api._udp_owner_pids(port),{102,123})
        self.assertEqual(calls,[(2,1,0),(2,1,0),(23,1,0),(23,1,0)])

    def test_unknown_status_unbounded_growth_truncation_and_other_family_fail_closed(self):
        for mode in ('access','zero','too_large','truncated','changed','ipv6_unreadable'):
            api=Windows();calls=[]
            def table(buffer,size,ordered,family,kind,reserved):
                calls.append(family);length=ctypes.cast(size,ctypes.POINTER(W.DWORD))
                if mode=='access' or mode=='ipv6_unreadable' and family==23:return 5
                length.contents.value=0 if mode=='zero' else 16*1024*1024+1 if mode=='too_large' else 4
                if buffer is None or mode=='changed':return 122
                ctypes.memmove(buffer,bytes(W.DWORD(1 if mode=='truncated' else 0)),4);return 0
            api.ip=SimpleNamespace(GetExtendedUdpTable=table)
            with self.subTest(mode=mode),self.assertRaises(ProcessIdentityChanged):api._udp_owner_pids(54321)
            self.assertLessEqual(len(calls),4)


if __name__ == '__main__':main()
