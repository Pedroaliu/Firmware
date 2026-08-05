# Jixia Agent Entry

Before answering project-specific questions or modifying this repository:

1. inspect the default branch and recent commits;
2. read `PROJECT_CONTEXT.md`;
3. read `README.md`;
4. read `docs/JIXIA_ARCHITECTURE_V0.3.md`;
5. read `docs/JIXIA_PROJECT_SOURCES.md`;
6. inspect current source paths, namespaces, and build files;
7. locate relevant uploaded references by titles in the source manifest.

Do not rely on old conversation memory when repository state is available.

## Naming rule

- `Jixia` is the project/platform brand.
- Pangu, Mozi, Nuwa, Luban, Yuange, Bianque, Taiyi, Sunbin, Guigu, and Jingjie are implementation codenames.
- Source directories, public interfaces, functions, types, protocols, and schemas use semantic English names.
- Freestanding C++ implementation code uses nested `jixia::*` namespaces.
- Assembly and cross-language boundaries use minimal `jixia_` C ABI symbols.
- Do not create new source paths or public symbols from codenames.
- Do not reintroduce `ArchFW` as a current component name.

Repository state wins over remembered chat state unless the user explicitly says otherwise.
