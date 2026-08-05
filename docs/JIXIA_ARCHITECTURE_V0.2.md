# Historical Jixia Architecture v0.2

This file is retained so existing links and the architecture history remain traceable.

The canonical architecture is now:

- [`JIXIA_ARCHITECTURE_V0.3.md`](JIXIA_ARCHITECTURE_V0.3.md)

v0.3 preserves the firmware-native LPAR, CECSIM-style co-design, RAS, security, and confidential-computing direction while establishing a cleaner naming rule:

- **Jixia** remains the project/platform brand;
- Chinese cultural names remain implementation codenames;
- source directories and public interfaces use English technical meaning;
- freestanding implementation code uses `jixia::*` C++ namespaces;
- assembly and cross-language boundaries use minimal `jixia_` C ABI symbols.

The complete v0.2 content remains available in Git history at commit:

- `6c6769adb8f1aa9c6e1b6f4afb9d3800b5d22433`
