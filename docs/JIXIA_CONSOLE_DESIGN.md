# Jixia Console Architecture

**Status:** initial standalone design baseline  
**Date:** 2026-08-07  
**Branch:** `feature/console-foundation`  
**Integration base:** completed `M00-03 Recoverable trap and mret`  
**Scope:** firmware console, diagnostic output, output routing, and future logging transport foundations

## 1. Scope and sequencing

Console is a standalone platform feature. It is not part of the machine-timer interrupt milestone.

The development sequence is intentionally:

```text
M00-03 completed baseline
        |
        +--> feature/console-foundation   <-- current work
        |
        +--> milestone/m00-04-timer-interrupt
                 parked while Console is validated
```

The timer implementation is preserved on its own branch and will be resumed after the Console foundation is accepted. This avoids mixing two independent concerns in one milestone and keeps regression evidence attributable to one feature at a time.

## 2. Problem statement

Jixia must not let firmware code depend directly on one UART helper such as `uart_puts()`.

A server firmware platform eventually needs output to several destinations with very different initialization and failure properties:

- polling UART immediately after reset;
- an in-memory boot/debug history for postmortem analysis;
- framebuffer or text display output;
- BMC Serial-over-LAN (SOL) or another management transport;
- simulator/debug channels for Jingjie;
- future structured RAS/event records that may be rendered as text but must not depend on text parsing.

The architectural problem is therefore not merely replacing `uart_puts()` with prettier syntax. The goal is to separate:

```text
frontend syntax
formatting
routing
buffering
transport/device access
failure policy
```

so that each can evolve independently.

## 3. Reference-system lessons

The design is informed by Hostboot, EDK II, seL4, and NXP MCUXpresso.

### 3.1 OpenPOWER Hostboot

Hostboot separates formatting from the destination. Kernel `printk()` formats through `Util::vasprintf()` into a generic character-output interface, while the kernel `Console` stores output in a fixed memory buffer. Higher-level console code can later forward formatted data through richer services.

Jixia adopts these ideas:

- diagnostic history exists independently of a physical serial port;
- formatting consumes a generic character sink;
- the earliest path requires no heap or scheduler;
- later console services may be richer than the kernel emergency path.

Jixia does **not** directly adopt Hostboot's `Singleton<Console>` model. Jixia currently has no general static-constructor bootstrap comparable to Hostboot's C++ bootstrap, and panic/multi-hart paths should not depend on hidden singleton initialization.

Relevant Hostboot references:

```text
src/include/kernel/console.H
src/kernel/console.C
src/include/util/sprintf.H
src/usr/console/console.C
```

### 3.2 EDK II

EDK II demonstrates that a logical debug frontend can be connected to different backends such as serial output, UEFI console output, status-code reporting, or null output.

Jixia adopts the same backend independence without importing the full UEFI library/protocol model into the microkernel.

### 3.3 seL4

seL4 provides the constraint on what belongs in the trusted kernel. Debug printing is useful, but rich device stacks are not a reason to enlarge the microkernel.

For Jixia:

- the kernel may retain a small console-routing/buffering mechanism;
- polling output remains available for bring-up and panic;
- complex display, USB, network, or BMC services should eventually live outside the minimum microkernel when the service architecture exists.

### 3.4 NXP MCUXpresso

NXP's debug-console organization reinforces explicit separation between string formatting, logging/buffering, and physical I/O, and recognizes that backends have different blocking characteristics.

## 4. Architectural model

The target model is:

```text
                         Jixia output architecture

                +--------------------------------+
                |            Frontends           |
                |                                |
                | console::out << ...            |
                | future printk(...)             |
                | future log::info(...)          |
                +---------------+----------------+
                                |
                                v
                +--------------------------------+
                |            Formatter           |
                | text / integer / hex / pointer |
                +---------------+----------------+
                                |
                                v
                +--------------------------------+
                |          Console Router        |
                | normal / emergency routing     |
                +----+------------+----------+----+
                     |            |          |
                     v            v          v
                Memory Sink    UART Sink   future sinks
                     |                       |   |   |
                     |                       |   |   +--> Jingjie
                     |                       |   +------> BMC/SOL
                     |                       +----------> Screen
                     v
              postmortem/debug/RAS handoff
```

