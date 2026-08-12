# Jixia Developer Workflow

## Status

- **Type:** developer workflow and local tooling guide
- **Scope:** local environment preparation, build, QEMU run, log capture, GDB debug, and pre-commit quality checks
- **Primary scripts:** `scripts/setup-dev-env.sh`, `scripts/jixia.sh`, `scripts/pre-commit-check.sh`
- **Default build directory:** `build/clion-debug`

This document defines the normal local development entry points for Jixia.

The detailed workflow lives under `docs/` rather than expanding the top-level `README.md` into a tooling manual. The README remains the project/architecture entry point; this document is the canonical reference for developer setup and execution.

The current default build directory deliberately remains `build/clion-debug` because it is already shared by `CMakePresets.json`, CLion, and the existing milestone regression scripts. A future build-layout change should update those consumers together rather than creating parallel build conventions.

C/C++ formatting is defined by the repository-root `.clang-format`. See `docs/JIXIA_CODE_STYLE.md` for the formatting contract and incremental-adoption rule.

---

## 1. Quick start

For a new Debian/Ubuntu/Deepin/UOS development host:

```bash
git clone <repository-url>
cd Firmware

bash scripts/setup-dev-env.sh
bash scripts/jixia.sh run
```

The first command checks the host and installs missing development packages when the platform is supported. The second command configures/builds Jixia, boots it in QEMU, streams UART output, and stores the run evidence under the build tree.

For an existing machine, check without changing the host:

```bash
bash scripts/setup-dev-env.sh --check
```

---

## 2. Design goals

The developer tooling follows several rules.

### 2.1 Local state is inspected before packages are installed

`setup-dev-env.sh` checks commands first and installs only missing packages.

It does not download and execute arbitrary remote installation scripts. On supported APT-based distributions, package installation goes through the distribution package manager.

### 2.2 One normal developer entry point

Routine development should converge on:

```text
scripts/jixia.sh
```

instead of accumulating unrelated `build-*.sh`, `run-*.sh`, and ad-hoc QEMU command lines.

Milestone-specific test scripts remain separate because they are machine-checkable acceptance gates rather than interactive developer commands.

### 2.3 Reproducibility is part of debugging

Each generic QEMU run records the exact QEMU command and output logs so a future debugging session can reconstruct how the firmware was launched.

### 2.4 The command surface should remain backend-neutral where practical

Today `jixia.sh` drives QEMU. Long term the same developer vocabulary can grow toward simulator backends without changing the conceptual workflow:

```text
configure -> build -> run/debug -> collect evidence
```

### 2.5 Formatting and patch hygiene are repository contracts

New or modified C/C++ lines must satisfy the repository `.clang-format`. Untouched historical code is not mass-reformatted as part of unrelated architecture work.

Before a commit, the staged patch is checked for whitespace errors and clang-format compliance. GitHub Actions repeats those checks against the branch/main merge-base before building or running target tests.

---

## 3. Environment bootstrap

### Check only

```bash
bash scripts/setup-dev-env.sh --check
```

Exit status is non-zero when required commands are missing.

### Install missing dependencies

```bash
bash scripts/setup-dev-env.sh
```

The script prints the missing package list before invoking APT and asks for confirmation.

Non-interactive installation:

```bash
bash scripts/setup-dev-env.sh --yes
```

Skip debugger installation when only build/run support is required:

```bash
bash scripts/setup-dev-env.sh --without-gdb
```

### Required tool classes

The bootstrap currently checks:

```text
git
cmake >= 3.20
ninja
python3
clang-format
GNU timeout

qemu-system-riscv64

riscv64-unknown-elf-gcc
riscv64-unknown-elf-g++
riscv64-unknown-elf-objcopy
riscv64-unknown-elf-objdump
riscv64-unknown-elf-readelf

gdb-multiarch or riscv64-unknown-elf-gdb
```

Automatic package installation is currently intended for Debian-family hosts, including the primary Deepin development environment. Other distributions are checked but are not automatically modified when dependencies are missing.

---

## 4. Developer command overview

Show help:

```bash
bash scripts/jixia.sh help
```

Current commands:

```text
env         inspect resolved local tools and build directory
configure   configure the build tree
build       build jixia.elf and generated firmware artifacts
run         boot QEMU and collect logs
debug       boot QEMU halted and attach/serve GDB
clean       remove the selected Jixia build directory
```

