# ArchFW / RVSoC-Sim：LPAR 与 CECSIM 式协同设计方向 v0.1

## 状态

本文记录 2026-08-05 形成的阶段性架构判断，用于避免后续讨论重新退回到“普通固件 + 普通 KVM + 普通模拟器”的默认路线。

这不是最终规格，而是一份研究方向与设计原则。后续实现可以修正具体机制，但不应在没有充分论证的情况下丢失本文所记录的核心问题意识。

---

## 1. 项目的根本目标

ArchFW 与 RVSoC-Sim 的主要目的不是尽快做出一个可商用固件、一个兼容 KVM 的 Hypervisor，或者一个追求 benchmark 排名的处理器。

项目的核心目的是学习：

- 一台服务器如何从处理器、SoC、固件、Hypervisor、分区固件、I/O 服务、管理平面和 RAS 共同构成；
- 不同系统层之间如何划分职责、权限、故障域和升级边界；
- 当“逻辑分区”从上电开始就是机器的一等对象时，CPU、内存、I/O、中断、安全、计量和恢复应该如何协同设计；
- 如何使用模拟器在真实硬件出现之前验证固件、平台状态机、动态配置和故障恢复；
- 如何通过非主流设计理解主流设计的 trade-off。

主流 x86/ARM + Linux/KVM 已经足够支持今天的大多数云计算场景。研究 IBM POWER / PowerVM / LPAR，不是为了证明其市场方案优于 KVM，而是为了接触一种不同的计算机观，并从中获得更丰富的架构判断能力。

计算机系统不是消灭复杂性的科学，而是决定：

- 复杂性放在哪一层；
- 哪个组件拥有资源；
- 谁可以修改状态；
- 故障发生时影响谁；
- 升级时需要替换谁；
- 性能、隔离、灵活性、成本和可验证性如何取舍。

---

## 2. 从 Host OS 虚拟化转向 Firmware-Native Partitioning

常见 KVM 心智模型为：

```text
Hardware
   |
Host Linux
   |
KVM / QEMU / vhost / VFIO
   |
Guest VMs
```

物理 CPU、内存和设备首先属于 Host OS，再由 Host OS 创建和管理虚拟机。

LPAR / PowerVM 展示了另一种模型：

```text
Physical Machine
   |
Host Firmware
   |
Firmware Hypervisor
   +---------------------------+
   |             |             |
Partition FW   Partition FW   Partition FW
   |             |             |
Linux LPAR     AIX LPAR       Service/VIOS LPAR
```

这里没有一台天然高于其他业务系统的 Host OS。多个操作系统由平台直接建立为逻辑机器，在分区执行模型上彼此平级；它们可以承担不同职责，但都运行在 Firmware Hypervisor 之上。

ArchFW 后续应研究并逐步形成如下组件：

- **ArchFW**：整机初始化、PlatformGraph、安全、全局编排与 RAS；
- **ArchHV**：最小 Type-1 / Firmware Hypervisor；
- **ArchLPAR**：逻辑分区对象与资源契约；
- **ArchPFW**：每个逻辑分区自己的启动固件；
- **ServiceRoot LPAR**：拥有复杂物理设备驱动并向其他分区提供服务；
- **ArchVIO**：稳定的虚拟 I/O 与跨分区通信协议；
- **ArchMC**：类似 HMC 的管理与服务控制面。

正常主路径可以是：

```text
Boot0
  |
ArchFW Early Executive
  |
Protected ArchFW Services
  |
ArchHV
  +----------------------+
  |                      |
ArchPFW                 ArchPFW
  |                      |
ServiceRoot LPAR        Guest LPAR
```

Petitboot 可以作为裸金属兼容路径或恢复路径，但不应成为 ArchFW 主架构不可替换的中心。

---

## 3. LPAR 要学习的核心思想

### 3.1 Partition 是机器对象，不是大型 Host 进程

一个 ArchLPAR 最终不只是 `vCPU + RAM`，而应包含：

- ArchLPID / VMID；
- vCPU 与算力 entitlement；
- 逻辑内存视图；
- 地址翻译上下文；
- 中断上下文；
- 虚拟或直通设备；
- Partition Firmware；
- 独立时间视图；
- BootEpoch；
- 安全测量状态；
- 性能、能源和 I/O 记账；
- RAS、健康与恢复状态；
- 迁移兼容级别。

### 3.2 CPU 资源是一份契约

不仅描述 vCPU 数量，还应研究：

- Virtual Processor Count；
- Entitled Capacity；
- Capped / Uncapped；
- Uncapped Weight；
- 实际 consumed capacity；
- cede / prod / confer；
- dispatch、stolen、donated 与 scaled time；
- Cache、NUMA 与设备亲和性。

### 3.3 Partition Identity 应贯穿整个平台

ArchLPID 不应只是管理软件中的编号。长期目标是让它进入：

- TLB / G-stage translation；
- 中断投递；
- IOMMU context；
- 设备 ownership；
- Trace；
- 性能计数；
- NoC / DDR 流量归属；
- 能源核算；
- RAS 事件与 FFDC。

