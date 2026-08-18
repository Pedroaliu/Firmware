# Jixia Project Source Manifest

## Purpose

This file records the repositories, documents, uploaded references, and source-of-truth order used by the Jixia project.

A new chat session should scan this manifest before claiming that a source is missing or before selecting a similarly named Git repository.

## 1. Source-of-truth order

When sources disagree, use this order unless the user explicitly overrides it:

1. Current code and build files in `Pedroaliu/Firmware` on `main`.
2. `PROJECT_CONTEXT.md` and the latest canonical `docs/JIXIA_*` architecture documents.
3. Current user instructions in the active project conversation.
4. Current state of related user repositories after scanning their default branches and recent commits.
5. Primary specifications, source repositories, books, and papers.
6. Historical `ARCHFW_*` and superseded Jixia design records.
7. Old chat summaries or model memory.

Repository state beats remembered chat state when the repository is newer.

## 2. Confirmed user Git repositories

### Canonical Jixia repository

- Repository: `Pedroaliu/Firmware`
- Branch: `main`
- Role: Jixia firmware platform, semantic `boot/`, `microkernel/`, `platform/model/`, `hypervisor/`, service, RAS, security, simulator-interface modules, target-side tests, and QEMU functional prototypes.
- Naming rule: Jixia is the project brand; Chinese cultural names are implementation codenames; source paths and C++ namespaces use technical English meaning.

### Related simulator and virtualization repositories

These repositories are related but are not interchangeable. Confirm the active one before writes.

- `Pedroaliu/RVSoC-Sim-v2` — newer RVSoC simulator work; default branch `main`.
- `Pedroaliu/archlab_rvsoc_sim` — earlier simulator repository; default branch `master`.
- `Pedroaliu/archlab-rvsoc-sim-t` — related timing/simulator experiment; default branch `main`.
- `Pedroaliu/archlab-virt` — separate KVM/virtualization study; default branch `main`.
- `Pedroaliu/Pltsim` — simulator reference/work repository; default branch `master`.
- `Pedroaliu/my-cs-arch-notes` — private architecture notes repository; default branch `main`.

### External implementation repositories

- `open-power/hostboot`, reference branch `release-fw1120`, pinned scheduler-study commit
  `22e3c409ab8b439d4c8eb31b644acb498032a487` — major source for Hostboot architecture,
  executive/task scheduling, isteps, targeting, RAS, chip operations, and distributed firmware
  cooperation.

Additional external repositories to pin when implementation reaches them:

- RISC-V ISA/privileged architecture specifications;
- RISC-V AIA and IMSIC specifications;
- RISC-V IOMMU specification and implementation references;
- BOOM;
- XiangShan;
- QEMU RISC-V virt;
- OpenSBI;
- seL4;
- Linux kernel/KVM;
- Petitboot/LinuxBoot;
- EDK II and U-Boot compatibility references.

Do not silently select a moving branch. Record the branch/tag/commit when code is adopted.

## 3. Canonical in-repository design records

Read in this order:

1. `PROJECT_CONTEXT.md`
2. `README.md`
3. `docs/JIXIA_ARCHITECTURE_V0.3.md`
4. `docs/JIXIA_PROJECT_SOURCES.md`
5. `docs/images/jixia-firmware-architecture.svg`
6. historical `docs/JIXIA_ARCHITECTURE_V0.2.md`
7. historical `docs/ARCHFW_LPAR_CECSIM_CODESIGN_DIRECTION_V0.1.md`
8. historical `docs/ARCHFW_ARCHITECTURE_V0.1.md`

The current naming policy is authoritative:

- cultural names are architecture and implementation codenames;
- directories, public symbols, types, schemas, protocols, and C++ namespaces use semantic English names;
- assembly and cross-language boundaries use minimal `jixia_` C ABI symbols.

## 4. IBM POWER / LPAR / RAS core references

The following materials were uploaded or discussed in the Firmware project conversations and should be located by title in the current conversation or File Library:

- **Advanced virtualization capabilities of POWER5 systems** — cooperative partitioning, micro-partitioning, PFW, VPA, entitlement, cede/prod/confer, and the physical-driver boundary between PHYP and VIOS.
- **POWER5 functional verification paper** — coverage-driven verification, resource exhaustion, SMT transition testing, architecture checkers, trace, and workaround hooks. Normalize the exact bibliography when the PDF is next opened.
- **Advanced features in IBM POWER8 systems** — simultaneous partitions per core, LPID in processor state, micro-partition prefetch, 24x7 counters, facility lazy restore, reference history, secure IPL, and the untrusted service-processor boundary.
- **POWER6 partition mobility material** — virtual partition memory, processor compatibility, dirty tracking, and virtual time continuity. Normalize the exact bibliography when reopened.
- **POWER7 architecture/RAS material** — LPID-tagged translation, core recovery, cache deletion, memory sparing, and partition-scoped fault handling. Normalize the exact bibliography when reopened.
- **POWER9 LPAR energy-estimation paper** — per-partition resource/energy accounting and its modeling limitations. Normalize the exact bibliography when reopened.

These are architecture references, not requirements to copy POWER instruction encodings or proprietary interfaces.

## 5. CECSIM and simulator co-design references

- **System z9 CECSIM firmware simulation paper** — full-system firmware execution before hardware, LPAR and I/O models, management scripts, SIMCALL, dynamic configuration, target tests, injection, regression, and coverage.
- POWER5 verification material — architectural invariants and coverage-driven verification.
- The Jixia rule derived from these sources: firmware and the full-system simulator must be co-designed; the simulator is an executable architecture specification, not a late compatibility wrapper.

## 6. Firmware security and trusted-computing references

