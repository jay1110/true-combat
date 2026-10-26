"""Exact Windows process identity and explicit stop; never changes game windows/input."""
import ctypes
from ctypes import wintypes as W
import os
from pathlib import Path
import socket


class _ProcessEntry32(ctypes.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD), ("th32ProcessID", W.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", W.DWORD),
                ("cntThreads", W.DWORD), ("th32ParentProcessID", W.DWORD),
                ("pcPriClassBase", W.LONG), ("dwFlags", W.DWORD), ("szExeFile", W.WCHAR * 260)]


class _UdpOwnerRow4(ctypes.Structure):
    _fields_ = [("address", W.DWORD), ("port", W.DWORD), ("pid", W.DWORD)]


class _UdpOwnerRow6(ctypes.Structure):
    _fields_ = [("address", ctypes.c_ubyte * 16), ("scope", W.DWORD),
                ("port", W.DWORD), ("pid", W.DWORD)]


class ProcessIdentityChanged(RuntimeError):
    """Fatal identity mismatch: never retry against another process."""


class Windows:
    def __init__(self):
        if os.name != "nt":
            raise RuntimeError("Native process inspection requires Windows.")
        self.k = ctypes.WinDLL("kernel32", use_last_error=True)
        self.k.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
        self.k.OpenProcess.restype = W.HANDLE
        self.k.CloseHandle.argtypes = [W.HANDLE]
        self.k.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, ctypes.POINTER(W.DWORD)]
        self.k.GetProcessTimes.argtypes = [W.HANDLE] + [ctypes.POINTER(W.FILETIME)] * 4
        self.k.TerminateProcess.argtypes = [W.HANDLE, W.UINT]
        self.k.TerminateProcess.restype = W.BOOL
        self.k.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
        self.k.WaitForSingleObject.restype = W.DWORD
        self.k.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
        self.k.CreateToolhelp32Snapshot.restype = W.HANDLE
        self.k.Process32FirstW.argtypes = [W.HANDLE, ctypes.POINTER(_ProcessEntry32)]
        self.k.Process32FirstW.restype = W.BOOL
        self.k.Process32NextW.argtypes = [W.HANDLE, ctypes.POINTER(_ProcessEntry32)]
        self.k.Process32NextW.restype = W.BOOL
        self.ip = ctypes.WinDLL("iphlpapi", use_last_error=True)
        self.ip.GetExtendedUdpTable.argtypes = [ctypes.c_void_p, ctypes.POINTER(W.DWORD),
                                               W.BOOL, W.ULONG, ctypes.c_int, W.ULONG]
        self.ip.GetExtendedUdpTable.restype = W.DWORD

    def identity(self, pid):
        handle = self.k.OpenProcess(0x1000, False, pid)
        if not handle:
            return None
        try:
            return self._handle_identity(handle)
        finally:
            self.k.CloseHandle(handle)

    def _handle_identity(self, handle):
        size = W.DWORD(32768)
        path = ctypes.create_unicode_buffer(size.value)
        times = [W.FILETIME() for _ in range(4)]
        if not self.k.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size)) or not self.k.GetProcessTimes(handle, *(ctypes.byref(t) for t in times)):
            return None
        return (os.path.normcase(str(Path(path.value).resolve())), (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime)

    def _same_image_pids(self, expected_exe):
        """Fresh exact-image discovery; an unreadable candidate is not ignored."""
        snapshot = self.k.CreateToolhelp32Snapshot(0x00000002, 0)
        if not snapshot or snapshot == ctypes.c_void_p(-1).value:
            raise ProcessIdentityChanged("Native Prozessliste konnte nicht sicher gelesen werden.")
        try:
            entry = _ProcessEntry32()
            entry.dwSize = ctypes.sizeof(entry)
            found = set()
            more = self.k.Process32FirstW(snapshot, ctypes.byref(entry))
            while more:
                if entry.szExeFile.casefold() == Path(expected_exe).name.casefold():
                    handle = self.k.OpenProcess(0x00101000, False, entry.th32ProcessID)
                    if not handle:
                        # ERROR_INVALID_PARAMETER proves that this snapshot PID
                        # no longer exists. Access denied/unknown never proves
                        # that another same-name process is a different image.
                        if ctypes.get_last_error() != 87:
                            raise ProcessIdentityChanged("Gleichnamiger Prozess konnte nicht eindeutig gelesen werden.")
                    else:
                        try:
                            status = self.k.WaitForSingleObject(handle, 0)
                            if status == 0x00000102:
                                identity = self._handle_identity(handle)
                                if not identity:
                                    raise ProcessIdentityChanged("Programmpfad eines Prozesskandidaten ist unbekannt.")
                                if identity[0] == expected_exe:
                                    found.add(int(entry.th32ProcessID))
                            elif status != 0:
                                raise ProcessIdentityChanged("Lebenszustand eines Prozesskandidaten ist unbekannt.")
                        finally:
                            self.k.CloseHandle(handle)
                more = self.k.Process32NextW(snapshot, ctypes.byref(entry))
            if ctypes.get_last_error() != 18:  # ERROR_NO_MORE_FILES
                raise ProcessIdentityChanged("Native Prozessliste ist unvollständig.")
            return found
        finally:
            self.k.CloseHandle(snapshot)

    def _udp_owner_pids(self, port):
        """Read both fresh owner-PID tables, with bounded allocation/retries."""
        owners = set()
        for family, row_type in ((2, _UdpOwnerRow4), (23, _UdpOwnerRow6)):
            size = W.DWORD()
            status = self.ip.GetExtendedUdpTable(None, ctypes.byref(size), False, family, 1, 0)
            if status not in (0, 122):  # NO_ERROR / ERROR_INSUFFICIENT_BUFFER
                raise ProcessIdentityChanged("Native UDP-Portzuordnung konnte nicht gelesen werden.")
            for attempt in range(3):
                if not 4 <= size.value <= 16 * 1024 * 1024:
                    raise ProcessIdentityChanged("Native UDP-Porttabelle hat eine ungültige Größe.")
                buffer = ctypes.create_string_buffer(size.value)
                status = self.ip.GetExtendedUdpTable(buffer, ctypes.byref(size), False, family, 1, 0)
                if status == 122:
                    continue
                if status or size.value < 4 or size.value > len(buffer):
                    raise ProcessIdentityChanged("Native UDP-Porttabelle ist unvollständig.")
                count = W.DWORD.from_buffer_copy(buffer.raw[:4]).value
                stride = ctypes.sizeof(row_type)
                if count > (size.value - 4) // stride:
                    raise ProcessIdentityChanged("Native UDP-Porttabelle enthält unvollständige Einträge.")
                for index in range(count):
                    row = row_type.from_buffer_copy(buffer.raw[4 + index * stride:4 + (index + 1) * stride])
                    if socket.ntohs(row.port & 0xffff) == port:
                        owners.add(int(row.pid))
                break
            else:
                raise ProcessIdentityChanged("Native UDP-Porttabelle änderte sich während der Prüfung.")
        return owners

    def probe_owner(self, pid, identity, *, expected_exe, port):
        """Fast, uncached guard for a probe; lifecycle/window discovery stays separate.

        The original handle binds creation and liveness before/after fresh
        duplicate-image and UDP-owner reads. A different process never gains
        authorization from a PID alone, or from a cached port observation.
        """
        try:
            expected = tuple(identity)
            image = os.path.normcase(str(Path(expected_exe).resolve()))
        except (OSError, TypeError, ValueError) as exc:
            raise ProcessIdentityChanged("Ungültige erwartete Testprozessidentität.") from exc
        if (type(pid) is not int or pid <= 0 or type(port) is not int or not 1 <= port <= 65535 or
                len(expected) != 2 or expected[0] != image or type(expected[1]) is not int or expected[1] <= 0):
            raise ProcessIdentityChanged("Ungültiger Testprozess, Port oder erwartete Programmpfad-/Startzeitbindung.")
        handle = self.k.OpenProcess(0x00101000, False, pid)
        if not handle:
            raise ProcessIdentityChanged("Eigener Testprozess konnte nicht frisch identifiziert werden.")
        try:
            if self._handle_identity(handle) != expected or self.k.WaitForSingleObject(handle, 0) != 0x00000102:
                raise ProcessIdentityChanged("Testprozess beendet oder PID/Startzeit ersetzt.")
            if self._same_image_pids(image) != {pid}:
                raise ProcessIdentityChanged("Keine eindeutige Instanz des exakten Testprogramms vorhanden.")
            if self._udp_owner_pids(port) != {pid}:
                raise ProcessIdentityChanged("Testport gehört nicht eindeutig zum gebundenen Testprozess.")
            if self._handle_identity(handle) != expected or self.k.WaitForSingleObject(handle, 0) != 0x00000102:
                raise ProcessIdentityChanged("Testprozess während der Portprüfung beendet oder verändert.")
            return dict(pid=pid, identity=list(expected), owns_port=True,
                        verification="native_process_snapshot_udp_table_v1")
        finally:
            self.k.CloseHandle(handle)

    def terminate(self, pid, identity, *, expected_exe):
        """Nur nach ausdrücklichem Stop-Auftrag; derselbe Handle verhindert PID-Reuse."""
        expected = tuple(identity)
        if (len(expected) != 2 or expected[0] != os.path.normcase(str(Path(expected_exe).resolve())) or
                type(pid) is not int or pid <= 0):
            raise ProcessIdentityChanged("Beenden verweigert: falsche erwartete Testprozessidentität.")
        handle = self.k.OpenProcess(0x1000 | 0x0001 | 0x00100000, False, pid)
        if not handle:
            raise RuntimeError("Eigener Testprozess konnte nicht sicher zum Beenden geöffnet werden.")
        try:
            if self._handle_identity(handle) != expected:
                raise ProcessIdentityChanged("Beenden verweigert: Testprozess beendet oder PID ersetzt.")
            if not self.k.TerminateProcess(handle, 1):
                raise RuntimeError("Beenden des identifizierten Testprozesses nicht bestätigt.")
            if self.k.WaitForSingleObject(handle, 5000) != 0:
                raise RuntimeError("Identifizierter Testprozess wurde noch nicht vollständig beendet.")
            return {"pid": pid, "identity": list(expected), "terminated": True}
        finally:
            self.k.CloseHandle(handle)

