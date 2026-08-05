# Recovery

**Implementation codename:** Taiyi / 太乙

Recovery performs policy-approved retry, alternate path, service restart, vCPU/LPAR reset, page retirement, device reset or reassignment, topology degradation, checkpoint restore, failover, and migration.

Code uses semantic namespaces such as `jixia::ras::recovery`.

Recovery must remain executable from a state that does not depend on the failed target remaining trustworthy.
