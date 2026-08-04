# ArchFW POWER PRDF FIR / Attention Routing Notes v0.1

## 1. Scope

This note records the currently confirmed notification path from a POWER hardware error to Hostboot PRDF, and separates three concepts that are easy to mix together:

1. Power ISA Machine Check and HMI exception paths.
2. The platform Attention/IPOLL/PSIHB notification path.
3. PRDF rule analysis and resolution after notification.

The focus here is the third path: how a FIR bit is classified, propagated, converted into a PSIHB Local Error interrupt, and finally delivered to PRDF.

---

## 2. Core conclusion

PRDF is not an interrupt handler and is not called directly by a FIR, Machine Check, or HMI.

The runtime path is:

```text
Hardware detector
    -> Local FIR bit
    -> FIR MASK + ACT0/ACT1/ACT2 classification
    -> Chiplet attention aggregation
    -> Processor global attention aggregation
    -> IPOLL status
    -> PSIHB Local Error interrupt
    -> skiboot PRD event
    -> Linux OPAL-PRD driver
    -> opal-prd userspace daemon
    -> HBRT Attention Service
    -> PRDF::main()
```

During Hostboot IPL the Linux/skiboot/opal-prd portion is replaced by Hostboot's own interrupt service, attention interrupt task, and PRD worker task.

The most precise statement is:

> PSIHB Local Error notifies the Attention Service. The Attention Service enumerates the actual processor and memory attentions, builds an AttentionList, and then synchronously calls PRDF.

---

## 3. Machine Check, HMI, and Attention are different paths

### 3.1 Machine Check

Power Machine Check is an execution-context exception. It answers whether the current CPU, instruction, process, partition, or guest can continue.

It may be synchronous, such as a load consuming an uncorrectable memory error, or asynchronous, such as a retired store later failing in the fabric or memory subsystem.

### 3.2 HMI

HMI, Hypervisor Maintenance Interrupt, reports processor maintenance and hypervisor-critical events such as processor recovery completion or failure, malfunction alert, time facility failure, and some hypervisor resource failures.

### 3.3 Attention

Attention is not a Power ISA exception vector. It is a platform RAS notification class produced by the FIR hierarchy and summarized through IPOLL.

It answers a different question:

> Which hardware object is reporting an error, what is the root cause, what evidence must be captured, and what callout, threshold, GARD, deconfiguration, reset, or mask action should follow?

The same physical fault may produce more than one path. For example, a memory UE can synchronously cause a Machine Check for the consuming CPU while also setting memory FIRs that later drive Attention/PRDF analysis.

---

## 4. A FIR is a state register plus an action policy

A typical FIR block contains at least:

```text
FIR      error occurrence state
MASK     whether a bit is allowed to report outward
ACT0     action encoding bit 0
ACT1     action encoding bit 1
ACT2     action encoding bit 2 on newer blocks
```

When error detector `k` fires:

```text
FIR[k] = 1
```

hardware also evaluates:

```text
MASK[k], ACT0[k], ACT1[k], ACT2[k]
```

and classifies the error before software sees it.

For the P10/Explorer-style generic FIR helper used by Hostboot memory code, the important encodings are:

| ACT0 | ACT1 | ACT2 | MASK | Hardware classification |
|---:|---:|---:|---:|---|
| 0 | 0 | 0 | 0 | Checkstop |
| 0 | 1 | 0 | 0 | Recoverable error |
| 1 | 0 | 0 | 0 | Attention |
| 1 | 1 | 0 | 0 | Local checkstop |
| 0 | 0 | 1 | 0 | Host attention |
| x | x | x | 1 | Masked |

Older blocks may not have ACT2, and individual IPs may have additional controls, but the general rule is stable:

> FIR records what happened; action and mask registers define how the hardware reports and contains it.

---

## 5. FIR hardware action is not PRDF resolution

There are two different kinds of "action".

### 5.1 FIR action

FIR action is programmed before the error occurs. It determines whether the bit is treated as recoverable, host attention, local/unit checkstop, system checkstop, or masked.

### 5.2 PRDF resolution

PRDF resolution runs after notification and diagnosis. It determines:

- error signature;
- callout target and priority;
- threshold state;
- GARD or deconfiguration policy;
- service-call policy;
- FFDC and dump collection;
- whether FIR bits should be cleared or masked;
- whether recovery or repair procedures should be invoked.

Therefore the path is not:

```text
FIR bit -> PRDF decides recoverable versus checkstop
```

It is:

```text
Initialization HWP programs FIR action
    -> hardware classifies and propagates the error
    -> PRDF diagnoses the reported class and performs service resolution
```

---

## 6. Hierarchical aggregation from local FIR to PSIHB

A local FIR normally does not directly drive PSIHB Local Error. The error passes through a hierarchy.

