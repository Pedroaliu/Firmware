# Jixia Console / Kernel Print Architecture

**Status:** design baseline accepted for staged implementation  
**Date:** 2026-08-07  
**Current branch:** `feature/console-foundation`  
**Current work item:** `F00-01 Kernel print foundation`  
**Integration base:** completed `M00-03 Recoverable trap and mret`

## 1. Decision summary

Jixia Console is intentionally split by execution layer and lifetime.

The current work implements **only the Mozi microkernel print path**. The richer firmware/user console service is deliberately deferred until Jixia has a real service/user execution environment.

```text
RESET / catastrophic fallback
        |
        v
raw polling UART primitive
        ^
        |
Mozi kernel printk
        |
shared formatter
        |
KernelLogBuffer
        |
        +---- optional temporary UART mirror during bring-up

------------------------------------------------------------ future boundary

usr / service code
        |
display / displayf / future console::out
        |
shared formatter
        |
message / queue
        |
Console Service / daemon
        |
        +---- UART
        +---- Screen
        +---- BMC/SOL
        +---- Jingjie
        +---- other runtime transports
```

This file is the canonical Console design record. Future work should start from the decisions and TODOs below rather than repeating the Hostboot/UEFI/seL4/NXP source-study discussion.

## 2. Why kernel and user Console are separate

The key conclusion from OpenPOWER Hostboot is that `src/kernel/console.C` and `src/usr/console/*` are different subsystems, not merely two backends of one object.

### 2.1 Hostboot kernel path

Hostboot kernel output is deliberately small:

```text
kernel caller
   |
printk(...)
   |
Util::vasprintf(...)
   |
Console::putc(...)
   |
kernel_printk_buffer
```

Important properties:

- fixed memory storage;
- no console daemon;
- no message queue;
- no heap requirement in the print path;
- no UART driver object requirement;
- usable as a fallback when higher-level console code fails.

The kernel buffer is append-only: once full, it stops accepting new characters rather than overwriting early boot history.

### 2.2 Hostboot usr path

Hostboot `src/usr/console` is a runtime service:

```text
caller
   |
display/displayf
   |
format to temporary buffer
   |
timestamp + message
   |
g_msgq
   |
consoleDaemon
   |
VUART1 / VUART2
   |
Uart object / device transport
```

The usr path assumes facilities that kernel printing must not require:

- tasks;
- message queues;
- heap allocation;
- `std::vector`;
- timestamps;
- device objects;
- runtime initialization;
- multiple logical consoles;
- synchronous `flush()` as a service-level barrier.

The daemon separates logical purpose from device transport:

```text
DEFAULT / Boot Status -> VUART1
DEBUG / Debug Trace   -> VUART2
```

A concrete UART may in turn represent a physical serial path or a BMC/SOL virtual UART.

### 2.3 Failure-domain lesson

A console service or UART driver can itself fail. A failure path must therefore not recursively depend on the same service it is diagnosing.

For Jixia:

```text
Console Service failure
        |
        +---- must not require Console Service again
        |
        v
kernel printk / kernel buffer
        |
        +---- raw emergency device primitive if safe
```

The kernel print path is therefore an independent diagnostic foundation, not merely a feature of the future service console.

## 3. Current F00-01 scope: Kernel Print Foundation

The current implementation is intentionally small.

### 3.1 Public kernel API

Normal Mozi code uses:

```cpp
printk("mcause=%lx mepc=%lx\n", frame.mcause, frame.mepc);
```

`printk` is currently a C++ kernel API in the `jixia::microkernel` namespace. A stable C ABI wrapper can be added later if imported C code needs it; cross-language exported symbols must follow the project `jixia_` naming rule.

### 3.2 Shared formatter

`printk` does not know how numbers are converted to characters. Formatting lives in a reusable freestanding utility layer:

```text
printk
   \
    +--> jixia::format::vformat --> character Writer
   /
future snprintf / displayf / service frontend
```