---

## 5. Environment inspection

```bash
bash scripts/jixia.sh env
```

This prints the resolved paths for the compiler, binary utilities, QEMU, GDB, and the selected build directory.

Use this first when a local machine behaves differently from another development host.

---

## 6. Configure and build

### Configure

```bash
bash scripts/jixia.sh configure
```

Default output directory:

```text
build/clion-debug
```

### Build

```bash
bash scripts/jixia.sh build
```

The repository's CMake build generates the normal firmware inspection artifacts:

```text
build/clion-debug/jixia.elf
build/clion-debug/jixia.bin
build/clion-debug/jixia.map
build/clion-debug/jixia.dis
build/clion-debug/jixia.readelf
```

### Force reconfiguration

```bash
bash scripts/jixia.sh build --reconfigure
```

### Use another build directory

```bash
bash scripts/jixia.sh build --build-dir build/experiment
```

Custom build directories are useful for experiments, but the default should remain the shared project build path unless a test explicitly requires isolation.

---

## 7. Run under QEMU

Default run:

```bash
bash scripts/jixia.sh run
```

Current default platform contract:

```text
machine = virt
cpu     = rv64
memory  = 128M
smp     = 4
bios    = <build-dir>/jixia.bin
```

Change hart population:

```bash
bash scripts/jixia.sh run --smp 1
bash scripts/jixia.sh run --smp 2
bash scripts/jixia.sh run --smp 4
```

Change memory:

```bash
bash scripts/jixia.sh run --memory 256M
```

The default QEMU timeout is five seconds because the current firmware intentionally parks after its tests and does not perform a normal process exit. GNU `timeout` status 124 is therefore treated as an expected end to a generic run.

Run longer:

```bash
bash scripts/jixia.sh run --timeout 15
```

Disable the timeout and run until interrupted:

```bash
bash scripts/jixia.sh run --timeout 0
```

---

## 8. Run evidence and logs

Generic run evidence is written below:

```text
build/clion-debug/logs/
```

Example:

```text
build/clion-debug/logs/run-20260811-110000/
    serial.log
    qemu.log
    command.txt
```

`serial.log` records firmware UART output.

`qemu.log` records QEMU diagnostic/stderr output.

`command.txt` records the exact launch command with all effective QEMU arguments. This file is intended to become part of bug reproduction evidence.

For quiet automation or when UART should not be streamed interactively:

```bash
bash scripts/jixia.sh run --no-console
```

---

## 9. Pass raw QEMU options

Everything after `--` is passed directly to QEMU.

Example: collect interrupt/guest-error diagnostics:

```bash
bash scripts/jixia.sh run --smp 4 -- -d int,guest_errors
```

This escape hatch avoids adding a new wrapper option for every QEMU experiment while keeping the common platform arguments under the Jixia developer command.

When an extra QEMU option becomes a stable project requirement, it should move into an explicit `jixia.sh` option or a milestone test rather than living forever as an undocumented command fragment.

---

## 10. GDB debugging

Start QEMU halted and attach an available RISC-V capable debugger:

```bash
bash scripts/jixia.sh debug
```

The command:

```text
builds Jixia
    -> launches QEMU with -S
    -> opens a GDB server on TCP 1234
    -> loads jixia.elf symbols
    -> attaches GDB
```

A normal interactive session can then use commands such as:

```gdb
break _start
continue
```

or:

```gdb
break jixia_microkernel_boot_main
continue
```

### Stop automatically at a symbol

```bash
bash scripts/jixia.sh debug \
    --break jixia_microkernel_boot_main
```

The wrapper sets a temporary breakpoint and continues to that symbol.

### Multi-hart debug

```bash
bash scripts/jixia.sh debug --smp 4
```

This is the normal foundation for privilege-transition, trap, per-hart, interrupt, and future scheduler debugging.

### Start only the QEMU debug server

For CLion, VS Code, another terminal, or a custom debugger frontend:

```bash
bash scripts/jixia.sh debug --server-only
```

Change the GDB TCP port when needed:

```bash
bash scripts/jixia.sh debug --server-only --gdb-port 1235
```

The command prints an attach command and keeps QEMU waiting until the developer stops it.