Uploaded in the project:

- `Yao-Zimmer2020_Book_BuildingSecureFirmware.pdf`
  - Full title: **Building Secure Firmware: Armoring the Foundation of the Platform**
  - Roles: firmware resiliency, threat modeling, trust regions, virtual firmware, kernel hardening, device security, DMA/MSI, secure update, recovery, secure coding, fuzzing, and lifecycle maintenance.

- `Intel® Trusted Execution Technology for Server Platforms A Guide to More Secure Datacenters (William Futral, James Greene) (z-lib.org).pdf`
  - Full title: **Intel Trusted Execution Technology for Server Platforms: A Guide to More Secure Datacenters**
  - Roles: static/dynamic roots of trust, measured launch, launch control policy, sealing, remote attestation, trusted compute pools, and the distinction between trusted host measurement and future trusted guests.

- `Saltzer_Schroeder_75.pdf`
  - Role: protection design principles, economy of mechanism, complete mediation, least privilege, open design, separation of privilege, and least common mechanism.

- `Arbaugh_FS_97.pdf`
  - Role: secure boot / integrity chain background; normalize exact title when reopened.

- `Blackham_SCRH_11.pdf`
  - Role: secure systems/real-time or separation background; normalize exact title when reopened.

- `Ge_YCH_19.pdf`
  - Role: security architecture/reference; normalize exact title when reopened.

- `07b-sec.pdf`
  - Role: project security lecture/reference material.

The confidential-computing design must later add current primary references for RISC-V confidential computing, Arm CCA/RME, AMD SEV-SNP, and Intel TDX. Pin exact versions at the time of study.

## 7. Microkernel, library OS, and system structure references

Uploaded project-library files include:

- `Liedtke_93.pdf`
- `Liedtke_95.pdf`
- `p66-hartig.pdf`
- `singularity_eurosys2006.pdf`
- `libraryos_asplos11.pdf`
- `p124-wheeler.pdf`
- `p120-chen.pdf`
- `Fleming_Wallace_86.pdf`
- `p337-wulf.pdf`
- `zeng02ecosystem.pdf`
- `adya.pdf`
- `backtrack.pdf`
- `tinyos.pdf`
- `weiser94scheduling.pdf`

These should be located through File Library search by filename when needed. Do not infer an exact title or conclusion from the filename alone; open the file and normalize its bibliography before citing it in a design decision.

## 8. Course and lecture references currently available

Uploaded files include:

- `01a-intro.pdf`
- `01b-sel4.pdf`
- `02a-threadsevents.pdf`
- `02b-threadsevents.pdf`
- `03a-hw.pdf`
- `03b-vms.pdf`
- `04b-smp.pdf`
- `05a-rts.pdf`
- `05b-linux.pdf`
- `07a-perf.pdf`
- `07b-sec.pdf`
- `08a-multiproc-1.pdf`
- `08b-uk.pdf`
- `09a-multiproc-2.pdf`
- `10a-sel4.pdf`
- `10b-local.pdf`

Use these as supporting educational material, not automatically as canonical platform specifications.

## 9. Planned source areas by technical subsystem

### Boot and microkernel — codenames Pangu / Mozi

- RISC-V privileged specification
- OpenSBI startup/trap patterns
- Hostboot executive and task/service model
- seL4/L4/QNX/Zircon mechanism references
- secure firmware root-of-trust and recovery references

### Platform model — codename Nuwa

- Hostboot targeting
- CUE
- Device Tree and ACPI specifications
- hardware graph/schema systems
- CECSIM dynamic configuration

### Hypervisor and logical partitions — ArchHV / Jiuzhou

- POWER5–POWER8 LPAR papers
- RISC-V H extension
- RISC-V AIA/IMSIC
- RISC-V IOMMU
- KVM/Xen as comparison baselines

### Driver domain and firmware personalities — codenames Luban / Yuange

- LinuxBoot and Petitboot
- Linux PCI/NVMe/SCSI/RAID/network drivers
- VFIO/IOMMU patterns
- UEFI, ACPI, SBI, DT, U-Boot/FIT

### RAS diagnosis and recovery — codenames Bianque / Taiyi

- POWER RAS papers and Hostboot diagnostics
- server RAS specifications
- PCIe AER/DPC, memory RAS, CPU RAS
- NIST SP800-193 firmware resiliency

### Dynamic debug and simulator interface — codenames Guigu / Jingjie

- CECSIM
- POWER5 verification
- QEMU plugin/debug interfaces
- target-side test frameworks
- checkpoint/replay and deterministic simulation
- formal/invariant checking

### Confidential LPAR

- measured boot and attestation foundations
- Building Secure Firmware virtual firmware threat model
- Intel TXT as historical trusted-host foundation
- current confidential-VM primary specifications and threat models
- memory encryption/integrity, protected state, secure I/O, and secure migration research

## 10. Source hygiene rules

- Prefer primary papers, specifications, and source repositories.
- Record title, authors, year, stable URL/repository, branch/tag/commit, and local filename once normalized.
- Separate source-derived statements from project inference.
- Do not cite an uploaded filename as proof without opening the relevant pages.
- Do not treat OpenPOWER and commercial IBM PowerVM firmware stacks as the same architecture.
- Do not treat Intel TXT as equivalent to a modern confidential VM; it is mainly a trusted-launch and attestation foundation.
- When a source is historical, preserve its contribution but verify current standards before implementation.

## 11. Maintenance

Update this manifest whenever:

- a repository becomes the active implementation;
- an external dependency is pinned;
- a new paper materially changes the architecture;
- an uploaded source is normalized to a full bibliography;
- a source is deprecated or superseded;
- the canonical naming or namespace policy changes.
