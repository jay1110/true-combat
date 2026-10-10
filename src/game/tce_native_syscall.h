/* Native ET/ETLegacy syscall ABI. All variadic slots are pointer-sized. */
#ifndef TCE_NATIVE_SYSCALL_H
#define TCE_NATIVE_SYSCALL_H
#define TCE_VM_CALL_END ((intptr_t)-1337)
#define TCE_SYSCALL_0(op) syscall((intptr_t)(op), TCE_VM_CALL_END)
#define TCE_SYSCALL_1(op, a1) syscall((intptr_t)(op), (intptr_t)(a1), TCE_VM_CALL_END)
#define TCE_SYSCALL_2(op, a1, a2) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), TCE_VM_CALL_END)
#define TCE_SYSCALL_3(op, a1, a2, a3) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), TCE_VM_CALL_END)
#define TCE_SYSCALL_4(op, a1, a2, a3, a4) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), TCE_VM_CALL_END)
#define TCE_SYSCALL_5(op, a1, a2, a3, a4, a5) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), TCE_VM_CALL_END)
#define TCE_SYSCALL_6(op, a1, a2, a3, a4, a5, a6) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), TCE_VM_CALL_END)
#define TCE_SYSCALL_7(op, a1, a2, a3, a4, a5, a6, a7) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), TCE_VM_CALL_END)
#define TCE_SYSCALL_8(op, a1, a2, a3, a4, a5, a6, a7, a8) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), TCE_VM_CALL_END)
#define TCE_SYSCALL_9(op, a1, a2, a3, a4, a5, a6, a7, a8, a9) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), TCE_VM_CALL_END)
#define TCE_SYSCALL_10(op, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), TCE_VM_CALL_END)
#define TCE_SYSCALL_11(op, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), TCE_VM_CALL_END)
#define TCE_SYSCALL_12(op, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), TCE_VM_CALL_END)
#define TCE_SYSCALL_13(op, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), (intptr_t)(a13), TCE_VM_CALL_END)
#define TCE_SYSCALL_14(op, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), (intptr_t)(a13), (intptr_t)(a14), TCE_VM_CALL_END)
#define TCE_SYSCALL_15(op, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), (intptr_t)(a13), (intptr_t)(a14), (intptr_t)(a15), TCE_VM_CALL_END)
#define TCE_SYSCALL_16(op, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16) syscall((intptr_t)(op), (intptr_t)(a1), (intptr_t)(a2), (intptr_t)(a3), (intptr_t)(a4), (intptr_t)(a5), (intptr_t)(a6), (intptr_t)(a7), (intptr_t)(a8), (intptr_t)(a9), (intptr_t)(a10), (intptr_t)(a11), (intptr_t)(a12), (intptr_t)(a13), (intptr_t)(a14), (intptr_t)(a15), (intptr_t)(a16), TCE_VM_CALL_END)

#endif
