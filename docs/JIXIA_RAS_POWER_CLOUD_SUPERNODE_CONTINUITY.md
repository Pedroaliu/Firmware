# Jixia RAS：从 POWER、Cloud 到 AI Supernode 的连续性设计

> 本文是 `JIXIA_AI_RAS_ARCHITECTURE_SUMMARY.md` 的架构补充，记录 Jixia 在 AI 超节点时代对 RAS 目标、故障域、降级运行和软硬件协同边界的进一步定义。

## 1. 核心结论

传统 RAS 的目标在不同计算时代发生了三次变化：

```text
POWER / enterprise server
    Keep the machine alive

Cloud / scale-out
    Keep the service alive

AI supernode / large synchronous computation
    Keep useful computation making forward progress
```

Jixia 不应简单复制 POWER，也不应沿用“机器坏了就 drain”的纯云计算策略。下一代 RAS 应组合三类能力：

- POWER 的 fault containment、FFDC、deterministic diagnosis、deconfiguration；
- Cloud 的 failure-aware software、resource scheduling、machine evacuation；
- AI 时代的 topology-aware、workload-aware、accelerator-aware recovery。

最终目标不是单纯提高单机 uptime，而是：

> **Keep useful computation making forward progress under component failures.**

---

## 2. 三代 RAS 的优化对象不同

### 2.1 POWER：保护 Machine

POWER 高端系统的基本假设是：一台机器本身就是重要资产和故障域。

因此 RAS 倾向于：

```text
Fault
  ↓
Detect
  ↓
Correct / Retry
  ↓
Diagnose
  ↓
Contain
  ↓
Deconfigure / Guard / Repair
  ↓
Machine continues
```

即使失去部分 core、memory、I/O capacity，只要系统仍能提供服务，degraded operation 就有价值。

### 2.2 Cloud：保护 Service

Cloud 将最终优化目标从 machine availability 提升到 fleet/service availability：

```text
Machine unhealthy
      ↓
Drain workload
      ↓
Scheduler reschedules
      ↓
Machine isolated / repaired later
      ↓
Service continues
```

在高度同质化的资源池中，一台严重 down-configuration 的机器甚至可能不适合继续留在 production pool，因为它会破坏：

- capacity predictability；
- topology predictability；
- performance isolation；
- scheduler simplicity。

因此 Cloud 的重要思想是：

> Hardware is allowed to fail; software must tolerate machine failure.

### 2.3 AI Supernode：保护 Computation

大规模 AI 训练/推理的关键变化是：大量 accelerator 不再只是承载彼此独立的 workload，而可能共同完成同一个同步计算任务。

例如：

```text
P0 P1 P2 ... Pn
 \  |  /      /
  collective
      ↓
 training step
      ↓
 parameter update
```

此时一个 accelerator、HBM、link、switch 或 communicator 的故障可能让大量仍然健康的 accelerator 等待，从而使整个任务停止前进。

因此 AI RAS 必须直接观察：

- job 是否仍在完成 training step；
- tokens/s 是否持续推进；
- collective 是否持续完成；
- rollback distance 有多大；
- recovery 后能否重新建立有效计算。

---

## 3. Useful Forward Progress 的定义

“机器活着”与“计算活着”不是一回事。

例如：

```text
Host OS          alive
CPU              alive
63/64 GPU        alive
Fabric           mostly alive
Power            on

but:

training_step = 10003
training_step = 10003
training_step = 10003
```

如果 collective 因一个 rank 卡住，所有 GPU 都可能处于 busy/wait 状态，但没有新的有效 step 完成。

因此 Jixia 应把 forward progress 作为一等信号。

可观察指标包括：

```text
completed_steps / time
completed_tokens / time
useful_flops / elapsed_time
collective_completion_rate
checkpoint_rollback_distance
recovery_time_to_next_valid_step
```

RAS 目标不是保证每个 component 永远健康，而是减少：

```text
Fault
  ↓
Lost useful work
  ↓
Rollback
  ↓
Recovery overhead
  ↓
Time without progress
```

---

## 4. Supernode：OS Boundary 与 Fabric Boundary 分离

传统服务器通常近似：

```text
one host OS
   │
CPU + local accelerators
   │
network
   │
another server
```

AI supernode 则扩大 scale-up fabric，使多个 accelerator endpoint 跨 board/chassis/rack 形成一个更大的紧耦合计算域：

```text
Linux0         Linux1         Linux2
  │              │              │
CPU0           CPU1           CPU2
  │              │              │
P0 P1          P2 P3          P4 P5
  \              |             /
   ===== Scale-up Fabric ======
```

因此必须明确：

> **OS boundary != fabric boundary != RAS fault domain.**

一个 supernode 可以包含多个 Host OS domain，但 accelerator data plane 仍属于同一个大规模 scale-up domain。

Scale-up 与 scale-out 的区别应理解为：

```text
Scale-up:
    扩大紧耦合计算/内存/accelerator domain
    强调低延迟、高带宽、peer access、collective

Scale-out:
    增加相对独立的 server/supernode
    强调 network/RDMA/message based expansion
```

AI data center 实际是：