Debug runs also receive their own evidence directory under:

```text
build/clion-debug/logs/debug-<timestamp>/
```

---

## 11. Formatting and pre-commit checks

The repository-root `.clang-format` is the canonical C/C++ style. CLion should be configured to honor the project file rather than an IDE-only personal style.

Jixia currently uses attached braces for new/modified C/C++ code:

```cpp
bool ready() {
    if (condition) {
        return true;
    }

    return false;
}
```

Assembly (`.S`) is intentionally excluded from automatic formatting enforcement.

### Check working-tree C/C++ changes

```bash
bash scripts/check-format.sh
```

### Fix working-tree changed lines

```bash
bash scripts/check-format.sh --fix
```

### Check the staged commit

```bash
git add <files>
bash scripts/pre-commit-check.sh
```

The pre-commit script runs:

```text
git diff --cached --check
scripts/check-format.sh --cached
```

If staged formatting needs repair:

```bash
bash scripts/check-format.sh --cached --fix
git add <reformatted-files>
bash scripts/pre-commit-check.sh
```

GitHub CI runs the equivalent patch-hygiene and changed-line formatting gate against the merge-base with `main` before target build/test execution.

Detailed style policy: `docs/JIXIA_CODE_STYLE.md`.

---

## 12. Environment overrides

The generic developer command supports environment overrides for local experiments:

```text
JIXIA_BUILD_DIR
JIXIA_BUILD_TYPE
JIXIA_QEMU
JIXIA_CC
JIXIA_CXX
JIXIA_GDB_PORT
JIXIA_QEMU_SMP
JIXIA_QEMU_MEMORY
JIXIA_QEMU_TIMEOUT_SECONDS
```

Example:

```bash
JIXIA_QEMU=/opt/qemu/bin/qemu-system-riscv64 \
JIXIA_QEMU_TIMEOUT_SECONDS=20 \
bash scripts/jixia.sh run
```

These are local developer overrides. Stable project requirements belong in repository configuration rather than personal shell environment.

---

## 13. Relationship to CMake presets and IDEs

`CMakePresets.json` remains the canonical IDE/manual debug preset and currently uses:

```text
jixia-rv64-debug
build/clion-debug
```

The developer wrapper intentionally shares that output directory so:

```text
CMake preset
CLion
jixia.sh
milestone tests
```

can inspect the same fresh firmware artifacts instead of accidentally testing a stale second build tree.

The scripts do not replace CMake. They provide a consistent human-facing workflow around the existing CMake build.

---

## 14. Relationship to milestone tests

`scripts/jixia.sh run` is a generic interactive/reproducible execution command.

Milestone tests such as:

```text
scripts/test-kernel-print.sh
scripts/test-timer-interrupt.sh
scripts/test-m00-05-population.sh
scripts/test-m00-06-02-supervisor-transition.sh
scripts/test-m00-06-03-supervisor-transition.sh
```

remain the acceptance source of truth for their milestones.

Do not replace a machine-checkable milestone gate with a successful interactive `jixia.sh run`.

Long term, common launch/build code may be factored so generic developer commands and acceptance tests share implementation without sharing pass/fail policy.

---

## 15. Expected future extensions

Possible later commands include:

```text
jixia.sh test
jixia.sh inspect
jixia.sh disasm
jixia.sh trace
jixia.sh fault
```

After Jingjie becomes an executable backend, the command model may grow toward:

```text
jixia.sh run --backend qemu
jixia.sh run --backend jingjie
```

This is intentionally future work. The current script should stay small enough to remain understandable and dependable.

---

## 16. Troubleshooting order

When a new developer cannot build or boot Jixia, use this order:

```text
1. bash scripts/setup-dev-env.sh --check
2. bash scripts/jixia.sh env
3. bash scripts/check-format.sh when a style gate fails
4. bash scripts/jixia.sh build --reconfigure
5. inspect build/clion-debug/jixia.elf and jixia.bin
6. bash scripts/jixia.sh run --smp 1
7. inspect the generated serial.log, qemu.log, and command.txt
8. use bash scripts/jixia.sh debug when register/CSR evidence is required
9. run the relevant milestone acceptance script
```

The project debugging rule remains: prefer observable evidence—build output, disassembly, CSR/register state, QEMU logs, structured tests, and GDB—over guessing.
