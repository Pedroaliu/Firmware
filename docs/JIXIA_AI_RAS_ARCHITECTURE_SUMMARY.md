# Jixia AI 时代 RAS 总体方案

> 目标：参考 IBM Power Hostboot PRDF 的复杂硬件诊断思想，在 AI 时代构建一套面向云服务器的、可观测、可解释、可主动诊断、可恢复、可持续学习的 RAS（Reliability, Availability, Serviceability）体系。  
> 核心原则：**Power-style deterministic rule 作为可信骨架，AI 用来增强关联、推理、经验沉淀和规则发现，但最终硬件动作必须由确定性、可审计的策略执行。**

---

## 1. 为什么传统服务器 RAS 已经不够

传统服务器固件和 RAS 能力通常分散在多个软件域：

```text
Firmware / UEFI
    -> Boot 错误
    -> Silicon init
    -> HWP/FSP 返回状态
    -> 部分硬件错误

BMC
    -> SEL / PEL
    -> Sensor
    -> FRU
    -> Power / Thermal

OS / Hypervisor
    -> MCE / EDAC
    -> PCIe AER
    -> SMART / NVMe
    -> Driver reset
    -> Kernel log

Cloud Control Plane
    -> Workload impact
    -> Machine maintenance history
    -> Fleet statistics
```

这些系统各自都能看到一部分问题，但缺少统一的硬件语义和统一生命周期。

典型问题包括：

1. **多错误同时出现时难以判断因果链**
   - 能知道“哪里报错”，但不知道哪个是根因、哪个是级联错误。
   - 例如 PCIe AER、LTSSM recovery、NIC reset、I/O timeout 同时出现时，传统系统往往只能生成多条日志。

2. **复杂问题需要人工反复复现**
   - 增加打印；
   - 刷 BIOS；
   - 重新复现；
   - 再抓寄存器；
   - 再分析。
   - 缺少贯穿 Boot + Runtime 的持续诊断能力。

3. **日志很多，但缺少知识**
   - BMC、Firmware、OS、Cloud 的日志语义不同。
   - 同一个物理设备在不同层级可能有不同 ID，很难自动关联。

4. **机器重启后容易“失忆”**
   - 很多错误只看当前 Boot。
   - 无法持续理解一个 DIMM、Core、PCIe Link 或 SSD 在几个月内是如何逐步退化的。

5. **传统 RAS Rule 主要依赖人工经验**
   - Hostboot PRDF 已经非常强，但规则主要由硬件专家人工编写。
   - 新硬件代际、复杂拓扑和海量云服务器让人工维护规则越来越困难。

---

## 2. Power PRDF 给我们的启发

IBM Power Hostboot 的 PRDF（Processor Runtime Diagnostics）不是简单的 error handler。

它更像一个硬件诊断专家系统：

```text
Hardware Evidence
      ↓
Diagnostic Rule
      ↓
Context / Service Data
      ↓
Resolution
      ↓
Callout / Guard / Recovery
```

它的重要价值不是某一个 CE threshold，而是：

> **把多年硬件工程经验编码成一套可执行的诊断知识模型。**

Jixia 不应该抛弃这种确定性规则体系，而应该在其上增加 AI 时代的新能力。

因此下一代架构不是：

```text
Error
  ↓
AI Model
  ↓
直接操作硬件
```

而是：

```text
Structured Evidence
        +
Platform Topology
        +
Machine History
        +
Deterministic PRD Rules
        +
AI Reasoning
        ↓
Diagnosis / Hypothesis
        ↓
Deterministic Policy
        ↓
Contain / Recover / Verify
```

---

## 3. Jixia RAS 总体架构

```text
                         Fleet / Cloud RAS
                Case Mining / Rule Discovery / AI
                              │
                     Candidate Knowledge
                              │
                  Validate / Simulate / Review
                              │
                              ▼
+------------------------------------------------------------------+
|                    Jixia Machine Reliability Plane               |
|                                                                  |
|   Structured Event  ───────────────┐                              |
|                                    │                              |
|   PlatformGraph ───────────────────┼────> Incident Graph          |
|                                    │             │                |
|   Machine Health Journal ──────────┘             │                |
|                                                  ▼                |
|                                      +----------------------+     |
|                                      |   RAS Reasoner       |     |
|                                      |----------------------|     |
|                                      | Rule Matching        |     |
|                                      | Event Correlation    |     |
|                                      | Case Retrieval       |     |
|                                      | Hypothesis Ranking   |     |
|                                      | Active Diagnosis     |     |
|                                      +----------+-----------+     |
|                                                 │                 |
|                                      need more evidence?          |
|                                                 │                 |
|                                                 ▼                 |
|                                           HWP Probe               |
|                                                 │                 |
|                                                 ▼                 |
|                                      Structured HWP Result        |
|                                                 │                 |
|                                                 ▼                 |
|                                      Deterministic RAS Policy     |
|                                                 │                 |
|                               contain / retry / degrade / guard   |
|                                                 │                 |
|                                                 ▼                 |
|                                              Verify               |
|                                                 │                 |
|                                                 ▼                 |
|                                           Incident Case           |
+------------------------------------------------------------------+
                              │
              ┌───────────────┼────────────────┐
              ▼               ▼                ▼
             BMC              OS              Cloud
       critical state     rich history     fleet memory
```

