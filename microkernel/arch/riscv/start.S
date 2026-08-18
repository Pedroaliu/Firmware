#include "microkernel/arch/riscv/hart_layout_defs.h"

/*
 * These variables must live in the firmware image (.data), NOT .bss.
 *
 * Secondary harts need to use them before the boot hart clears .bss.
 * Putting them in .bss would create a bootstrapping race:
 *
 *   secondary reads boot flag
 *        ||
 *   boot hart is still clearing the same storage
 */
.section .data.boot_sync, "aw", @progbits

.balign 4
.global jixia_boot_next_secondary_slot
.type jixia_boot_next_secondary_slot, @object

jixia_boot_next_secondary_slot:
    /*
    * Slot 0 is permanently reserved for the platform boot hart.
    * Secondary harts atomically claim slots starting at 1.
    */

    .word 1

.size jixia_boot_next_secondary_slot, . - jixia_boot_next_secondary_slot

.balign 4
.global jixia_boot_release
.type jixia_boot_release, @object

jixia_boot_release:
    /*
    * 0 = global state is not ready
    * 1 = .bss/global initialization is complete
    */
    .word 0

.size jixia_boot_release, . - jixia_boot_release

.balign 4
.global jixia_executive_release
.type jixia_executive_release, @object

jixia_executive_release:
    /*
     * Hostboot-shaped second release gate:
     *
     *   0 = per-hart executive objects are not ready
     *   1 = CpuManager, idle tasks, scheduler, and init task are ready
     */
    .word 0

.size jixia_executive_release, . - jixia_executive_release

/*
 * Private per-hart early/kernel stacks.
 *
 * Keep these OUTSIDE .bss. A secondary hart may already be executing on
 * its stack while hart 0 is clearing .bss.
 *
 * @nobits means the raw firmware image does not waste 64 KiB storing zeros.
 */

 .section .stack.harts, "aw", @nobits

 .balign HART_BOOT_STACK_ALIGNMENT

 .global __hart_stacks_start
 __hart_stacks_start:

    .space HART_MAX_COUNT * HART_BOOT_STACK_SIZE

.global __hart_stacks_end
__hart_stacks_end:

/*
 * Dedicated trusted runtime M-mode trap stacks.
 *
 * Once HartLocal is installed, every M-level trap switches here before
 * constructing a TrapFrame. This keeps normal M call frames and trap frames
 * in separate ownership domains.
 */
.section .stack.harts.trap, "aw", @nobits

.balign HART_TRAP_STACK_ALIGNMENT
.global __hart_trap_stacks_start
__hart_trap_stacks_start:

    .space HART_MAX_COUNT * HART_TRAP_STACK_SIZE

.global __hart_trap_stacks_end
__hart_trap_stacks_end:

.section .text.boot, "ax", @progbits

.global _start
.type _start, @function

