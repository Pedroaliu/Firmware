# Jixia Console Architecture

**Status:** initial design baseline  
**Date:** 2026-08-07  
**Scope:** Jixia host firmware / Mozi microkernel console and low-level diagnostic output  
**Current implementation phase:** M00-04, single-hart QEMU `virt`

## 1. Why Jixia needs a console architecture

Jixia must not let higher-level firmware code depend directly on a single UART helper such as `uart_puts()`. A server firmware platform eventually needs several output destinations with very different failure and initialization properties:

- polling UART during the first instructions after reset;
- an in-memory boot/debug history for postmortem inspection;
- framebuffer or text-mode screen output when a display service exists;
- BMC Serial-over-LAN (SOL) or a future PLDM/vendor management transport;
- simulator/debug channels for Jingjie;
- later structured RAS/event records that may be rendered to a console but must also survive independently of a human-readable terminal.

The architectural problem is therefore not "replace `uart_puts()` with a prettier print function". The problem is to define a stable output model whose front-end syntax, formatting, routing, buffering, hardware transport, and failure behavior can evolve independently.

The design target is:

```text
call site
   |
   |  console::out << "hart=" << hex(hart_id) << '\n'
   v
ConsoleStream / future printf-style frontend
   |
   v
Formatter
   |
   v
Console Router
   |
   +----------------+----------------+----------------+
   |                |                |                |
Memory Sink      UART Sink      Screen Sink       SOL Sink
   |                                 |                |
postmortem                         display         BMC path
   |
   +---------------------> future RAS / simulator consumers
```

The stream syntax is deliberately only a frontend. It must never become the architecture itself.

## 2. Source-study conclusions

This design is informed by four families of systems.

### 2.1 OpenPOWER Hostboot

Hostboot separates formatting from the destination. Kernel `printk()` formats through `Util::vasprintf()` into an output interface, while the kernel `Console` stores characters in a fixed memory buffer. Higher-level console code can later route formatted text through a message queue to a console daemon. Important lessons for Jixia are:

1. early diagnostic history should exist independently of a physical serial port;
2. formatting should consume a generic character sink rather than know about UART hardware;
3. the earliest path must work without heap allocation or complex services;
4. a later console service can be substantially richer than the kernel's emergency path.

Hostboot's `Singleton<Console>` is useful as a reference, but Jixia does **not** adopt it directly. Jixia currently has no general C++ static-constructor bootstrap comparable to Hostboot's `Kernel::cppBootstrap()`, and future multi-hart/panic paths should not depend on hidden singleton construction or locks.

Relevant Hostboot references:

- `src/include/kernel/console.H`
- `src/kernel/console.C`
- `src/include/util/sprintf.H`
- `src/usr/console/console.C`

Reference repository: `open-power/hostboot`, branch `release-fw1120`.

### 2.2 EDK II / UEFI

EDK II's `DebugLib`, `PrintLib`, `SerialPortLib`, console-output implementations, and null/status-code variants demonstrate a second important property: the same logical debug frontend can be built against different output backends. The calling code does not need to know whether text ultimately goes to a serial port, a UEFI console protocol, a status-code mechanism, or nowhere at all.

Jixia adopts the same separation of frontend policy from transport, while avoiding the large UEFI library/protocol model in the minimal microkernel.

### 2.3 seL4

seL4 is the constraint on how far Jixia should go inside the microkernel. Kernel debug printing is a debugging facility, not a reason to move rich device stacks into the TCB. A real serial/display/BMC service can eventually live outside the minimal kernel while the kernel retains only the smallest emergency mechanism required for diagnosis and bootstrapping.

For Jixia this means:

- the microkernel may own a small routing/buffering mechanism;
- low-level polling output may remain available for panic/bring-up;
- complex screen, USB, network, or BMC transports should not become mandatory microkernel mechanisms merely because they can display text.

### 2.4 NXP MCUXpresso debug console