---

## 4. 四层 RAS Intelligence Model

### L0 — Hardware Protection

硬件自身完成快速、确定性的错误检测与纠正：

```text
ECC
CRC
Retry
Spare
Link Recovery
Parity
Poison
```

这一层必须快、确定，不依赖 AI。

### L1 — Deterministic PRD-like Diagnostics

参考 IBM Power PRDF：

```text
signature
rule
threshold
resolution
callout
containment
```

它是整个 RAS 系统的可信骨架。

关键要求：

- 规则可解释；
- 规则可版本化；
- 规则可测试；
- 规则可回放；
- 最终 recovery action 必须来自这一层或受其约束。

### L2 — Semantic RAS Reasoner

AI 时代最值得创新的一层。

它不负责直接控制硬件，而负责：

```text
Event Correlation
Topology Correlation
Temporal Correlation
Historical Correlation
Case Retrieval
Hypothesis Generation
Evidence Gap Analysis
Active Diagnosis
```

它回答的问题不是简单的：

> “这个 SSD 会不会坏？”

而是：

> “现在发生了什么？”  
> “这些错误是不是同一个事故？”  
> “最可能的根因是什么？”  
> “哪些只是级联错误？”  
> “还缺什么证据？”  
> “应该调用哪个 HWP Probe？”  
> “过去有没有类似案例？”

### L3 — Fleet Intelligence

云上大量机器产生长期数据：

```text
100,000+ servers
      ↓
Incident Cases
      ↓
AI Mining
      ↓
Candidate Rules
      ↓
Simulation / Replay / Validation
      ↓
Human Review / Canary
      ↓
Signed Rule Package
      ↓
Jixia Deterministic Rule Engine
```

关键原则：

> **AI can learn or propose the rules; firmware executes accepted rules deterministically.**

---

## 5. Structured Event：先把“日志”升级为“事实”

AI 是否有价值，很大程度取决于输入数据质量。

传统日志：

```text
ERROR 0x1234
PCIe fail
MC warning
timeout
```

意义有限。

Jixia 应统一成结构化事件：

```text
Event {
    event_id
    event_type
    timestamp
    boot_epoch

    source_object
    target_object

    severity
    error_class

    syndrome
    register_snapshot

    owner
    service
    transaction_id

    parent_incident
}
```

这样 firmware / BMC / OS / Cloud / Jingjie 都使用同一套语义。

---

## 6. PlatformGraph：统一描述“这台机器是谁”

所有 RAS 推理必须建立在拓扑之上。

例如：

```text
Machine
 └─ Socket0
     ├─ Die0
     │   ├─ Core0
     │   ├─ Core1
     │   └─ MC0
     │       └─ Channel0
     │           └─ DIMM0
     │
     └─ PCIeRC0
         └─ Retimer0
             └─ NIC0
```

PlatformGraph 不只描述物理拓扑，还叠加：

```text
owner
health
trust
service
capability
firmware version
power state
```

因此错误不再只是：

```text
BDF 0000:31:00.0 error
```

而能变成：

```text
NIC0
  ↓
Retimer0
  ↓
PCIeRC0
  ↓
Socket0
  ↓
owner = NetworkService
  ↓
affected workload / VM
```

---

## 7. Incident Graph：把一堆日志重构成一个“故障故事”

现代服务器中的错误往往是链式传播。

例如：

```text
PCIe PHY margin degradation
          ↓
Replay ↑
          ↓
LTSSM Recovery
          ↓
AER Corrected Storm
          ↓
NIC Reset
          ↓
I/O Timeout
```

传统系统可能生成 5 条独立日志。

Jixia 应形成：

```text
Incident #4817

Primary suspect:
    PCIeRC1 / Retimer0 / NIC0 Link

Likely root cause:
    Gen5 Link Margin Degradation

Supporting evidence:
    replay burst
    LTSSM recovery
    repeated corrected AER
    previous retrain history

Contradicting evidence:
    NIC internal health normal

Current action:
    retrain link

Next action:
    degrade Gen5 -> Gen4 if recurrence
```

这比单个错误码有价值很多。

---