/*
 * QEMU virt entry:
 *
 *   a0 = architectural hart ID
 *   a1 = device tree blob address
 *
 * M00-05 keeps hart 0 as the platform boot hart.
 *
 * Important:
 *   hart ID != HartLocal index.
 */

 _start:
    /*
     * Nothing below is ready for asynchronous traps yet.
     */
     csrci mstatus, 8
     csrw mie, zero

     /*
      * Explicitly mark the per-hart HartLocal anchor as not installed.
      *
      * hart::initialize() later publishes HartLocal* into mscratch. The
      * privilege-aware trap entry treats zero as the early-M bootstrap state
      * and never relies on an implementation-specific reset value.
      */
     csrw mscratch, zero

     /*
      * Every hart needs a valid gp before entering C/C++.
      *
      * Prevent relaxation because gp itself is not valid yet.
      */

      .option push
      .option norelax

      la gp, __global_pointer$

      .option pop

     /*
      * Dense runtime stack slot allocation.
      *
      * Boot hart:
      *      hart_id == 0
      *      slot    == 0
      *
      * Secondary:
      *      atomically claims slot 1..N-1
      *
      * This deliberately avoids:
      *
      *      slot = hart_id
      */
      beqz a0, .Lboot_slot

     /*
      * Atomic fetch-and-add.
      *
      * t2 receives the OLD value, which is this hart's unique slot.
      *
      * Atomicity is needed here for uniqueness.
      * Memory ordering is NOT the purpose of this AMO; release/acquire comes
      * later when global state is published.
      */

      la t0, jixia_boot_next_secondary_slot
      li t1, 1
      amoadd.w t2, t1, (t0)

      li t3, HART_MAX_COUNT
      bgeu t2, t3, .Lhart_overflow_park

      mv s0, t2
      j .Lstack_init

 .Lboot_slot:

      li s0, 0

 .Lstack_init:

    /*
     * stack_bottom =
     *     __hart_stacks_start + slot * HART_BOOT_STACK_SIZE
     *
     * stack grows downward, so initial sp is stack_bottom + size.
     */

     la t0, __hart_stacks_start
     li t1, HART_BOOT_STACK_SIZE

     mul  t2, s0, t1
     add  t0, t0, t2

     add sp, t0, t1

     /*
      * Defensive ABI alignment.
      */
     andi sp, sp, -HART_BOOT_STACK_ALIGNMENT

     /*
      * Only architectural hart 0 performs global initialization.
      */
     beqz a0, .Lboot_global_init

 /*
  * Secondary hart rendezvous.
  *
  * It is already executing on its OWN stack, but it must not touch .bss or
  * enter C++ until the boot hart publishes global initialization completion.
  */
 .Lsecondary_wait:

     la t0, jixia_boot_release

 .Lsecondary_poll:

     lw   t1, 0(t0)
     beqz t1, .Lsecondary_poll

     /*
      * Acquire side:
      *
      * the release flag load must be observed before later reads/writes
      * performed by this hart.
      */
     fence r, rw

     j .Lhart_runtime_entry

 /*
  * Boot-hart-only global initialization.
  */
 .Lboot_global_init:

     la t0, __bss_start
     la t1, __bss_end


 .Lclear_bss:

     bgeu t0, t1, .Lbss_done

     sd zero, 0(t0)
     addi t0, t0, 8

     j .Lclear_bss


 .Lbss_done:

     /*
      * Release side:
      *
      * All preceding global-memory writes, especially .bss zeroing,
      * must become ordered before publishing jixia_boot_release = 1.
      */
     fence rw, w

     la t0, jixia_boot_release
     li t1, 1
     sw t1, 0(t0)

 /*
  * From this point forward:
  *
  *   - this hart has a private stack;
  *   - .bss is initialized;
  *   - the C/C++ global environment may be used.
  */
 .Lhart_runtime_entry:

     /*
      * mtvec is a per-hart CSR. Every participating hart must install it.
      */
     la   t0, jixia_trap_entry
     csrw mtvec, t0


     /*
      * Third C ABI argument:
      *
      *   a2 = dense hart index
      */
     mv a2, s0


     /*
      * Keep the existing QEMU contract:
      *
      *   hart ID 0 = boot hart
      *
      * This also preserves the M00-04 hart0 timer regression.
      */
     beqz a0, .Lcall_boot_main


     call jixia_microkernel_secondary_main

     /*
      * Secondary entry is [[noreturn]].
      */
 .Lsecondary_returned:

     wfi
     j .Lsecondary_returned


 .Lcall_boot_main:

     call jixia_microkernel_boot_main

     /*
      * Boot entry is also [[noreturn]].
      */
 .Lboot_returned:

     wfi
     j .Lboot_returned


 .Lhart_overflow_park:

     /*
      * More harts arrived than this early runtime can represent.
      *
      * Do not let an unrepresented hart enter C/C++ or borrow another
      * hart's stack.
      */
     wfi
     j .Lhart_overflow_park


 .size _start, . - _start


/*
 * The executive gate lives in start.S because its ordering is part of the
 * reset-to-runtime ISA contract. C++ invokes these small helpers only after
 * every hart already owns a stack and HartLocal anchor.
 */
.balign 4
.global jixia_wait_for_executive_release
.type jixia_wait_for_executive_release, @function

jixia_wait_for_executive_release:
    la t0, jixia_executive_release

.Lexecutive_release_poll:
    lw t1, 0(t0)
    beqz t1, .Lexecutive_release_poll
    fence r, rw
    ret

.size jixia_wait_for_executive_release, . - jixia_wait_for_executive_release


.balign 4
.global jixia_release_executive_harts
.type jixia_release_executive_harts, @function

jixia_release_executive_harts:
    fence rw, w
    la t0, jixia_executive_release
    li t1, 1
    sw t1, 0(t0)
    ret

.size jixia_release_executive_harts, . - jixia_release_executive_harts