```text
Local/unit FIR
    -> chiplet CS/RE/UCS/HA aggregation FIR
    -> processor GLOBAL_CS/RE/UCS/HA aggregation FIR
    -> IPOLL class bit
    -> PSIHB Local Error LSI
```

P10 PRDF rule files expose the software-visible hierarchy with registers such as:

```text
GLOBAL_CS_FIR
GLOBAL_RE_FIR
GLOBAL_UCS_FIR
GLOBAL_HA_FIR

TP_CHIPLET_CS_FIR
TP_CHIPLET_RE_FIR
TP_CHIPLET_UCS_FIR
TP_CHIPLET_HA_FIR

TP_LOCAL_FIR + MASK + ACT0 + ACT1 + ACT2
```

Equivalent structures exist for other chiplets and units.

PRDF follows this tree downward:

```text
Global attention FIR
    -> responsible chiplet attention FIR
    -> local FIR
    -> exact active bit
    -> resolution
```

The complete physical wiring and every propagation gate are defined in IBM chip FIR/RAS workbooks. The open source confirms the register hierarchy and software behavior, but does not expose every internal hardware connection.

---

## 7. Who defines the FIR behavior

### 7.1 Silicon/RAS design

The hardware specification defines:

- which detector maps to each FIR bit;
- which actions are legal;
- containment and escalation behavior;
- local-to-chiplet-to-global propagation;
- interaction with retry, poison, freeze, channel fail, core recovery, Machine Check, and HMI.

Some errors may be fundamentally unrecoverable and cannot safely be turned into a recoverable notification by firmware.

### 7.2 Initialization HWP and platform policy

Hostboot HWPs program ACT0/ACT1/ACT2/MASK during phases such as:

- chiplet initialization;
- memory initialization;
- link training completion;
- post-memory-diagnostics transition;
- runtime FIR unmasking.

The selected action may depend on:

- platform mode;
- policy attributes;
- populated topology;
- silicon revision;
- known workarounds;
- whether the channel or subchannel exists;
- manufacturing or lab environment.

A critical programming rule is:

```text
write action registers first
    -> unmask the FIR bit last
```

This prevents an already-set bit from being exposed with stale or unintended action encoding.

### 7.3 PRDF rule files

PRDF rule files do not normally program the original severity classification. They decode the current state from FIR, MASK, and action registers and map it into the appropriate attention group.

For example, the P10 processor rule reconstructs the active classes conceptually as:

```text
CHECK_STOP = FIR & ~MASK & ~ACT0 & ~ACT1 & ~ACT2
HOST_ATTN  = FIR & ~MASK & ~ACT0 & ~ACT1 &  ACT2
RECOVERABLE= FIR & ~MASK & ~ACT0 &  ACT1 & ~ACT2
UNIT_CS    = FIR & ~MASK &  ACT0 &  ACT1 & ~ACT2
```

This allows PRDF to verify which local bits belong to the attention type reported at the top of the hierarchy.

---

## 8. Runtime notification path in detail

### 8.1 PSIHB interrupt source

On P9/P10, the relevant interrupt source is `P9_PSI_IRQ_LOCAL_ERR`. Skiboot dispatches it to:

```text
prd_psi_interrupt(chip_id)
```

This is a normal platform LSI routed through PSIHB/XIVE to OPAL, not the Power ISA Machine Check or HMI vector.

### 8.2 Skiboot PRD front end

`prd_psi_interrupt()`:

1. reads the processor IPOLL status;
2. keeps only PRD-relevant classes;
3. masks the pending IPOLL classes;
4. records them in per-chip software state;
5. queues an `OPAL_PRD_MSG_TYPE_ATTN` message.

The message contains:

```text
processor chip ID
IPOLL status
current IPOLL mask
```

### 8.3 Linux OPAL-PRD driver

The Linux PowerNV OPAL-PRD driver registers for `OPAL_MSG_PRD` and `OPAL_MSG_PRD2`.

When a message arrives, it:

1. copies the message into a kernel queue;
2. wakes the `/dev/opal-prd` wait queue;
3. causes the userspace daemon's `poll()`/`read()` to return.

### 8.4 opal-prd and HBRT

The daemon invokes HBRT's runtime interface:

```text
handle_attns(proc, ipoll_status, ipoll_mask)
```

HBRT then:

1. converts the XSCOM chip ID to a Hostboot Target;
2. asks the Attention Service to resolve active processor and OCMB attentions;
3. builds a sorted AttentionList;
4. calls `PRDF::main(primary_attention_type, attention_list)`.

PRDF itself does not own the hardware interrupt handler.

---

## 9. Hostboot IPL notification path

During IPL, Hostboot registers a message queue for the local error interrupt source:

```text
INTR::registerMsgQ(..., INTR::ISN_LCL_ERR)
```

Hostboot splits handling into two tasks:

