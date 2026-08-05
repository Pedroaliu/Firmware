# Mozi / 墨子

Mozi is the Jixia host firmware microkernel.

Current executable code lives here. The near-term implementation is an RV64 M-mode foundation running on QEMU virt.

Mozi owns minimum trusted mechanisms:

- trap and interrupt entry;
- hart lifecycle;
- memory-domain and capability primitives;
- typed IPC and service lifecycle;
- ownership enforcement;
- secure launch and root RAS routing;
- measured/audited Guigu hooks.

Mozi does not own full device drivers, filesystems, UEFI/ACPI/DT compatibility, or large management policy.
