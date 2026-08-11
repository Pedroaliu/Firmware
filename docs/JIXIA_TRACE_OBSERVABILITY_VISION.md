# Jixia Trace / Waveform-style Observability Vision

**Status:** research idea / future architecture record  
**Date:** 2026-08-07  
**Scope:** future debug trace, structured events, and Jingjie integration

## 1. Motivation

Traditional firmware logs are linear text:

```text
[INFO] hart0 init memory
[INFO] hart1 init pcie
[INFO] hart0 memory done
```

This loses important execution relationships:

- whether operations were parallel;
- which hart/thread/device was executing;
- interrupt timing;
- blocking dependencies;
- request latency across components.

Jixia should not rely only on printf-style debugging. The long-term goal is a waveform/timeline-style debugging system similar in spirit to hardware waveform viewers, but for firmware and system execution.

## 2. Three observability layers

Do not merge all information into printk.

```text
                 Jixia Observability

       printk             Trace              RAS
          |                 |                 |
     human text       structured events   fault records
```

### printk

Purpose:

- early boot visibility;
- human-readable diagnostics;
- panic/failure fallback.

Properties:

- simple;
- low dependency;
- not a high-volume tracing mechanism.

### Trace

Purpose:

- execution timeline;
- concurrency analysis;
- performance analysis;
- simulator/debugger integration.

### RAS Event

Purpose:

- machine-readable hardware/software failure information;
- recovery decision;
- diagnosis.

A RAS event may be rendered to console, but console text is not the RAS ABI.

## 3. Trace record concept

Future trace records should be structured, not strings.

Possible fields:

```text
timestamp
sequence
hart
thread
component
event_id
type
correlation_id
arguments
```

## 4. Time dimension

The trace system should provide a unified time concept.

Possible sources:

```text
real hardware:
    mtime / platform monotonic clock

simulator:
    Jingjie Tick
```

The upper trace layer should not depend on the clock source.

Goal:

```text
Jixia firmware trace
          <---->
Jingjie simulator timeline
```

## 5. Space dimension

Events should have ownership/context:

```text
System
 |
 +-- CPU
 |    +-- Hart0
 |    |    +-- ThreadA
 |    |    +-- IRQ
 |    +-- Hart1
 |
 +-- PCIe
 |
 +-- Memory
 |
 +-- RAS
```

The viewer should support collapsing and expanding levels:

```text
system
  -> subsystem
      -> component
          -> thread/event
```

## 6. Event types

Future trace ABI should distinguish:

### Instant event

Examples:

```text
IRQ_ENTER
LOCK_ACQUIRE
PAGE_FAULT
IPC_SEND
```

### Duration span

Examples:

```text
DDR_TRAINING_BEGIN
...
DDR_TRAINING_END

PCI_ENUM_BEGIN
...
PCI_ENUM_END
```

### Counter/state track

Examples:

```text
PCIe queue depth
memory bandwidth
CPU frequency
thread state
IRQ level
```

## 7. Concurrency and causality

Timestamp alone is insufficient for parallel debugging.

Use correlation IDs for relationships:

Example IPC:

```text
Hart0:
    IPC_SEND id=42

Hart1:
    IPC_RECV id=42
```

Example interrupt:

```text
Device:
    IRQ_RAISE id=88

Hart3:
    IRQ_ENTRY id=88
    IRQ_EXIT id=88
```

This allows visualization of happens-before relationships.

## 8. Waveform-style visualization goal

Future viewer should resemble a hardware waveform/debug timeline:

```text
time ------------------------------------------------>

Hart0
 |---- DDR training ----------------|

Hart1
      |---- PCIe enumeration -------|

IRQ
             ^ timer interrupt

IPC
 Hart0 ----------------------------> Hart1
```

The user should be able to zoom:

```text
system
  |
  +-- hart
       |
       +-- thread
            |
            +-- instruction/event/cycle
```

## 9. Relationship with existing Console design

Current F00-01 Kernel Print remains intentionally simple:

```text
printk
  |
KernelLogBuffer
```

Future Trace is a separate subsystem.

Do not make printk become a tracing framework.

## 10. Future roadmap

Potential milestone:

```text
M00-08 Structured event and trace ABI
```

Future work:

- TraceRecord ABI;
- per-hart trace buffers;
- trace clock abstraction;
- event dictionary;
- correlation tracking;
- Jingjie timeline import/export;
- waveform/timeline viewer.

## 11. Design principle

Jixia debugging should evolve from:

```text
searching logs
```

into:

```text
replaying system behavior in time and space
```

The final goal is a firmware-native logical analyzer for complex parallel systems.
