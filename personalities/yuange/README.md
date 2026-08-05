# Yuange / 元歌

Yuange is the firmware personality framework.

It presents one authoritative virtual PlatformGraph as different OS-facing machine models:

- UEFI + ACPI;
- SBI + Device Tree;
- U-Boot/FIT/extlinux compatibility;
- minimal test payload.

Yuange translates and presents state. It does not decide resource ownership independently.
