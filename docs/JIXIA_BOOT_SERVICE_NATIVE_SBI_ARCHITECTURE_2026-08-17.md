# Jixia Boot Service and Native SBI Runtime Architecture

## Status

**Architecture checkpoint:** ACCEPTED

**Date:** 2026-08-17

This document records the architecture decisions that close the Hostboot service-startup research gate and define the implementation direction after M00-07.

The primary reference remains IBM Hostboot for firmware lifecycle and boot orchestration. Jixia intentionally strengthens protection boundaries using RISC-V privilege levels, explicit address-space ownership, message passing, and later capability policy.

## 1. Lifecycle split

Jixia has two distinct host-firmware lifetimes:

```text
RESET
  -> Boot Engine / minimum prerequisite logic
  -> Jixia Boot Realm
  -> exit_boot_services()
  -> Jixia Runtime Realm
  -> S/HS-mode system software
```

The boot realm may be software-rich because it performs heavy platform initialization. The runtime realm must be smaller and remain the trusted machine-level authority for SBI, RAS, security, and confidential-computing policy.

## 2. Boot privilege model

The production boot-service model is:

```text
M-mode
    Jixia microkernel
    bare / physical address domain
    owns traps, scheduler, VMM, PageManager, IPC, capability enforcement

S-mode
    intentionally unused by Jixia boot services
    reserved for the later OS/hypervisor world

U-mode
    disposable firmware boot services
    translated through Sv39
    examples: InitService, PNOR/VFS providers, MemoryInit, FabricInit, PCIe/CXL init
```

M00-06 and M00-07 S-mode probes remain valid acceptance mechanisms, but they do not define the production service privilege model.

### 2.1 M-mode stays bare

Normal Jixia M-mode kernel execution uses physical addresses. The kernel is not required to map itself into every firmware-service address space.

A U-mode task owns or references an Sv39 root through its task/VSpace context. On dispatch:

```text
kernel prepares task context
    -> satp = task VSpace root
    -> mepc = U-mode entry VA
    -> mstatus.MPP = U
    -> mret
```

When a U-mode trap returns to M-mode, normal M-mode execution again uses the physical/bare domain.

### 2.2 Address-space ownership must be explicit

The first implementation may temporarily use one simple service VSpace, but APIs must not bake in a global `satp`. A task/service context must carry an address-space identity from the beginning so later per-service isolation does not require redesigning the task ABI.

Kernel code must never treat a U-mode virtual pointer as a physical pointer. User-memory access goes through explicit `copy_from_user` / `copy_to_user` style abstractions; their internal implementation may later use software page-table walking or controlled RISC-V mechanisms such as MPRV.

## 3. Boot tasks are ephemeral firmware processes

U-mode firmware services exist to make the platform operational. They are not permanent runtime dependencies.

Architecture invariant:

> No system-runtime operation may require a boot-only U-mode service after `exit_boot_services()`.

Examples of boot-only objects:

```text
InitService task
memory/fabric/PCIe initialization tasks
boot VSpaces
boot task stacks
boot IPC endpoints
pageable boot components
boot-scoped capabilities
```

They may use pre-DDR contained memory and later mainstore while the boot realm is active, but their lifecycle ends before control is handed to the OS/hypervisor.

## 4. Component loading model

Jixia keeps ELF as a development and build-time interchange format, but the pre-DDR runtime does not need a Linux-style general dynamic ELF loader.

Preferred flow:

```text
GCC/LLVM ELF component
    -> Jixia image builder
       validate layout and relocations
       extract entry points
       assign virtual ranges and permissions
       generate component identity and backing metadata
       generate dependencies / later capability requests
       generate hashes/signature metadata
    -> PNOR image + component catalog
```

Runtime lookup is then approximately:

```text
ComponentId / name
    -> resident component catalog
    -> entry + VmRegion + backing object + permissions
    -> create/start task
```

This follows the Hostboot principle of moving deterministic work to build time while leaving room for a richer NXP-style manifest and Jixia capability policy.

## 5. Resident bootstrap closure

