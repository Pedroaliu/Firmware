# RAS Diagnosis

**Implementation codename:** Bianque / 扁鹊

RAS diagnosis classifies errors, correlates cross-layer FFDC, identifies the source, resource owner, and affected scope, and recommends recovery actions.

Code uses semantic namespaces such as `jixia::ras`, `jixia::ras::diagnosis`, and `jixia::ras::ffdc`.

Diagnosis and recovery are separate responsibilities. Confidential-LPAR diagnosis must not require customer plaintext.