```text
Supernode A  --scale-out--  Supernode B
     │                          │
  scale-up                   scale-up
     │                          │
 accelerators                accelerators
```

---

## 5. XPU 自主执行的正确边界

不能把 accelerator 的自主性理解成“protocol ASIC 自己会进行复杂任务调度”。

现代 XPU 内部更接近一个异构可编程系统：

```text
XPU
│
├─ programmable compute cores
│    GPU SM / NPU AI Core
│
├─ optional embedded control processors
│    firmware / runtime / management
│
├─ hardware scheduler / microcode
│
├─ DMA / copy / collective engines
│
└─ protocol engines
     NVLink-like / UB-like / PCIe / HBM
```

需要区分三种 intelligence：

```text
Host / Cluster:
    WHAT / WHO / POLICY

Device Runtime / Program:
    HOW TO EXECUTE

Protocol Hardware:
    HOW TO TRANSFER
```

协议 ASIC 可以固定逻辑处理：

- request/response；
- address decode；
- route；
- credit；
- retry；
- remote read/write/atomic。

但以下逻辑需要可编程实体或明确硬件 scheduler：

- task decomposition；
- work stealing；
- peer selection；
- dynamic load balancing；
- dependency scheduling；
- collective orchestration；
- fault-aware task redistribution。

该可编程实体可以是：

- accelerator core 上的 persistent/device program；
- embedded ARM/RISC-V/control processor 上的 firmware/RTOS/runtime；
- microcode；
- fixed-function hardware scheduler；
- 或上述能力的组合。

---

## 6. Resource Envelope：Host 与 Device 的长期边界

Jixia 应明确采用 Resource Envelope 思想。

Host/ArchHV/cluster scheduler 决定：

```text
这个 job 拥有：

P1 P2 P3 P4
指定 HBM capacity
指定 VA range
指定 fabric endpoints
指定 queues / communicators
指定 power / security / trust constraints
```

然后 Device Execution Domain 可以在这个 envelope 内自主：

```text
task scheduling
P2P
remote load/store/atomic
collective
work stealing
follow-up launch
local load balancing
```

但不能自行突破 ownership/security boundary 去征用未授权 accelerator。

因此稳定边界是：

> **Host decides what resources the workload owns; device software decides how those resources execute the work; protocol hardware executes the transactions.**

---

## 7. AI Supernode 的 RAS Fault Domain 不再等于 Server Node

Jixia 不应把全部错误简单压缩成 `node healthy / node failed`。

至少需要把以下对象作为一等 fault domain：

```text
Host CPU
Host OS domain
Accelerator compute core/group
Accelerator device-control processor/firmware
HBM stack/channel/page
Scale-up link
Scale-up switch
PCIe/CXL endpoint
Scale-out NIC/DPU
Power/thermal domain
Management Complex
```

例如：

```text
XPU compute core bad
    !=
XPU HBM bad
    !=
XPU device-control processor bad
    !=
scale-up link bad
    !=
Host Linux dead
```

这些错误的 containment、recovery owner 和对 computation progress 的影响完全不同。

---

## 8. 从 Downconfiguration 升级到 Topology-aware Degradation

POWER 式降级通常关心：

```text
bad core -> disable core
bad page -> retire page
bad FRU -> guard/deconfigure FRU
```

AI supernode 不能简单执行：

```text
bad accelerator -> disable accelerator
```

因为 accelerator topology 直接影响：

- tensor parallel；
- pipeline parallel；
- data parallel；
- collective ring/tree；
- memory locality；
- rail balance；
- model placement。

因此 Jixia 的 degraded operation 应变成：

```text
Fault
  ↓
Contain physical fault
  ↓
Update PlatformGraph
  ↓
Compute viable degraded topology
  ↓
Notify Host / AI Runtime
  ↓
Rebuild communicator / remap workload
  ↓
Verify next useful step completes
```

建议资源健康状态至少允许：

```text
HEALTHY
SUSPECT
DEGRADED
CONTAINED
RECONFIGURING
DEGRADED_OPERATIONAL
DRAIN_REQUIRED
FAILED
```

并把 capacity、performance、redundancy、topology、repairability 一并暴露给上层，而不是只提供 GOOD/BAD。

---

## 9. Jixia Management Complex 在 AI 时代的职责升级

Management Complex 的意义不只是“比 BMC 更靠近 silicon”。

在 supernode 中，它需要成为受保护的 physical-health authority：

```text
Management Complex
│
├─ CPU/Core RAS
├─ Accelerator RAS
├─ HBM/DDR RAS
├─ Scale-up fabric RAS
├─ PCIe/CXL RAS
├─ power/thermal
├─ FFDC / flight recorder
├─ topology / PlatformGraph health
└─ recovery workflow
```

它应知道：

> 哪个物理资源正在失败，故障传播到哪些 topology object，当前还剩什么可运行拓扑。

但它不应该取代 Host/AI runtime 的语义决策。

AI runtime 才知道：

> 这个 accelerator 当前承载哪个 tensor-parallel rank、MoE expert、VM、process 或 workload state。

因此继续坚持：

