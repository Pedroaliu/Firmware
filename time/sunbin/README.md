# Sunbin / 孙膑

Sunbin is the virtual time and migration-continuity subsystem.

It will define virtual timers, timebase offsets, pause/resume semantics, migration monotonicity, and deterministic replay time.

Guest-visible time must not move backward across dispatch, suspend, recovery, or migration.
