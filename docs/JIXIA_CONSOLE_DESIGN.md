# Jixia Console / Kernel Print Architecture

**Status:** accepted kernel-print baseline  
**Date:** 2026-08-07  
**Scope:** Mozi kernel diagnostic output now; richer service console later

## 1. Decision

Jixia separates the minimum kernel diagnostic path from the future runtime Console Service.

```text
RESET / catastrophic fallback
        |
        v
raw polling UART primitive
        ^
        |
Mozi printk
        |
shared formatter
        |
KernelLogBuffer
        |
        +---- temporary QEMU UART mirror

------------------------------------------------ future service boundary

service/user caller
        |
shared formatter
        |
message / bounded queue
        |
Console Service
        |
        +---- UART
        +---- screen
        +---- BMC / SOL
        +---- Jingjie
        `---- other runtime transports
```

The kernel path must remain usable without tasks, IPC, heap allocation, scheduler progress, or a console daemon.

## 2. Current kernel API

Normal Mozi code uses:

```cpp
jixia::microkernel::printk("mcause=%p mepc=%p\n", ...);
```

Low-level `uart_putc()` remains below this API for reset-time and catastrophic fallback. Normal kernel/test code should not directly format output through UART helpers.

## 3. Shared formatter

Formatting is hardware-independent and lives in:

```text
lib/format.{h,cpp}
```

`jixia::format::vformat()` writes characters through a generic `Writer`, allowing reuse by `printk` and future buffer/service frontends.

Current conversions:

```text
%%  %c  %s
%d  %i  %u
%o  %x  %X
%b  %B  %p
```

Current length modifiers:

```text
hh  h  l  ll  z  t
```

Current flags/width:

```text
#  0  -  +  space
numeric field width
```

Not claimed:

- floating-point formatting;
- precision;
- locale;
- hosted stdio;
- full ISO-C `printf` compatibility.

## 4. KernelLogBuffer

The authoritative early kernel diagnostic store is fixed and allocation-free:

```text
capacity        36 KiB
allocation      static / BSS
write policy    append-only
overflow        retain old data and set truncated flag
heap            none
```

Append-only behavior deliberately preserves early boot evidence. Runtime trace/debug and RAS logs may use different storage semantics later.

Likely future split:

```text
Kernel early log        append-only
Runtime trace/debug     per-hart/ring storage
RAS event log           structured/persistent records
```

## 5. UART mirror

During QEMU bring-up every kernel-log character may also be mirrored to the existing polling UART.

```text
printk
   |
   +---- KernelLogBuffer       authoritative
   |
   `---- raw UART mirror       bring-up visibility
```

The mirror is a backend convenience, not the long-term console architecture.

## 6. Failure-domain rule

A Console Service or device backend can itself fail. Fatal diagnostics must not recursively depend on the failing service.

```text
Console Service failure
        |
        v
kernel printk / KernelLogBuffer
        |
        `---- raw emergency primitive when safe
```

For that reason, the minimal kernel print path remains independent even after a richer service console exists.

## 7. Current source layout

```text
lib/
    format.h
    format.cpp

microkernel/console/
    kernel_console.h
    kernel_console.cpp
    printk.h
    printk.cpp

microkernel/core/
    kernel_print_test.cpp

platform/qemu_virt/
    uart.c
    uart.h

scripts/
    test-kernel-print.sh
```

## 8. Integrated regression order

The stable bring-up image keeps completed foundations live in one sequence:

```text
Kernel Print test
    -> Recoverable EBREAK/C.EBREAK test
    -> Machine Timer interrupt test
    -> TrapFrame capture test (parks hart)
```

`test-kernel-print.sh` therefore checks:

```text
KERNEL_PRINT_TEST: PASS
RECOVERABLE_TRAP_TEST: PASS
MACHINE_TIMER_TEST: PASS
TRAP_FRAME_TEST: PASS
```

This prevents console integration from silently regressing the trap/timer foundation.

## 9. Concurrency gate

The current buffer/output path was designed and first accepted on the single-hart baseline. It is not yet claimed to be multi-writer safe.

M00-05 must define the multi-hart policy before secondary harts start using normal `printk` concurrently.

Preferred direction is ownership before synchronization:

- per-hart staging/state where practical;
- avoid one giant global lock in trap/panic paths;
- use an established synchronization algorithm only where true shared mutation is required;
- document RISC-V/C++ memory-order assumptions explicitly.

See `docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md`.

## 10. Future Console Service

Do not build the richer runtime service merely to make Console look complete. Resume it only after Jixia has:

```text
task/thread execution
IPC/message primitive
service lifecycle
allocator/runtime suitable for services
device ownership model
```

Then consider:

- bounded message queues and drop accounting;
- logical DEFAULT/DEBUG channels independent of physical devices;
- UART runtime device object;
- screen/framebuffer backend;
- BMC/SOL backend;
- Jingjie backend;
- service-level flush barrier;
- handoff/readout of the early KernelLogBuffer.

## 11. Console versus Trace and RAS

These are related but distinct contracts:

```text
Console       human-readable interactive/status text
Kernel print  minimal trusted diagnostic text
Trace         high-volume execution/observability records
RAS event     structured machine/service failure record
```

A structured RAS event may be rendered through Console, but console text must not become the RAS ABI.

## 12. Reference-design lesson

The Hostboot source study led to two durable decisions:

1. kernel printing and `usr/console` belong to different execution layers and failure domains;
2. one generic formatter can serve multiple frontends without importing a large hosted runtime.

Jixia uses Hostboot as a design reference rather than importing its libc/runtime wholesale.