> **Management Complex owns physical-health semantics; software owns workload/ownership semantics.**

---

## 10. Dual-View RAS 在 Supernode 中继续成立

精确执行错误仍需靠 in-band context：

```text
faulting CPU/XPU execution context
PC / privilege / address / process / kernel state
```

而 Management Complex 提供 out-of-band/global evidence：

```text
fabric counters
HBM syndrome
power history
switch state
first-failure state
topology
cross-device correlation
```

最终 Case 应合并：

```text
Precise Software Context
          +
Global Physical Evidence
          ↓
      RAS Incident
```

这比单纯 Firmware First 或 Kernel First 更适合 Jixia。

---

## 11. AI RAS 的 Recovery Ladder

Jixia 应优先保护 computation progress，恢复动作从小到大升级：

```text
L0  hardware correction
    ECC / CRC / replay

L1  local engine retry
    queue restart / link replay / targeted scrub

L2  local component containment
    page retire / lane degrade / compute-unit disable

L3  topology recovery
    reroute / rebuild collective / replace peer

L4  workload reconfiguration
    remap rank / shrink world / migrate state / spare accelerator

L5  supernode drain
    only when useful progress cannot be restored safely
```

这里 `reset` 只是 recovery mechanism 之一，不应等同于 recovery policy。

---

## 12. 未来 Jixia 应优化的指标

传统：

```text
Machine Uptime
MTBF
MTTR
```

仍然重要，但 AI supernode 还需要：

```text
Useful Work Availability
Time to Next Valid Step
Lost Compute Time
Checkpoint Rollback Distance
Degraded Topology Efficiency
Recovery Success Rate
Fault Containment Radius
```

特别建议定义：

```text
Useful Work Availability
=
Time making valid forward progress
/
Total elapsed time
```

它不替代传统 availability，但更贴近 AI workload 的实际价值。

---

## 13. 对 Jixia 当前架构的直接要求

当前方案需要明确保留以下能力：

1. **Management Complex**
   - independent lifetime；
   - protected sideband；
   - accelerator/fabric first-class RAS；
   - Host-dead FFDC；
   - retained state；
   - redundancy/failover。

2. **PlatformGraph**
   - 从 CPU/DDR/PCIe 扩展到 XPU/HBM/switch/link；
   - 同时描述 health、capacity、ownership、trust、performance、fault-domain。

3. **Structured Event**
   - one event model；
   - Host、MC、BMC、AI runtime 多 delivery path；
   - severity、priority、recovery owner 必须分离。

4. **Host/AI Runtime Contract**
   - async recovery request；
   - immediate ACK != recovery completed；
   - communicator/topology/workload reconfiguration 由软件层完成。

5. **Simulation / Fault Injection**
   - simulator 与 silicon 使用同一 fault contract；
   - 可注入 XPU、HBM、link、switch、Host OS、device-control processor 故障；
   - 验证最终是否恢复 useful forward progress，而不仅是 component reset success。

---

## 14. 后续实验方向

为了真正理解 supernode execution 和 RAS，计划建立 `Generic Scale-Up Fabric` 教学模型：

```text
Lab 1  Host -> Command Queue -> Accelerator
Lab 2  Accelerator P2P remote memory
Lab 3  Fabric switch / request / response / credit / retry
Lab 4  Two Host OS domains + one accelerator fabric
Lab 5  Multi-accelerator AllReduce
Lab 6  Training-step forward progress
Lab 7  Fault + retry + reroute + topology/workload recovery
```

重点不是一开始精确模拟 NVLink 或 UnifiedBus，而是先验证：

```text
Host policy
    ↓
Resource Envelope
    ↓
Device Execution Plane
    ↓
Protocol/Data Plane
    ↓
Fault
    ↓
Containment / Reconfiguration
    ↓
Useful Forward Progress restored
```

---

## 15. 当前稳定判断与开放问题

### 稳定判断

- AI RAS 不等于更强的单机 RAS；
- supernode 的 system boundary 可以大于 OS boundary；
- accelerator endpoint 不能简单等同于 Host compute node；
- device autonomy 不等于无软件，而是控制软件可以下沉到 device execution domain；
- protocol ASIC 提供 mechanism，不应被假设具备任意高层 scheduling intelligence；
- topology-aware degradation 是 AI RAS 的核心能力；
- 最终恢复判断必须包含 workload forward progress verification。

### 仍需研究

- NVIDIA/Ascend 等具体 XPU 中 device-control processor、SM/AI Core、firmware 和 hardware scheduler 的真实职责边界；
- scale-up fabric 在 load/store、atomic、coherence、protection 上的精确语义；
- accelerator failure 后 communicator/world-size 动态重构的实际软件约束；
- Management Complex 与 XPU 内部 control processor 的权限和故障责任边界；
- supernode 级 spare、reroute、degraded topology 的最优抽象；
- confidential workload 下 MC/XPU sideband access 的安全限制。

---

## 16. 一句话原则

> **POWER 教我们让硬件故障可被隔离，Cloud 教我们让软件接受机器会失败，AI Supernode 要求我们把两者重新组合，让整个计算在局部组件失败后仍能继续产生有效进展。**