NXP's debug-console organization explicitly separates logging/formatting from low-level I/O and supports multiple physical backends such as UART, USB CDC, and SWO. It reinforces the value of keeping string formatting, buffering, and transport independent and of recording whether an output path is blocking or non-blocking.

## 3. Design principles

The Jixia console follows these rules.

### 3.1 Formatting and transport are independent

Code that converts an integer to hexadecimal must not know the UART base address. Code that writes a UART register must not parse `%d` or maintain stream formatting state.

### 3.2 Memory output is a first-class sink

UART is not the authoritative log store. An in-memory ring records recent console output so that a debugger, simulator, crash-dump path, or later debug-pointer ABI can inspect it even if an external terminal was absent.

### 3.3 Multiple sinks are routed through one small mechanism

Normal output may fan out to multiple attached sinks. A failed or unavailable future SOL/display sink must not disable UART or memory output.

### 3.4 Console and structured logging share infrastructure, not semantics

Console output is human-facing text. Future structured logging/RAS records will carry fields such as timestamp, hart, component, severity, event ID, address, syndrome, and recovery action. Structured events may be rendered to the console, but text parsing must never become the RAS data model.

```text
                 common sink / transport infrastructure
                              ^
                    +---------+---------+
                    |                   |
                Console text       Structured log/RAS
```

### 3.5 Early, runtime, and panic paths have different contracts

A backend declares capabilities. Early/panic-safe paths must not require heap allocation, a scheduler, interrupts, message queues, or a dynamically initialized C++ object graph.

### 3.6 No heap allocation in the console core

The router uses a fixed-capacity sink table. The memory sink uses a statically allocated buffer. Formatting uses caller-stack scratch space only.

### 3.7 No dependency on the C++ standard iostream runtime

Jixia is currently freestanding and links with `-nostdlib`/`-nostartfiles`. The project will provide a small `ConsoleStream` with familiar `operator<<` syntax, but it is not named or claimed to be `std::cout` compatible.

### 3.8 No mandatory virtual dispatch in the first implementation

A conventional abstract base class with virtual methods would be a reasonable hosted-C++ design, but Jixia currently does not execute a general static-constructor runtime. The initial `ConsoleSink` is therefore a small class containing a context pointer plus function pointers and capability bits. This gives object-style encapsulation and pluggable backends without hidden constructor/vtable dependencies.

When Jixia later owns an explicit C++ runtime bootstrap, the implementation can be reconsidered without changing the public routing model.

## 4. Three output phases

### 4.1 Phase 0: reset / early output

The earliest path must work before the console router is initialized.

Current QEMU behavior remains intentionally simple:

```text
reset/start.S
   -> raw polling 16550 UART helper when absolutely required
```

The existing low-level `uart_putc()` primitive is therefore not deleted. It becomes a hardware backend primitive rather than an application-facing console API.

Future physical platforms may use a different immutable early sink.

### 4.2 Phase 1: microkernel runtime console

Once `jixia_microkernel_main()` is entered, platform console initialization attaches at least:

```text
MemorySink
UartSink
```

Normal C++ code then uses the common frontend:

```cpp
console::out << "hart=" << console::hex(hart_id) << '\n';
```

The router fans each emitted character to all attached normal sinks.

### 4.3 Phase 2: service console

Later milestones may attach or project output into richer services:

- framebuffer/display service;
- BMC/SOL service;
- USB/network debug service;
- Jingjie simulator event channel.

These transports can be asynchronous and policy-rich. They should not enlarge the mandatory panic path.

## 5. Panic and RAS constraints

A panic/machine-check path cannot assume that normal kernel infrastructure is healthy. It may execute after corruption of:

- heap metadata;
- scheduler state;
- locks;
- a service queue;
- interrupt state;
- a normal console backend.

The console therefore distinguishes normal routing from emergency routing. Emergency output is sent only to sinks marked `panic_safe`.

A `panic_safe` sink must not require:

- dynamic allocation;
- locks whose owner may have failed;
- task scheduling;
- asynchronous completion;
- an interrupt-driven driver stack.

