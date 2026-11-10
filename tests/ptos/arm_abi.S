/* A minimal test monitor for QEMU virt/Cortex-A7. Native call code executes
 * in USR mode; only the test's trap handler runs in SVC mode. No pTOS kernel
 * or guest RAM/address-space implementation is simulated here. */
.syntax unified
.arch armv7-a
.arm
.section .text.boot,"ax",%progbits
.global _start
_start:
    cpsid if
    cps #0x13
    ldr sp, =0x47f00000
    ldr r0, =vectors
    mcr p15, 0, r0, c12, c0, 0
    isb
    mov r0, #0xd0
    msr spsr_cxsf, r0
    ldr lr, =user_start
    movs pc, lr
user_start:
    ldr sp, =0x47e00000
    bl arm_abi_user
    b .

.section .vectors,"ax",%progbits
.align 5
vectors:
    b unexpected
    b unexpected
    b svc_handler
    b unexpected
    b unexpected
    b unexpected
    b unexpected
    b unexpected
svc_handler:
    sub sp, sp, #56
    stmia sp, {r0-r12, lr}
    mov r0, sp
    mrs r1, spsr
    bl arm_abi_dispatch
    str r0, [sp]
    ldmia sp, {r0-r12, lr}
    add sp, sp, #56
    movs pc, lr
unexpected:
    ldr sp, =0x47f00000
    bl arm_abi_failure
    b .
.section .note.GNU-stack,"",%progbits
