# Firmware Personality Framework

**Implementation codename:** Yuange / 元歌

This framework presents an authoritative virtual PlatformGraph as OS-facing machine personalities:

- UEFI + ACPI;
- SBI + Device Tree;
- U-Boot/FIT/extlinux compatibility;
- minimal test payload.

Code uses semantic namespaces such as `jixia::firmware_personality::uefi`, `jixia::firmware_personality::acpi`, and `jixia::firmware_personality::device_tree`.

A personality translates and presents state; it does not independently decide resource ownership.