The formatter knows text representation, not hardware.

Initial supported conversions:

```text
%%  %c  %s
%d  %i  %u
%o  %x  %X
%b  %B
%p
```

Initial length modifiers:

```text
hh  h  l  ll  z  t
```

Initial flags/width:

```text
#  0  -  +  space
numeric field width
```

Not implemented in F00-01:

- floating point;
- precision;
- locale;
- hosted `FILE`/stdio;
- full ISO C printf compatibility.

Unknown conversion characters are emitted literally rather than causing a trap.

### 3.3 KernelLogBuffer

The authoritative kernel diagnostic store is a fixed static buffer.

Initial policy:

```text
capacity        36 KiB
allocation      static / BSS
write policy    linear append-only
overflow        retain old data, set truncated flag
heap            none
```

The first implementation follows Hostboot's early-buffer preference for preserving boot history rather than using a runtime ring.

Why not a ring yet:

- Jixia is still in bring-up;
- reset/trap/privilege sequencing is highly valuable evidence;
- runtime recent-history logging and RAS have different requirements;
- one buffer type should not be forced to serve all future logging semantics.

Possible future split:

```text
Kernel early log        append-only
Runtime trace/debug     ring/per-hart ring
RAS event log           structured/persistent
```

### 3.4 UART mirroring

During current QEMU bring-up, `printk` is mirrored to the existing raw polling UART so developers can see output immediately.

The memory buffer remains authoritative:

```text
printk
   |
   +---- KernelLogBuffer       always
   |
   +---- raw UART mirror       current bring-up convenience
```

The UART mirror is not the long-term Console architecture and can later be disabled when a service console owns normal external output.

The raw `uart_putc()` primitive remains below `printk` for:

- reset-time output before kernel print state is usable;
- lowest-level bring-up;
- failures in the formatter/kernel console itself;
- future catastrophic paths that must bypass richer infrastructure.

## 4. Current dependency model

```text
microkernel/core/*
        |
        v
microkernel/console/printk
        |
        +---------------------+
        |                     |
        v                     v
lib/format              KernelLogBuffer
                              |
                              v
                      raw platform UART
                      (temporary mirror)
```

Rules:

- normal kernel call sites do not include `uart.h`;
- formatter code never includes `uart.h`;
- no heap, task, IPC, scheduler, exception, RTTI, or iostream dependency;
- no general static-constructor runtime is required;
- current path is single-hart and non-locking.

## 5. Concurrency and panic constraints

F00-01 runs on the existing single-hart baseline.

The initial buffer is **not** claimed to be multi-writer safe. M00-05 per-hart state must revisit this.

Preferred future directions:

- per-hart kernel staging/log buffers;
- explicit atomic reservation when a global aggregate is truly needed;
- no giant global lock on a machine-check/panic path.

A future panic/emergency path must avoid dependencies on:

- heap metadata;
- scheduler state;
- task-owned locks;
- asynchronous completion;
- message queues;
- normal Console Service state.

## 6. What is deliberately not implemented now

The following design is accepted as future direction but is not part of F00-01:

```text
ConsoleRouter
multi-sink runtime routing
ConsoleSink runtime hierarchy
console::out / cout-like frontend
screen/framebuffer output
BMC/SOL output
logical DEFAULT / DEBUG console channels
console daemon/service task
message queue based output
timestamp insertion
async buffering
runtime flush barrier
USB/network console
Jingjie service transport
```

These belong to the service/user phase because they need runtime mechanisms and policy.

Keeping them out of the current microkernel follows the Jixia minimality rule: rich output policy should not enlarge Mozi merely because it can display text.

## 7. Future usr / Console Service design record

When Jixia has real services/tasks/IPC, resume from this model:

```text
usr/service caller
      |
      +---- display(...)
      +---- displayf(...)
      +---- future console::out << ...
      |
      v
shared formatter
      |
      v
Console message
  timestamp
  logical channel
  payload
      |
      v
Console Service
      |
      +---- DEFAULT / boot-status channel
      +---- DEBUG / trace channel
      |
      +---- UART backend
      +---- screen backend
      +---- BMC/SOL backend
      +---- Jingjie backend
```

### 7.1 Service requirements

The service should eventually support:

- logical channels independent of physical device IDs;
- selectable/fan-out transports;
- one broken transport not disabling all others;
- device ownership outside the minimal microkernel;
- asynchronous output where appropriate;
- bounded queues and drop accounting;
- a service-level `flush()` barrier for shutdown/reboot;
- handoff/readout of the kernel boot buffer;
- configuration to compile/disable expensive console behavior.

### 7.2 Device objects

Runtime transports with independent identity/state are good class candidates:

```text
UartDevice
FramebufferConsole
SolTransport
JingjieConsole
```

They may contain state such as base address, initialization/failure state, cursor, protocol channel, queue state, or connection state.

The kernel does not instantiate these runtime objects.

### 7.3 Future cout-like frontend

A Jixia `console::out << ...` frontend may be added when the service layer exists.

It must remain syntax sugar over shared formatting/service transport. It must not become the architecture itself and must not pretend to be `std::cout`.

## 8. Console versus Log / Trace / RAS

These concepts are related but not interchangeable.

```text
Console
    human-readable interactive/status text

Kernel print
    minimal trusted diagnostic text

Trace
    high-volume execution/observability records

RAS event
    structured machine/service failure record
```

A future RAS record may contain:

```text
timestamp
hart
component
severity
event_id
address
syndrome
recovery_action
```

It may be rendered to Console, but human-readable text must never become the structured RAS ABI.

Jingjie may consume both console text and structured events, but the interfaces remain distinct.

## 9. Hostboot src.zip library review

The supplied Hostboot `src.zip` contains `lib/` and `libc++/` support code. Jixia should not import it wholesale.

### 9.1 Needed now: formatter idea

`lib/sprintf.C` is directly relevant.

Useful design ideas:

- one formatter writes through a generic character receiver;
- kernel `printk` and future `snprintf`/service output can reuse it;
- integer/string/pointer formatting stays independent of hardware.

Jixia implements its own smaller freestanding formatter rather than copying Hostboot's complete implementation.

### 9.2 Useful later, not now

`lib/stdio.C`

- shows how `sprintf/snprintf/vsprintf/vsnprintf` can reuse the same formatter through a buffer writer;
- worth adding when Jixia has real callers needing formatted strings;
- not necessary to implement `printk`.

`lib/string.C`, `string_ext.C`, `string_utils.C`

- useful reference for a future minimal libc/string layer;
- Jixia will eventually need selected `memcpy/memset/memmove/strlen/...`;
- do not add unrelated libc surface merely for Console.

`lib/ctype.C`

- tiny character classification helpers;
- formatter can avoid this dependency for now;
- add only when a broader libc layer needs it.

`lib/assert.C`

- useful later when Jixia defines assert/panic/terminate policy;
- should build on the kernel emergency diagnostics, not precede them.

`libc++/builtins.C`, `rt_builtins.C`

- relevant when Jixia intentionally supports `new/delete`, static-local guards, destructors, or a richer C++ runtime;
- not needed now because current code avoids heap objects and thread-safe static initialization.

### 9.3 Explicitly deferred until matching subsystems exist

`lib/stdlib.C`

- tied to Hostboot heap/page/VMM behavior;
- not portable to Jixia's allocator design;
- revisit only after Jixia has its own allocator/runtime policy.

`lib/sync.C`, `syscall_*.C`

- depend on tasks, syscalls, messages, MMIO and synchronization;
- revisit with service/task/IPC milestones.

`lib/tls.C`, `tlsrt.C`

- tied to thread-local runtime;
- revisit after real task/thread runtime exists.