`console::out` is only a frontend. It is not the architecture itself.

## 5. Design principles

### 5.1 Formatting and transport are independent

Integer/hex conversion must not know UART addresses. UART register code must not parse format strings or hold stream formatting state.

### 5.2 Memory output is a first-class sink

UART is not the authoritative history store. A fixed in-memory ring retains recent console output for debugger, simulator, crash-dump, or later management/RAS consumers.

### 5.3 Multiple sinks share one router

Normal output may fan out to multiple sinks. Failure or absence of a future SOL/display sink must not disable UART or memory output.

### 5.4 Console text and structured logging are distinct

Console is human-facing text. Future structured RAS/log records need stable fields such as:

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

A structured record may be rendered through Console, but human-readable text must never become the RAS ABI.

### 5.5 Early, runtime, and panic paths have different contracts

A backend advertises capabilities. The earliest and panic paths must not require:

- heap allocation;
- scheduler services;
- asynchronous completion;
- normal locks whose owner might have failed;
- interrupt-driven device stacks.

### 5.6 No heap allocation in the initial console core

The router uses a fixed-capacity sink table. The memory sink uses static storage. Formatting uses small stack scratch buffers.

### 5.7 No dependency on `std::iostream`

Jixia is freestanding and currently links with `-nostdlib` and `-nostartfiles`. Jixia therefore implements a lightweight `ConsoleStream` with familiar `operator<<` syntax but does not claim `std::cout` compatibility.

### 5.8 No mandatory virtual dispatch in the initial implementation

The initial `ConsoleSink` is a small class containing:

```text
context pointer
put-character callback
optional flush callback
capability bits
```

This provides an object-like backend interface without requiring vtables, heap allocation, or a general C++ runtime bootstrap. The choice can be revisited later without changing the frontend/router model.

## 6. Output phases

### 6.1 Phase 0: reset / pre-console

Before the router exists, raw platform output remains available:

```text
reset / earliest failure
        |
        v
raw polling UART primitive
```

The existing `uart_putc()`/raw UART layer is therefore not deleted. It becomes a device primitive below Console.

### 6.2 Phase 1: firmware runtime console

When the platform enters the C++ firmware runtime, platform initialization attaches at least:

```text
MemorySink
UartSink
```

Normal firmware code then uses:

```cpp
console::out
    << "hart=" << console::hex(hart_id) << '\n';
```

### 6.3 Phase 2: service console

Later the system may add:

```text
FramebufferSink
SolSink
UsbDebugSink
NetworkSink
JingjieSink
```

These may be asynchronous or service-backed and must not automatically become panic-safe or microkernel-resident.

## 7. Panic and RAS path

A panic or machine-check path may execute while heap, scheduler, locks, or normal service state is damaged.

Console therefore has two routes:

```text
normal
emergency
```

Emergency output is delivered only to sinks marked `panic_safe`.

For the initial QEMU implementation:

```text
MemorySink  panic_safe
UartSink    panic_safe, blocking/polling
```

A future SOL sink is **not** panic-safe by default because it may rely on queues, BMC protocol state, interrupts, or asynchronous transport.

Raw UART remains below the router as the last-resort path for faults that occur before Console initialization itself.

## 8. Core object model

### 8.1 `ConsoleSink`

`ConsoleSink` describes one output target.

Initial capabilities:

```text
EARLY_SAFE
PANIC_SAFE
BLOCKING
RUNTIME_ONLY
```

The first implementation uses `PANIC_SAFE` for emergency routing and records the other capabilities for later policy.

### 8.2 `ConsoleRouter`

The router owns no devices. It stores references to a fixed number of sink descriptors.

Responsibilities:

- attach sinks;
- fan out normal characters;
- select panic-safe sinks for emergency output;
- flush sinks that expose a flush callback;
- remain heap-free.

Non-responsibilities:

- UART MMIO;
- framebuffer drawing;
- SOL protocol handling;
- printf parsing;
- structured RAS schemas.

Initial capacity is eight simultaneous sinks. If more are ever needed in the minimal firmware, the design should be reconsidered rather than silently made unbounded.

### 8.3 `ConsoleStream`

The first frontend supports:

- `const char*`;
- `char`;
- `bool`;
- signed/unsigned native integer widths;
- pointers;
- explicit hexadecimal values.