## 8. Hypothesis-driven RAS：让诊断系统主动“做检查”

这是 AI + HWP 最值得做的一件事。

传统：

```text
Error
 ↓
已有数据
 ↓
Rule
 ↓
Action
```

Jixia：

```text
Observe
 ↓
Generate Hypotheses
 ↓
What evidence can distinguish them?
 ↓
Run Safe HWP Probe
 ↓
New Evidence
 ↓
Update Hypothesis
 ↓
Policy Action
```

例如：

```text
H1: DIMM Rank Failure
H2: Memory Controller Failure
H3: Fabric Corruption
```

Reasoner 可以要求：

```text
read syndrome distribution
read channel counters
run targeted scrub
read neighboring rank status
run DDR margin check
```

HWP 返回结构化结果后再进一步诊断。

HWP 因此不仅是：

```text
DDR_INIT()
PCIE_INIT()
```

还应该支持：

```text
DDR_MARGIN_CHECK()
PCIE_LINK_DIAG()
FABRIC_PROBE()
CORE_HEALTH_CHECK()
```

---

## 9. Machine Health Journal：机器不能每次重启都失忆

Jixia 为每台机器维护长期健康历史：

```text
Machine UUID
    |
    +-- BootEpoch 1
    +-- BootEpoch 2
    +-- ...
    +-- Runtime Events
```

例如 DIMM：

```text
Boot 1
training_margin = 91
CE = 2

Boot 20
training_margin = 78
CE = 18

Boot 50
training_margin = 47
CE = 140
scrub_corrected = 24
```

即使每一次都没有超过传统 threshold，长期趋势已经说明硬件正在退化。

这里 AI 可以做：

```text
trend
rate
acceleration
cross-signal correlation
peer comparison
```

但模型输出首先应作为：

```text
health hint
risk estimate
diagnostic evidence
```

而不是直接执行危险硬件动作。

---

## 10. Case Memory：让机器记住“以前怎么坏过”

每一个真正解决过的问题都应该留下 Case：

```text
trigger
topology
event sequence
FFDC
diagnosis
actions tried
which action worked
final root cause
affected components
firmware version
hardware revision
```

以后遇到新事故：

```text
Current Incident
      ↓
Search Similar Cases
      ↓
Case #104
Case #827
Case #1051
```

AI 可以解释：

> 当前错误和过去 3 次 Retimer 退化事件非常相似，因为都出现了 AER → LTSSM Recovery → Replay Burst 的序列。

这是比黑盒 failure score 更可解释的 AI-RAS。

---

## 11. BMC / OS / Cloud 三级数据存储

BMC 不应该存所有日志。

它的优点：

```text
persistent
always-on
hardware access
```

但资源有限。

因此采用三级架构：

### Level 0 — BMC / Firmware Local Journal

保存：

```text
Machine Identity
Health Summary
Critical Incidents
Recent FFDC
Upload Watermark
Unacknowledged Events
```

目标是：

> 即使 OS 挂掉、机器重启、网络不可达，关键证据仍然存在。

### Level 1 — Host OS Journal

OS 资源丰富，可保存：

```text
full structured event history
trace
telemetry
kernel RAS logs
device logs
incident database
case database
```

例如：

```text
/var/lib/jixia/
    events/
    trace/
    incidents/
    health/
    cases/
```

### Level 2 — Cloud / Fleet RAS

长期保存：

```text
Machine history
Fleet incidents
Hardware batches
Firmware versions
Repair outcomes
Cross-machine patterns
```

用于：

```text
fleet analysis
rule mining
case learning
AI reasoning
failure trend analysis
```

### 上传机制

BMC 不需要永久囤积日志，而使用：

```text
sequence number
      ↓
upload
      ↓
ACK watermark
      ↓
confirmed data can be compressed / expired
```

也就是：

> BMC 保存“状态 + 尚未安全转移的关键证据”，而不是充当大型日志服务器。

---

## 12. AI 如何改变 PRD Rule

传统：

```text
Hardware Expert
      ↓
Experience
      ↓
Hand-written Rule
```

AI 时代：

```text
Fleet Incident Database
      ↓
AI Rule Mining
      ↓
Candidate Rule
      ↓
Jingjie Fault Injection / Replay
      ↓
Regression Validation
      ↓
Human Review
      ↓
Canary
      ↓
Signed Rule Package
      ↓
Production
```

例如 AI 发现：

```text
Gen5
+
Retimer Temp > X
+
Replay slope > Y
+
LTSSM recovery >= N
```

与某类 Retimer 退化高度相关。

AI 可以生成：

```text
Candidate Rule:
    PCIE_RETIMER_MARGIN_DEGRADATION
```

但不能直接上线。

必须经过：

```text
simulation
replay
regression
review
versioning
signing
```

