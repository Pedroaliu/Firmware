# Virtual Time and Migration Continuity

**Implementation codename:** Sunbin / 孙膑

This module defines virtual timers, timebase offsets, pause/resume semantics, migration monotonicity, and deterministic replay time.

Code uses semantic namespaces such as `jixia::virtualization::time`.

Guest-visible time must not move backward across dispatch, suspend, recovery, or migration.