Hexadecimal formatting is explicit:

```cpp
console::out << console::hex(value);
```

rather than hidden mutable `std::ios`-style format state.

Two global constant-initialized stream objects are provided:

```cpp
console::out
console::emergency
```

## 9. Memory sink

The initial memory sink is a fixed 36 KiB ring buffer. The size is intentionally in the same range as Hostboot's early kernel printk buffer, while ring semantics preserve the most recent failure context once the buffer fills.

State:

```text
buffer[36 KiB]
write_position
wrapped
```

The initial API exposes read-only buffer metadata so later code can publish it through a Jixia debug-pointer table, crash dump, Jingjie introspection, or a management-service handoff.

### 9.1 Concurrency limitation

The initial Console feature is validated on the existing single-hart baseline. The ring is not claimed to be multi-writer safe.

When per-hart state is introduced, Console concurrency must be revisited. Preferred directions are per-hart staging/rings or an explicit atomic reservation design, not a giant global lock on the panic path.

## 10. QEMU UART sink

The QEMU backend wraps the existing polling 16550-compatible UART primitive.

It:

- converts `\n` to `\r\n` for serial terminals;
- performs no formatting;
- is blocking/polling;
- is early-safe and panic-safe;
- has no heap, scheduler, or interrupt dependency.

The generic Console core contains no QEMU MMIO knowledge.

## 11. Dependency direction

The desired dependency direction is:

```text
microkernel/console
        ^
        |
platform/qemu_virt/console
        |
platform/qemu_virt/uart
```

Platform startup exposes only a narrow initialization boundary to the generic firmware core.

No Console object may require a runtime static constructor. Static/BSS state must be valid before general C++ constructor support exists.

## 12. Failure policy

Console output is diagnostic best-effort infrastructure.

Initial rules:

- invalid sinks are rejected;
- attaching the same descriptor twice is harmless;
- sink-table exhaustion returns failure;
- one backend cannot remove other backends from the route;
- the first synchronous sinks expose no elaborate recoverable I/O-error protocol;
- future asynchronous sinks should prefer drop accounting over indefinitely blocking the kernel;
- Console failure must never turn a recoverable architectural event into an unrecoverable one.

## 13. `printk` relationship

Jixia may later add a printf-style frontend for C code and imported low-level components.

It must feed the same output infrastructure:

```text
console::out << ...        future printk(...)
          \                  /
           \                /
            common formatting/output core
                       |
                 ConsoleRouter
```

Jixia must not grow two independent console stacks.

## 14. Initial source layout

```text
microkernel/console/
    sink.h
    console.h
    console.cpp

microkernel/core/
    console_test.cpp

platform/qemu_virt/
    console.cpp
    uart.c / uart.h

scripts/
    test-console.sh
```

The layout is deliberately small. Screen, SOL, simulator, and structured-log implementations become separate modules when they actually exist.

## 15. Acceptance criteria

The standalone Console foundation is accepted when:

1. firmware boot banners and normal test output use `console::out` rather than direct UART helpers;
2. output still reaches QEMU serial through the UART sink;
3. the same stream is retained in the static memory ring;
4. the dedicated Console test reports `CONSOLE_TEST: PASS`;
5. M00-02 TrapFrame regression still passes;
6. M00-03 recoverable-breakpoint regression still passes;
7. no timer implementation is present in or required by the Console branch;
8. no heap, `std::iostream`, exception, RTTI, or runtime static-constructor dependency is introduced;
9. raw UART remains available for pre-console/fatal bring-up paths;
10. the generic console core contains no QEMU-specific MMIO knowledge.

After these criteria pass, Console can be integrated as a foundation and `M00-04 Timer interrupt` can resume on top of it.

## 16. Future evolution

```text
Console foundation
    ConsoleStream
    Router
    MemorySink
    QEMU UartSink
        |
        +--> per-hart console ownership/concurrency
        +--> structured log/event ABI
        +--> screen/framebuffer sink
        +--> BMC/SOL sink
        +--> Jingjie sink
        +--> C-compatible printk frontend
        +--> richer filtering/severity policy
```

The stable architectural contract is the separation between frontend, formatting, routing, and sink transport. Individual implementations may change as Jixia gains per-hart state, services, RAS, and a complete runtime.
