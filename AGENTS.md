# Jixia Agent Entry

Before answering project-specific questions or modifying this repository:

1. inspect the default branch, active development branch, and recent commits;
2. read `PROJECT_CONTEXT.md`;
3. read `docs/JIXIA_PROGRESS.md` to identify the single ACTIVE milestone;
4. read `docs/JIXIA_SOLO_ROADMAP.md` for phase order and feature gates;
5. read `README.md`;
6. read `docs/JIXIA_ARCHITECTURE_V0.3.md`;
7. read `docs/JIXIA_PROJECT_SOURCES.md`;
8. inspect current source paths, namespaces, build files, and tests;
9. locate relevant uploaded references by titles in the source manifest.

Do not rely on old conversation memory when repository state is available.

## Solo-development rule

- The project currently has one human developer working with ChatGPT as a research, teaching, review, and debugging partner.
- There must be exactly one primary ACTIVE milestone.
- Do not start a new major subsystem until the ACTIVE milestone satisfies the Definition of Done and `docs/JIXIA_PROGRESS.md` is updated.
- LPAR, ArchHV implementation, Service LPAR, confidential LPAR runtime, and migration remain frozen until the Jingjie simulator gates in `docs/JIXIA_SOLO_ROADMAP.md` are satisfied.
- When a milestone is completed, record the commit, tests, result, lessons, limitations, and next ACTIVE milestone in the progress ledger.

## Naming rule

- `Jixia` is the project/platform brand.
- Pangu, Mozi, Nuwa, Luban, Yuange, Bianque, Taiyi, Sunbin, Guigu, and Jingjie are implementation codenames.
- Source directories, public interfaces, functions, types, protocols, and schemas use semantic English names.
- Freestanding C++ implementation code uses nested `jixia::*` namespaces.
- Assembly and cross-language boundaries use minimal `jixia_` C ABI symbols.
- Do not create new source paths or public symbols from codenames.
- Do not reintroduce `ArchFW` as a current component name.

Repository state wins over remembered chat state unless the user explicitly says otherwise.
