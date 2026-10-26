#ifndef TCE_COMPAT_ABI_H
#define TCE_COMPAT_ABI_H

/* Class B/C: engine syscall packing, not an operating-system abstraction.
 * Include the module's SDK-facing declarations before expanding these macros.
 * Native dllEntry uses intptr_t variadic slots; WASM ABI1 uses an argument
 * vector. They are distinct contracts, not interchangeable implementations.
 * vmMain/dllEntry signatures, QDECL and SDK public layouts stay unchanged.
 * ET 2.60b's 32-bit engine cannot load a native 64-bit module. */
#include "legacy/game/tce_native_syscall.h"

#endif