---

## 13. Jingjie Simulator 的作用：Counterfactual Diagnosis

线上机器不能随便做危险实验。

例如：

```text
“要不要 reset Memory Controller 看看？”
```

真实服务器上可能风险很大。

但可以把：

```text
PlatformGraph Snapshot
+
Incident Trace
+
Register State
```

送给 Jingjie。

然后验证不同假设：

```text
Hypothesis A:
DDR Channel Failure
    -> reproduce 93% evidence

Hypothesis B:
PCIe DMA Failure
    -> reproduce 27% evidence
```

这就是：

> **用 simulator 做故障假设验证。**

因此 Jixia RAS 和 Jingjie 最终形成闭环：

```text
Production Incident
      ↓
Structured Evidence
      ↓
Replay in Jingjie
      ↓
Fault Injection
      ↓
Hypothesis Validation
      ↓
Rule Improvement
      ↓
Regression
```

---

## 14. Recovery：模型只建议，Policy 才执行

这是安全边界。

AI 可以输出：

```text
Hypothesis
Confidence
Evidence
Recommended Action
```

但真正执行必须经过：

```text
RAS Policy Engine
```

例如：

```text
AI Recommendation:
    Offline Core17

Policy checks:
    evidence sufficient?
    redundancy available?
    workload impact acceptable?
    cloud policy allows?
    recovery path verified?

          ↓

Approved Action:
    offline Core17
```

所以原则是：

> **模型可以建议，Policy 才有权执行。**

---

## 15. 未来 RAS 的核心闭环

最终我们希望做到：

```text
Detect
  ↓
Normalize
  ↓
Correlate
  ↓
Understand
  ↓
Hypothesize
  ↓
Probe
  ↓
Decide
  ↓
Contain
  ↓
Recover
  ↓
Verify
  ↓
Remember
  ↓
Learn
```

而不是传统的：

```text
Error
 ↓
Log
 ↓
Reboot
```

---

## 16. 与传统 AI 故障预测方案的区别

传统 AI-RAS 常见方向：

```text
DDR CE curve
SSD SMART
temperature
telemetry
      ↓
ML model
      ↓
failure probability
```

Jixia 不排斥这种能力，但它不是核心。

我们的重点是：

```text
Prediction
    只是其中一个能力

更重要的是：
    root-cause reasoning
    event correlation
    active diagnosis
    case memory
    machine history
    rule discovery
    deterministic recovery
```

核心问题从：

> “它会不会坏？”

升级为：

> “现在发生了什么？”  
> “为什么？”  
> “影响什么？”  
> “还缺什么证据？”  
> “怎么恢复？”  
> “恢复以后真的好了吗？”  
> “以后再遇到能不能更快识别？”

---

## 17. 面向云服务器的最终定位

Jixia RAS 不只是一个错误处理模块。

它应该成为：

> **Hardware Reliability Control Plane**

最终能力：

```text
Unified Hardware Semantics
        +
Power-style Diagnostic Rules
        +
AI Reasoning
        +
Machine Long-term Memory
        +
Active HWP Diagnosis
        +
Deterministic Recovery
        +
Fleet Learning
        +
Simulator Verification
```

最终形成一个能持续回答以下问题的系统：

```text
机器现在健康吗？
哪里正在退化？
当前错误的根因是什么？
哪些错误只是级联现象？
影响了哪些资源和业务？
是否可以继续降级运行？
应该进行什么诊断？
应该采取什么恢复动作？
恢复是否成功？
过去有没有类似事故？
整个机群能否从这次事故中学到新规则？
```

---

## 18. Jixia RAS 的设计原则

1. **Deterministic rules remain the trusted spine.**
2. **AI does not directly control critical hardware.**
3. **All important evidence should be structured, not free-form logs.**
4. **Topology is part of diagnosis, not post-processing.**
5. **A reboot must not erase machine health history.**
6. **HWP is both an initialization mechanism and an active diagnostic mechanism.**
7. **BMC stores critical state, not unlimited logs.**
8. **OS provides rich local storage; Cloud provides long-term fleet memory.**
9. **AI may discover rules; rules must be validated before production.**
10. **Every recovery action must be followed by verification.**
11. **Production incidents should be reproducible in Jingjie whenever possible.**
12. **RAS knowledge must become better as the fleet operates longer.**

---

## 一句话总结

> **Power PRDF 把硬件专家经验变成了可执行规则；Jixia 要做的是在这个可信规则骨架之上，引入结构化硬件语义、机器长期记忆、AI 诊断推理、主动 HWP 探测、Fleet 规则学习和 Jingjie 仿真验证，把传统 RAS 从“错误处理系统”升级为“持续学习的服务器硬件可靠性控制平面”。**
