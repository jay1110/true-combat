"""Read WOW64 x87 control words; never write thread context or game memory.

GetThreadContext requires a stopped thread. Each selected thread is suspended
only around the single context read and resumed in finally, before any I/O.
"""
import ctypes as C
from ctypes import wintypes as W
import json
import struct

from live_compare import K, OUT


class ThreadEntry(C.Structure):
    _fields_ = [(name, W.DWORD) for name in ('size', 'usage', 'id', 'owner', 'base', 'delta', 'flags')]


def main():
    run = json.loads((OUT / 'current.json').read_text())
    K.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
    K.CreateToolhelp32Snapshot.restype = W.HANDLE
    K.Thread32First.argtypes = [W.HANDLE, C.POINTER(ThreadEntry)]
    K.Thread32Next.argtypes = [W.HANDLE, C.POINTER(ThreadEntry)]
    K.OpenThread.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    K.OpenThread.restype = W.HANDLE
    K.SuspendThread.argtypes = [W.HANDLE]
    K.ResumeThread.argtypes = [W.HANDLE]
    K.Wow64GetThreadContext.argtypes = [W.HANDLE, C.c_void_p]
    snapshot = K.CreateToolhelp32Snapshot(4, 0)
    if snapshot in (None, C.c_void_p(-1).value):
        raise C.WinError(C.get_last_error())
    entries = []
    try:
        entry = ThreadEntry(); entry.size = C.sizeof(entry)
        exists = K.Thread32First(snapshot, C.byref(entry))
        while exists:
            if entry.owner == run['pid']:
                handle = K.OpenThread(0xA, False, entry.id)
                if not handle:
                    raise C.WinError(C.get_last_error())
                raw = C.create_string_buffer(736)
                address = (C.addressof(raw) + 15) & ~15
                C.c_uint32.from_address(address).value = 0x10028
                try:
                    if K.SuspendThread(handle) == -1:
                        raise C.WinError(C.get_last_error())
                    try:
                        if not K.Wow64GetThreadContext(handle, address):
                            raise C.WinError(C.get_last_error())
                    finally:
                        if K.ResumeThread(handle) == -1:
                            raise C.WinError(C.get_last_error())
                    data = C.string_at(address, 716)
                    control = struct.unpack_from('<I', data, 28)[0] & 0xFFFF
                    entries.append({'thread_id': entry.id, 'x87_control_word': hex(control),
                                    'precision_bits': {0: 24, 0x200: 53, 0x300: 64}.get(control & 0x300),
                                    'rounding_bits': hex(control & 0xC00)})
                finally:
                    K.CloseHandle(handle)
            exists = K.Thread32Next(snapshot, C.byref(entry))
    finally:
        K.CloseHandle(snapshot)
    report = {'run': run, 'threads': entries, 'limit': 'Instantaneous thread control words, not exception/trap parity'}
    (OUT / (run['variant'] + '-fpu.json')).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(entries))


if __name__ == '__main__':
    main()