For the current QEMU implementation, the memory sink and polling UART sink are panic-safe. A future BMC SOL backend will normally **not** be panic-safe unless a separate primitive transport is deliberately designed for that purpose.

The raw UART helper also remains available below the router for failures that occur before router initialization itself.

## 6. Core object model

### 6.1 `ConsoleSink`

`ConsoleSink` is a lightweight descriptor for a concrete output target.

Conceptually it contains:

```text
context pointer
put-character callback
optional flush callback
capability flags
```

The context allows one sink class to describe stateful devices without heap allocation. A framebuffer sink could point at cursor/framebuffer state; a SOL sink could point at transport state; a memory sink points at ring-buffer state.

Initial capability flags are:

```text
EARLY_SAFE     usable without higher runtime services
PANIC_SAFE     safe on the emergency path
BLOCKING       put/flush may wait synchronously
RUNTIME_ONLY   requires normal runtime infrastructure
```

Capabilities are descriptive and allow later policy. The first router only uses `PANIC_SAFE` to select the emergency path.

### 6.2 `ConsoleRouter`

`ConsoleRouter` owns no devices. It holds references to a small fixed number of `ConsoleSink` descriptors.

Responsibilities:

- attach sinks;
- fan out normal characters;
- fan out emergency characters only to panic-safe sinks;
- flush sinks that expose a flush callback;
- avoid heap allocation.

Non-responsibilities:

- printf parsing;
- UART register access;
- screen drawing;
- SOL protocol state;
- RAS record schemas.

The initial sink capacity is deliberately small (8). If a server firmware needs more than eight simultaneous kernel console transports, the design should be reviewed rather than silently made unbounded.

### 6.3 `ConsoleStream`

`ConsoleStream` is the human-friendly frontend.

Initial operators cover:

- `const char*`;
- `char`;
- `bool`;
- common signed/unsigned integer widths through native C++ overloads;
- pointers;
- an explicit hexadecimal wrapper.

The API intentionally uses explicit `hex(value)` instead of maintaining hidden mutable stream formatting state. This keeps one global stream object reentrant enough for the current single-hart firmware and avoids `std::ios`-style state.

Example:

```cpp
console::out
    << "hart        : " << console::hex(hart_id) << '\n'
    << "timer count : " << timer::interrupt_count() << '\n';
```

A separate emergency stream uses the same formatting code but selects panic-safe sinks.

## 7. Memory sink

The first implementation allocates a fixed 36 KiB memory ring. The size intentionally follows the same order as Hostboot's early kernel printk buffer while using ring semantics so recent failure context is retained when the buffer fills.

State is:

```text
buffer[36 KiB]
write position
wrapped flag
```

The buffer is not dynamically allocated and is never freed.

The initial implementation exposes read-only metadata accessors so that later code can publish the buffer through:

- a Jixia debug-pointer table;
- Jingjie simulator introspection;
- crash/RAS dumps;
- a management-service handoff.

### Concurrency limitation

M00-04 is single-hart. The initial ring is therefore deliberately lock-free **because there is only one active hart**, not because it is magically multi-writer safe. M00-05 must revisit memory-console ownership together with per-hart state/stacks. Preferred future directions are per-hart staging buffers or a clearly defined atomic reservation scheme, not a giant global console lock on the panic path.

## 8. QEMU UART sink

The QEMU `virt` backend wraps the existing 16550-compatible polling UART primitive.

The sink:

- translates `\n` to `\r\n` for a conventional serial terminal;
- performs no formatting;
- is blocking/polling;
- is marked early-safe and panic-safe;
- contains no heap or scheduler dependency.

The low-level `platform/qemu_virt/uart.c` remains the device primitive. Higher-level firmware code should stop including `uart.h` except where an intentionally raw bring-up/emergency dependency is required.

## 9. Initialization and dependency direction

The generic microkernel console must not include a QEMU header.

Dependency direction:

```text
microkernel/console
        ^
        |
platform/qemu_virt/console
        |
platform/qemu_virt/uart
```

Platform setup performs:

```text
console core initialize
attach memory sink
attach QEMU UART sink
```

This keeps future real platforms free to choose different default sinks without editing the generic router.

No static object requires a runtime constructor. Sink descriptors and stream objects must be constant/static initialized, and router/ring state must be valid from zero-initialized BSS.

## 10. Error and failure policy

Console output is diagnostic best-effort infrastructure.

Initial policy:

- attaching more than the fixed sink capacity returns failure;
- a null character callback is rejected;
- writing to one sink does not change routing to other sinks;
- sink callbacks return no recoverable I/O error in the first implementation because the current polling UART and memory sink have no useful recovery policy;
- future asynchronous sinks may maintain their own dropped-record counters rather than block the kernel indefinitely;
- console failure must never turn a recoverable trap into an unrecoverable one.

## 11. Console versus `printk`

Jixia may later expose a small printf-style helper because format strings are convenient for imported C code and some low-level diagnostics. That frontend should feed the same router/formatter infrastructure.

The intended relationship is:

```text
console::out << ...      C++ stream-style frontend
printk("...", ...)       future C/format-string frontend
        \                 /
         \               /
          common formatter
                |
          ConsoleRouter
```

`printk` and `console::out` must not become two independent output stacks.

## 12. Console versus structured log/RAS

A future structured event might conceptually contain:

```text
timestamp
hart
component
severity
event_id
field_count
fields[]
```

It can be sent to a binary ring and optionally rendered through the console. This design prevents the common failure mode where production RAS evidence exists only as unstructured strings.

The console implementation in M00-04 does **not** prematurely define the RAS ABI. It only preserves the routing/sink separation required to share transports later.

## 13. File layout

Initial implementation:

```text
microkernel/console/
    sink.h              lightweight sink descriptor/capabilities
    console.h           public stream + router-facing API
    console.cpp         router, formatter, memory ring, stream implementation

platform/qemu_virt/
    console.h           platform console initialization
    console.cpp         UART sink descriptor and default attachment
    uart.c/.h           raw hardware primitive
```

The number of files is intentionally small at this stage. When structured logging, screen output, or management transports become real components, they should grow into separate modules rather than turning `console.cpp` into a monolith.

## 14. First implementation acceptance criteria

The initial code is acceptable when:

1. existing boot/trap/timer tests still emit their exact PASS markers;
2. normal firmware/test code can use `console::out` instead of direct `uart_puts()`;
3. output still reaches QEMU serial through the UART sink;
4. the same characters are retained in the static memory ring;
5. no heap, C++ standard iostream, exception, RTTI, or runtime static-constructor dependency is introduced;
6. raw UART remains available for pre-console fatal/bring-up paths;
7. the generic console core has no QEMU-specific MMIO knowledge.

The dedicated M00-04 timer acceptance remains separate: this console refactor must not be allowed to hide or redefine timer correctness.

## 15. Future evolution

Likely evolution order:

```text
M00-04
    ConsoleStream + Router + MemorySink + QEMU UartSink

M00-05
    define multi-hart console/ring ownership and panic concurrency

later platform work
    framebuffer/screen sink
    BMC/SOL sink
    Jingjie simulator sink

RAS phase
    structured binary/event ring
    console renderer for events
    crash handoff / debug pointers

service phase
    asynchronous console daemon/service
    complex transports outside the minimum microkernel
```

## 16. Invariants to preserve

The following are architectural invariants, not implementation preferences:

- **No caller knows where text is physically displayed.**
- **No device sink parses application formatting.**
- **No panic-safe sink depends on scheduler/heap/normal locks.**
- **No complex display/SOL driver becomes mandatory microkernel code merely for convenience.**
- **Raw early output remains available below the normal console stack.**
- **Structured RAS data will not be reduced to printable text.**
- **Changing the stream syntax must not require rewriting transports.**
- **Changing a transport must not require rewriting call sites.**

This is the contract that allows Jixia to begin with one QEMU UART today and grow into a server firmware console/debug/RAS system without another project-wide print rewrite.