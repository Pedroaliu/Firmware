# Jixia Cockpit Vision

Date: 2026-08-13
Status: Idea record / future design discussion

## Why this exists

Jixia is a 2026-born server architecture. Its human interface should not default to a 20-year-old blue BIOS Setup screen plus SOL/KVM as the primary operational model.

The traditional BIOS Setup and raw SOL/KVM paths should remain as rescue/fallback mechanisms, but the normal experience should be a modern, machine-model-driven **Jixia Cockpit**.

## Core idea

Jixia Cockpit is not just a prettier BIOS Setup. It is the interactive frontend to the same machine model and service APIs used by Host firmware, Management Complex, BMC, automation and RAS tooling.

The underlying architecture should expose one authoritative data/control model with multiple frontends:

- browser Cockpit
- CLI (`jixctl`)
- local rescue TUI/UART
- Redfish/automation APIs
- manufacturing/service tools
- BMC remote access

Do not maintain separate configuration logic for BIOS Setup, web UI and CLI.

## Proposed primary views

### System topology

Interactive topology/digital-twin view of:

- sockets / harts / clusters
- cache / NoC
- DDR channels
- PCIe hierarchy
- NIC / NVMe / GPU / accelerators
- M-C and SBE status
- power / thermal domains

A user should be able to click an IP or topology node and inspect its state, telemetry, RAS records, FIR/FFDC and configuration.

### Boot Timeline

Boot should be represented as a timestamped timeline across SBE -> M-C/Jixia-S -> Host/Jixia-H -> UEFI/OS.

Support:

- stage timing
- boot-to-boot comparison
- regression detection
- failing-stage identification
- link to related FFDC/RAS records

### RAS / serviceability

Do not reduce RAS to a flat SEL list.

Show:

- first failure
- causal/correlation graph
- secondary failures
- FIR / FFDC source
- capture generation
- whether data came from M-C live dump, BMC break-glass takeover or post-reset retained state
- recovery action and result

### Transactional firmware configuration

Configuration changes should be represented as transactions rather than isolated menu options.

Show before applying:

- old -> new values
- affected devices/domains
- reboot requirement
- retraining/re-enumeration impact
- security/RAS implications

Support apply-next-reboot, apply-and-reboot and discard.

### Unified console

Keep SOL-like reliability but multiplex it by source:

- SBE
- Jixia-S kernel
- M-C services
- RAS
- Jixia-H
- UEFI
- OS

Allow filtering by source rather than putting every firmware component on one undifferentiated serial stream.

### Service shell

Provide a modern remote/local shell backed by the same service APIs, e.g. conceptually:

- `jixctl topology show`
- `jixctl boot timeline`
- `jixctl ras fir show --failed`
- `jixctl ras ffdc collect ...`
- `jixctl pcie show ...`
- `jixctl power show ...`
- `jixctl config diff`

### Time-machine / history

Retain and navigate:

- recent boots
- fatal events
- configuration changes
- firmware updates
- topology changes

Enable diagnostic replay of firmware/RAS/power/config events without requiring full instruction replay.

## Remote/local access ideas

Traditional KVM and SOL remain useful fallbacks, but should not be the normal firmware-management UX.

Potential modern access paths:

- browser-native Cockpit through BMC
- secure CLI/API
- optional USB-C local service port exposing a service network/console/virtual-media endpoint
- KVM only when actual framebuffer/OS GUI access is needed
- low-level UART/debug header retained for bring-up and rescue

## AI role

AI may explain and correlate, but should not silently exercise privileged control.

Useful examples:

- explain why the last boot was slower
- correlate first failure and secondary RAS events
- summarize FFDC
- point to the most relevant topology node / trace / FIR

Privileged operations such as reset, register writes, debug unlock or configuration changes must still go through explicit authorization, audit, capabilities/policy and user confirmation.

## Design principle

The valuable part is not the visual skin. The key is to build a **single machine model + service API** underneath it.

SBE contributes bootstrap state; M-C contributes SoC control, telemetry, RAS and serviceability; Jixia-H contributes Host state; BMC securely exposes the resulting platform interface remotely.

Traditional BIOS Setup becomes a rescue frontend rather than the architectural center of firmware management.

## Working name

**Jixia Cockpit**

Tomorrow/future discussion should continue from this record and explore the machine data model, API boundaries, security model, UI information architecture and what should be prototyped first in the QEMU platform.
