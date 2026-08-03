# ArchFW Firmware State Store and Migration Design V0.1

## 1. Problem

Traditional UEFI NVRAM design often binds persistent configuration to a C structure generated from VFR/IFR.

Example:

```
VFR
  |
  IFR
  |
  NV Variable Definition
  |
  C Struct Layout
  |
  NVRAM Blob
```

This creates upgrade problems:

- adding a new setup item changes struct layout
- field offsets change
- deleted fields are difficult to migrate
- BMC cannot safely restore old binary NV data into a new firmware layout

Persistent state must not depend on firmware memory layout.

---

## 2. Design Principle

> Persistent firmware state must be represented as versioned semantic objects, not binary structures.

Runtime structures may change between firmware versions, but persistent objects maintain stable identity.

---

## 3. Firmware State Store

ArchFW should introduce a Firmware State Store instead of a traditional NV variable store.

Architecture:

```
                Firmware Services

 Config Service
 Targeting
 RAS
 Update Manager

                     |
                     |

             Firmware State Store

                     |

          Transaction / Migration Layer

                     |

 -------------------------------------------
 PNOR   EEPROM   BMC Storage   NVMe Storage
```

---

## 4. Object Based Persistence

Avoid:

```
struct SetupData
{
    uint8_t boot_mode;
    uint8_t secure_boot;
};
```

Use semantic objects:

```json
{
  "object": "security.secure_boot",
  "schema": "security.v2",
  "version": 3,
  "value": true
}
```

Objects contain:

- Object ID
- Schema version
- Value
- Owner
- Permission
- Checksum
- Timestamp

---

## 5. Configuration Layering

Persistent configuration should follow:

```
Factory Default
        |
Board Configuration
        |
Project Configuration
        |
User Override
        |
Effective Configuration
```

CUE remains the source of default/platform configuration.

Runtime changes create override objects instead of modifying defaults.

---

## 6. Upgrade Migration

Firmware upgrade should not preserve raw NV layout.

Instead:

```
Old State Store
        |
        |
Schema Migration Engine
        |
        v
New State Store
```

Migration handles:

- renamed keys
- removed fields
- new defaults
- type changes
- compatibility rules

Similar to database schema migration.

---

## 7. Transaction Model

Configuration updates require atomic transactions.

Concept:

```
BEGIN

write object A
write object B
write object C

COMMIT
```

Commit updates the active state pointer.

Power loss before commit:

- keep old state

Power loss after commit:

- recover new state

---

## 8. Storage Reliability

Inspired by:

- UEFI FTW
- filesystem journaling
- copy-on-write databases
- A/B firmware update systems

ArchFW should support:

- journal
- checksum
- versioning
- rollback
- snapshot
- migration

---

## 9. Relationship with VFS

VFS/Resource Manager handles firmware resources:

```
module
firmware image
configuration resource
debug package
```

State Store handles persistent state:

```
configuration
health state
RAS history
update metadata
Target state
```

They are complementary.

---

## 10. Security

Configuration changes require capability checks.

Example:

GPU Agent may modify:

```
gpu.power.policy
```

but cannot modify:

```
security.root_key
```

Persistent state operations should include:

- authentication
- authorization
- audit history

---

## 11. Future ArchFW Components

```
Firmware State Service

 + Schema Manager
 + Migration Engine
 + Transaction Manager
 + Storage Backend
```

This provides database-like reliability for firmware configuration and lifecycle state.
