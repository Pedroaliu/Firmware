# Platform Model

**Implementation codename:** Nuwa / 女娲

This module owns the canonical PlatformGraph, topology construction and repair, and filtered platform views.

Code will use namespaces such as `jixia::platform`, `jixia::platform::graph`, and `jixia::platform::ownership`.

ACPI, Device Tree, inventory, management, and simulator topology are generated views of authoritative platform state rather than independent sources of truth.