### 3.4 物理驱动不进入最小 Hypervisor

ArchHV 不应包含 NVMe、FC、网卡、USB 等复杂物理驱动。

ArchHV 只提供：

- 虚拟适配器；
- 消息队列；
- Memory Grant；
- 授权跨分区复制或 DMA；
- 虚拟中断；
- 分区生命周期；
- 资源隔离与 ownership。

复杂物理设备由 ServiceRoot LPAR 拥有。Guest 只面对稳定的 ArchVIO 设备模型。

### 3.5 RAS 以 Partition 为作用域

每次错误都应尽量回答：

- Fault source 是什么；
- 资源 owner 是哪个 ArchLPID；
- 错误是否已 containment；
- 是否只需冻结、重置或迁移一个 LPAR；
- 哪些其他分区可以继续运行；
- 应生成哪些 FFDC；
- 是否需要降级、隔离、备用路径或维修。

---

## 4. CECSIM 是本项目的重要参考模型

CECSIM 的关键价值不是“可以执行指令”，而是把以下对象放入同一验证环境：

- 处理器和多处理器模型；
- Host Firmware；
- 微码或低层 Firmware；
- LPAR；
- I/O 模型；
- 管理控制面；
- 动态配置；
- 故障注入；
- Trace；
- 自动回归；
- Coverage。

这与我们的方向高度一致：

> RVSoC-Sim 不只是性能模拟器，而是 ArchFW 的可执行架构规范与完整固件验证平台。

### 4.1 固件与模拟器共同设计

ArchFW 在设计状态机、接口和错误路径时，应同步考虑：

- 模拟器如何观测状态；
- 如何精确等待某个 Firmware event；
- 如何在关键窗口注入故障；
- 如何判断恢复是否成功；
- 同一个测试如何运行于 QEMU、RVSoC-Sim、RTL 和未来真机；
- 如何抽取目标端 Coverage；
- 如何 checkpoint / replay。

不应等固件基本完成后，再给模拟器“补一个能跑它的模型”。

### 4.2 ArchSimCall

参考 CECSIM 的 SIMCALL，后续可以定义测试构建专用接口：

```text
TEST_BEGIN
TEST_EVENT
TRACE_MARKER
INJECT_POINT
CHECKPOINT
TEST_PASS
TEST_FAIL
TEST_COMPLETE
```

在 RVSoC-Sim 中它是精确事件接口；在 QEMU 中可由 magic MMIO 或测试专用 ecall 实现；正式产品构建中应禁用或严格隔离。

### 4.3 PhysicalModelGraph 与 FirmwarePlatformGraph 分离

模拟器中的真实硬件模型不能直接等同于 Firmware 认为的平台配置。

必须能够故意制造：

```text
PhysicalModelGraph != FirmwarePlatformGraph
```

从而验证：

- 枚举遗漏；
- 配置陈旧；
- 热插拔；
- 冗余链路切换；
- 错误拓扑；
- 设备消失；
- Firmware 是否能拒绝非法状态并进入恢复流程。

### 4.4 多精度验证

长期保留：

```text
Fast Functional Model
        <->
Transaction / Timing Model
        <->
RTL / Verilator Co-simulation
```

快模型用于大规模回归、启动、动态配置和 RAS 状态机；精确模型用于 Cache、TLB、NoC、DDR、IOMMU、中断和 cycle 级资源争用；RTL 用于局部关键模块验证。

---

## 5. RVSoC-Sim Core 的方向

RVSoC-Sim 最终应实现自己的 RISC-V Core。为了控制复杂度，第一代 Core 参考：

- BOOM 的模块划分、乱序执行基本结构和教学可读性；
- 香山在高性能前端、乱序窗口、内存子系统、验证和工程组织上的思想；
- IBM POWER 在 SMT、Partition Identity、RAS、Trace、资源计量和 Hardware/Firmware Co-design 上的思想。

这里的“参考”不是复制完整 BOOM 或香山，也不是追求与其同等级性能。

第一代目标应是一个可理解、可验证、可逐步增加精度的教学型 Core：

```text
RV64
  |
简单前端
  |
Rename / Dispatch
  |
小型 Issue Queue
  |
整数执行 + Load/Store
  |
ROB / Commit
  |
Cache / TLB
```

优先级：

1. 正确性和可观察性；
2. 可与 ArchFW / ArchHV / ArchLPAR 联动；
3. 可支持故障注入和 RAS；
4. 可做 timing / cycle 分析；
5. 再考虑更宽、更深和更复杂的性能优化。

不应过早加入大量自定义 LPAR 指令或 CSR。首先使用标准 RISC-V H 扩展、AIA/IMSIC 和 IOMMU 建立完整软件语义；只有在实验明确暴露瓶颈后，才研究：

- 显式 LPID register；
- LPID-tagged TLB；
- LPID-aware performance counters；
- LPID-aware RAS queue；
- Partition-scoped reset；
- 快速 partition switch；
- split-core / simultaneous partitions；
- partition working-set prefetch。