`lib/errno.C`

- useful only after a broader C/POSIX-like service ABI needs errno semantics.

`math.C`, `crc32.C`, `random.C`, `rand.S`, `splaytree.C`

- unrelated to current kernel printing;
- adopt only if future functionality independently requires them.

### 9.4 Rule for Hostboot libraries

Use Hostboot as a design/source reference, not as a menu of files to import.

For each future library:

```text
need appears
   |
define Jixia contract
   |
compare Hostboot / UEFI / seL4 / other references
   |
implement only required semantics
   |
test in Jixia
```

## 10. F00-01 source layout

Target source layout:

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

The old experimental runtime `ConsoleRouter`/multi-sink implementation is intentionally removed from the current branch. Its design intent is preserved in this document for the future service phase.

## 11. F00-01 acceptance criteria

F00-01 is DONE only when all are true:

```text
[ ] GNU riscv64-unknown-elf build succeeds
[ ] QEMU boot banner is emitted through printk UART mirror
[ ] printk formatting probe matches exact KernelLogBuffer bytes
[ ] append-only buffer reports no unexpected truncation
[ ] KERNEL_PRINT_TEST: PASS
[ ] M00-03 RECOVERABLE_TRAP_TEST: PASS
[ ] M00-02 TRAP_FRAME_TEST: PASS
[ ] dedicated script reports Kernel print test: PASS
[ ] normal microkernel/test code no longer calls uart_puts/uart_put_hex_uintptr
[ ] raw uart_putc remains available below printk
[ ] no timer code is required by this branch
```

Acceptance command:

```bash
bash scripts/test-kernel-print.sh
```

Expected core markers:

```text
[Jixia][Test][KernelPrint]
probe      : s=ok d=-42 u=42 x=00001a2b p=0x0000000000001234 %
buffer     : append-only kernel log retained exact probe
capacity   : 36864 bytes
KERNEL_PRINT_TEST: PASS

RECOVERABLE_TRAP_TEST: PASS
TRAP_FRAME_TEST: PASS
Kernel print test: PASS
```

## 12. TODO after F00-01

Do **not** start these merely to make Console look complete.

### Near-term kernel TODO

```text
[ ] M00-05: define per-hart kernel-print ownership/concurrency
[ ] panic/assert path: define emergency output contract
[ ] expose kernel log metadata through future debug/introspection ABI
[ ] decide when normal UART mirroring is disabled
```

### Service-console gate

Resume the usr/service Console work only after Jixia has:

```text
task/thread execution
IPC/message primitive
service lifecycle
allocator/runtime suitable for services
device ownership model
```

Then implement:

```text
[ ] Console Service API
[ ] message queue / bounded buffering
[ ] logical DEFAULT and DEBUG channels
[ ] UART runtime device class
[ ] screen backend
[ ] BMC/SOL backend
[ ] Jingjie backend
[ ] service flush barrier
[ ] optional console::out frontend
[ ] kernel-log handoff/readout
```

### Logging/RAS gate

Later, independently define:

```text
[ ] structured log/event ABI
[ ] severity/component/event IDs
[ ] per-hart/central aggregation policy
[ ] persistent RAS storage/handoff
[ ] rendering structured events to Console
[ ] Jingjie structured-event transport
```

## 13. Design invariants not to forget

1. Kernel print and usr Console Service are different failure domains.
2. Kernel `printk` must not depend on the future Console Service.
3. Formatting is reusable and transport-independent.
4. Kernel log storage is authoritative; UART is currently a mirror.
5. Raw device output remains below the formatted path.
6. The current kernel buffer is append-only; ring semantics are a separate runtime decision.
7. Rich routing/device policy belongs in services when services exist.
8. `console::out` is optional future syntax sugar, not an architecture.
9. Console text is not the RAS/trace ABI.
10. Hostboot code is a reference; Jixia imports only mechanisms it actually needs.