Anything needed to create the pageable firmware world must not depend recursively on that pageable world.

The resident boot-critical closure includes at least:

```text
trap entry/exit
TaskContext minimum
scheduler minimum
PageManager
Sv39/VMM mechanisms
root component registry
minimum PNOR access
minimum component/provider bootstrap
InitService bootstrap path
```

Later implementations may move policy out of M-mode, but the bootstrap dependency graph must remain acyclic.

## 6. Page-fault and Resource Provider model

M00-07 proved direct kernel-side PNOR page materialization. The production service architecture evolves this to a Hostboot-style blocking Resource Provider path.

Target flow:

```text
U-mode service faults
    -> M-mode trap/VMM
    -> locate VmRegion/backing object
    -> allocate physical frame
    -> send provider request
    -> mark faulting task BLOCKED
    -> schedule resident/pinned U-mode provider
    -> provider fills frame from PNOR/backing store
    -> provider responds
    -> kernel installs PTE
    -> wake original task
    -> retry exact faulting instruction
```

The pager/provider critical path itself must remain resident or pinned so that handling a page fault does not recursively fault on unavailable provider code.

Read-side firmware backing and persistent mutation remain separate. Normal CPU stores never implicitly write PNOR.

## 7. InitService owns boot orchestration

The host boot flow follows the Hostboot responsibility model:

```text
resident Base/kernel
    -> initial boot task
    -> root component registry
    -> Base InitService
    -> PNOR/provider services
    -> extended/pageable component world
    -> InitService/ISTEP graph
    -> HWP/platform initialization
```

InitService is an orchestrator, not a hardware driver. Hardware-specific initialization logic belongs in components/HWPs invoked by the boot graph.

DDR discovery, configuration, training, diagnostics, grouping/interleave, and decode setup remain host-firmware work. They return to the memory roadmap only after this service substrate exists.

## 8. Boot-to-runtime transition

`exit_boot_services()` is a real lifecycle boundary, not a boolean marker.

Its eventual semantics include:

```text
stop new boot task creation
finish/cancel the boot graph
quiesce boot services
complete or drain IPC
revoke boot-scoped capabilities
release boot task stacks/pages
tear down boot service VSpaces
invalidate stale service translations
remove temporary physical permissions
scrub sensitive boot-only data
publish final platform handoff state
transfer device ownership
enter persistent Jixia M-mode runtime
```

Exact ordering will be frozen when the boot-service and runtime subsystems both exist.

## 9. Jixia Runtime is native, SBI-compatible M-mode firmware

The persistent runtime is not planned as an OpenSBI wrapper.

Decision:

> Jixia implements the RISC-V SBI standard interfaces with Jixia-owned M-mode mechanisms. OpenSBI is a primary reference implementation and differential/compatibility oracle, not the owner of Jixia machine state.

Reason: OpenSBI `lib/sbi` is not merely a thin ABI parser. It includes trap handling, hart/HSM state, domains, PMP/protection, IPI, timer, TLB, interrupt-controller integration, PMU, heap/scratch, and other M-mode executive mechanisms. Linking the complete OpenSBI runtime underneath Jixia would create overlapping ownership of the same machine-level state.

Selective reuse of leaf definitions or implementation ideas may be considered deliberately, but Jixia must retain authoritative ownership of:

```text
M-mode traps and delegation
hart lifecycle
machine interrupt routing
PMP / physical protection policy
runtime RAS state
security and confidential-computing state
Management Complex coordination
SBI extension policy
```

## 10. Runtime responsibility

After boot-service teardown:

```text
M-mode Jixia Runtime
    -> standard SBI services
    -> machine-level RAS containment/notification
    -> security monitor and trust continuity
    -> confidential-computing hooks/state
    -> platform runtime and Management Complex interface

S/HS-mode
    -> Linux / hypervisor
    -> normal OS memory management
    -> native or virtualized system software
```

Machine-level and OS-level recovery are intentionally distinct:

```text
Jixia/Management Complex
    decide whether the machine can continue safely
    preserve platform FFDC and containment state
    expose architectural/runtime events

OS/hypervisor
    decide how to recover processes, pages, VMs, and workloads
```