```text
Interrupt task
    - wait on interrupt message queue
    - do minimum pre-ACK masking
    - send EOI
    - set pending flag
    - signal condition variable

PRD task
    - wait on condition variable
    - enumerate pending attentions
    - call PRDF
```

The split ensures that full FIR traversal and rule analysis are never performed in the immediate interrupt path.

---

## 10. ACK and unmask transaction

The notification protocol is deliberately transactional:

```text
FIR/IPOLL reports error
    -> mask pending IPOLL class
    -> preserve evidence
    -> run Attention Service and PRDF
    -> clear/mask diagnosed FIR bits as appropriate
    -> send ATTN_ACK
    -> unmask acknowledged IPOLL class
```

Runtime `opal-prd` sends `OPAL_PRD_MSG_TYPE_ATTN_ACK` only after HBRT `handle_attns()` returns successfully. Skiboot then unmasks only the acknowledged IPOLL bits.

This avoids:

- interrupt storms;
- clearing evidence before diagnosis;
- unmasking a still-active condition;
- losing a new attention that arrived while the previous event was being processed.

---

## 11. Example: MCC subchannel timeout

Assume an MCC timeout bit is programmed as recoverable:

```text
MASK = 0
ACT0 = 0
ACT1 = 1
ACT2 = 0
```

The runtime sequence is:

```text
1. MCC timeout detector fires.
2. MCC local FIR bit is set.
3. FIR action logic classifies it as Recoverable.
4. MCC/chiplet recoverable aggregation bit is set.
5. Processor GLOBAL_RE_FIR records the reporting chiplet.
6. IPOLL_RECOVERABLE becomes active.
7. PSIHB Local Error LSI is raised.
8. Skiboot records and masks the IPOLL bit.
9. OPAL message wakes opal-prd.
10. HBRT Attention Service enumerates the reporting targets.
11. PRDF traverses GLOBAL_RE_FIR -> chiplet RE FIR -> MCC local FIR.
12. PRDF selects the matching bit resolution.
13. FFDC, callout, threshold, and service policy are updated.
14. The processed FIR bit is cleared or masked according to policy.
15. ATTN_ACK is returned.
16. Skiboot unmasks the acknowledged IPOLL class.
```

If the same detector were configured as local checkstop or checkstop, the hardware containment and aggregation class would change before PRDF runs.

---

## 12. ArchFW design lessons

ArchFW should preserve the following ideas:

### 12.1 Separate hardware detection, classification, notification, and diagnosis

```text
Detector
    -> hardware action policy
    -> aggregation and notification
    -> software diagnosis
    -> service resolution
```

### 12.2 Treat the interrupt as a doorbell, not as complete evidence

The interrupt summary identifies the reporting domain. The RAS service must collect an evidence snapshot from the owning IP before clearing state.

### 12.3 Model hierarchical propagation explicitly

RVSoC-Sim and ArchFW should model:

```text
LocalFIR
ChipletAttention
GlobalAttention
IPOLL
PSI/doorbell
```

instead of a direct `fault -> PRDF` callback.

### 12.4 Keep hardware action policy separate from diagnostic policy

Hardware action policy controls containment and notification. Diagnostic policy controls callout, thresholds, GARD, recovery workflow, and long-term state.

### 12.5 Require evidence-before-ACK semantics

The RAS transaction should be:

```text
mask -> snapshot -> diagnose -> resolve -> clear -> ACK -> unmask
```

### 12.6 Keep a plugin escape path

A declarative rule graph is suitable for FIR/register/bit mappings, but complex topology sorting, memory symbol algorithms, hardware procedures, and recovery orchestration require typed plugins or services.

---

## 13. Source anchors

The current conclusions are grounded in these open-source paths:

```text
open-power/skiboot
    hw/psi.c
    hw/prd.c
    external/opal-prd/opal-prd.c
    external/opal-prd/hostboot-interface.h

Linux PowerPC
    arch/powerpc/platforms/powernv/opal-prd.c

open-power/hostboot release-fw1120
    src/usr/diag/attn/ipl/attnsvc.C
    src/usr/diag/attn/runtime/attn_rt.C
    src/usr/diag/attn/common/attnsvc_common.C
    src/usr/diag/attn/common/attnprd.C
    src/usr/diag/prdf/common/prdfMain_common.C
    src/usr/diag/prdf/common/plat/p10/p10_proc.rule
    src/import/generic/memory/lib/utils/fir/gen_mss_fir.H
    src/import/chips/p10/procedures/hwp/memory/lib/fir/p10_fir.H
```

---

## 14. Next study point

The next PRDF study should trace one real P10 FIR bit end to end:

```text
HWP action programming
    -> local/chiplet/global propagation
    -> PRDF .rule entry
    -> Group and ErrorRegister lookup
    -> concrete Resolution objects
    -> threshold/callout/GARD/reset behavior
```

This will connect the hardware reporting policy to the PRDF rule compiler and resolution engine without leaving gaps between the two layers.