---

## 6. 两个项目的职责边界

### Firmware 仓库

负责：

- ArchFW；
- ArchHV；
- ArchLPAR contract；
- ArchPFW；
- ArchVIO protocol；
- ServiceRoot / ArchMC protocol；
- Secure Boot；
- RAS policy；
- 目标端 ArchTest；
- QEMU RISC-V virt 上的功能原型。

### RVSoC-Sim 仓库

负责：

- Core、Cache、TLB、NoC、DDR、PCIe、IOMMU、中断模型；
- ArchLPID 在微架构数据路径中的实现；
- 资源争用和性能隔离；
- per-LPAR telemetry；
- RAS fault injection；
- CECSIM 式脚本、事件、Trace、Coverage、Replay；
- 多精度和 RTL 协同模拟。

两者通过统一的：

- PlatformGraph；
- ArchIDL；
- ArchSimCall；
- Trace schema；
- Fault schema；
- BootEpoch / ArchLPID；
- 测试用例；

共同演进。

---

## 7. 近期实施顺序

当前已完成：

- M00-00：最小启动、栈、BSS、UART；
- M00-01：最小 M-mode fatal trap。

近期仍应先完成底层控制基础：

```text
M00-02  Complete TrapFrame
M00-03  Recoverable Trap + mret
M00-04  Timer Interrupt
M00-05  Per-hart State
M00-06  Privilege Transition
```

随后进入：

```text
M01  First ArchLPAR
  HS / VS
  hgatp / VMID
  Guest ecall / hcall
  Virtual timer

M02  Partition Contract
  ArchLPID
  State machine
  VPA
  Entitlement
  Capped / Uncapped
  PURR-like accounting

M03  Two Peer LPARs
  Service LPAR
  Guest LPAR
  Virtual console
  Cross-LPAR message
  Memory grant
  Virtual block

M04  Partition RAS
  Guest fault containment
  Device ownership
  Per-LPAR FFDC
  Freeze / Reset / Recover

M05  Mobility and Persistent State
  Dirty tracking
  Stop-and-copy
  Virtual timebase
  Compatibility profile
```

CECSIM 式测试、事件、回归和 Coverage 不应等到 M05 才开始，而应从 M00-02 逐步加入。

---

## 8. 设计护栏

1. **不是 IBM cosplay**：学习根本问题和职责划分，不机械复制 HPT、XIVE、TCE、PAPR 名字和接口。
2. **不是另一个 mini-KVM**：KVM 作为主流对照组和知识基线，不作为唯一架构模板。
3. **不是过早造 ISA**：先用标准 RISC-V 能力完成软件语义，再由实验决定硬件增强。
4. **不是先做完整 Core**：先把 Firmware、LPAR contract、验证接口和模拟框架建立稳，再逐步提高 Core 精度。
5. **不是只跑正常路径**：Trap、Timeout、Error、Hotplug、Degrade、Reset、Replay 与 Recovery 从第一天进入设计。
6. **不是只看性能**：正确性、隔离、可观测性、可恢复性和可验证性优先。

---

## 9. 当前架构判断

本阶段正式采用以下研究定位：

> ArchFW + RVSoC-Sim 将共同构建一个 RISC-V Firmware-Native Logical Partition Platform。
>
> 项目通过 BOOM、香山和 RISC-V 标准能力建立一个简化但可演进的 Core/SoC，通过 IBM POWER / PowerVM / LPAR 学习软硬件协同、资源契约、分区固件、服务分区、RAS 和迁移，通过 CECSIM 学习如何让模拟器与固件共同成为可执行架构与验证平台。

我们的目标不是证明它比主流方案更好，而是亲手回答：

> 如果 Partition 从芯片和第一条 Firmware 指令开始就是一等对象，一台服务器应该怎样被设计、验证、管理和恢复？

---

## 10. 当前参考资料

本文阶段性判断主要基于以下已收集资料：

- *Advanced virtualization capabilities of POWER5 systems*
- *Functional verification of the POWER5 microprocessor and POWER5 multiprocessor systems*
- *IBM POWER6 partition mobility: Moving virtual servers seamlessly between physical systems*
- *IBM POWER7 multicore server processor*
- *Advanced features in IBM POWER8 systems*
- *IBM POWER8 processor core microarchitecture*
- *Modeling and Estimation of LPAR Energy Consumption for IBM POWER9 Systems*
- *Advanced firmware verification using a code simulator for the IBM System z9*

后续应继续补充：

- POWER ISA virtualization facilities；
- PAPR / PowerVM platform contract；
- PHYP、PFW、RTAS、VIOS、HMC 的公开资料；
- POWER9/POWER10/POWER11 的 XIVE、Radix、Secure VM、RAS 与迁移资料；
- RISC-V H、AIA/IMSIC、IOMMU 与 RAS 相关规范；
- BOOM、香山的 Core、验证和仿真架构资料。