## 11. RISC-V runtime privilege continuation

Native OS profile:

```text
M     Jixia Runtime
S     Linux / native OS kernel
U     OS applications
```

Virtualized profile with H extension:

```text
M     Jixia Runtime
HS    host hypervisor / host kernel
VS    guest kernel
VU    guest application
```

The firmware boot U-mode world is destroyed before the OS U-mode world becomes relevant.

## 12. Management Complex boundary

The Management Complex remains always-on and host-independent:

```text
Management Complex
    RAS collection/aggregation
    telemetry
    watchdog
    power/thermal supervision
    BMC/OOB communication
    recovery/degrade coordination
    predictive health/rule processing

Jixia M Runtime
    architectural machine authority on host CPUs
    SBI/event delivery
    CPU-local machine containment/security actions

S/HS OS or hypervisor
    software/workload recovery and policy
```

The Management Complex is not a second Hostboot and does not absorb heavy DDR/HWP boot logic solely to gain parallelism.

## 13. Implementation roadmap after M00-07

The Hostboot service-startup research gate is sufficiently closed to start implementation.

### M00-08 — Boot Service Execution Foundation

Primary goal:

> Turn privilege-transition probes into a real first-class boot-task execution substrate.

Planned increments:

```text
08.01  TaskContext + M-mode bare kernel -> U-mode task -> ECALL -> M
08.02  minimal task states and scheduler / idle path
08.03  minimum task syscalls: yield, exit, create/wait as required
08.04  message queue + blocking/wakeup IPC foundation
08.05  resident Root Component Registry / prebuilt component catalog
08.06  init_main bootstrap -> registry -> InitService
08.07  minimal Base InitService task list and lifecycle acceptance
```

M00-08 does not implement a general dynamic ELF loader and does not yet move DDR initialization into the boot graph.

### Next — provider-backed pageable component foundation

After M00-08:

```text
component catalog / VmRegion metadata
extended component provider
kernel page fault -> provider IPC
blocked faulting task
PNOR -> physical frame
response -> install PTE -> resume
```

This replaces the M00-07 direct trap-to-FlashProvider path with the real service/provider architecture.

### Later — real memory ISTEP continuation

Only after InitService and provider-backed paging exist:

```text
InitService memory ISTEPs
    -> host-driven DDR discovery/config/training/diagnostics
    -> address map / decode viable
    -> exit contained
    -> VMM/PageManager mainstore extension
    -> continue the same service world
    -> natural post-DDR pageable component fault
    -> DDR-backed frame allocation
```

## 14. Architecture invariants to preserve

1. Hostboot remains the primary firmware lifecycle reference.
2. M-mode boot kernel executes in a bare/physical domain.
3. Boot firmware services execute as U-mode tasks with explicit VSpace identity.
4. S-mode is reserved for post-boot OS/hypervisor use, not Jixia boot services.
5. Boot U-mode services are disposable and cannot become runtime dependencies.
6. The runtime remains Jixia-owned M-mode firmware implementing the SBI standard ABI.
7. OpenSBI is a reference/oracle, not the authoritative Jixia runtime core.
8. Firmware components are preprocessed at build time; pre-DDR runtime avoids unnecessary general dynamic-linker complexity.
9. Page faults repair backing and resume the original task; they do not drive boot-phase state machines such as DDR initialization.
10. InitService owns boot orchestration; kernel/VMM owns mechanisms; HWPs/components own hardware operations.
11. Prepare resources completely before publishing them as available.
12. Heavy host initialization remains on host cores; Management Complex remains the always-on/OOB management plane.
13. Runtime RAS/security authority survives boot-service teardown in Jixia M Runtime and/or Management Complex, not in disposable U-mode services.

## 15. Immediate coding entry point

Start M00-08.01 by reviewing the accepted M00-06 privilege-transition code, M00-07 Sv39 primitives, and current TrapFrame. Promote the minimum reusable pieces into a real `TaskContext` and U-mode dispatch ABI rather than extending synthetic S-mode probes.
