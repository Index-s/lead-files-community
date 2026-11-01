# Tree Cutting (Lumberjacking) — Design and Implementation Plan (v2)

Status: **plan only, nothing implemented yet.**
Scope: client (DX9 + DX12 backends, 32- and 64-bit portable) and server, plus data.
Companion asset: `docs/tree-cutting/assets/Han_Treechopper.GR2` (player chop animation,
extracted from `Han's_Cutting_Tree_Animation_GR2.rar`).

v2 changes versus v1 (decisions taken with the owner):

| Topic | v1 | v2 |
|---|---|---|
| Configuration storage | text files under `share/locale/<lang>/` | **three DB tables** (`tree_proto`, `tree_drop`, `tree_spawn`) + one SQL install file with defaults |
| Axe item | new top-level item type `ITEM_AXE` | **`ITEM_PICK` with subtype 1** — no enum changes anywhere, no proto-dump changes |
| Axe model / icon | new asset | **reuse Giant Axe** (`item/weapon/03080.gr2`, `icon/item/03080.tga`) |
| Tree models | new asset required | **reuse existing actors** (Christmas fir 20384, fossil tree) as shipped defaults; any `.gr2` NPC can be mapped later |
| Tree fall / hit feedback | needs `dead`/`damage` motions in the model | **procedural** fall and shake in client code — works with any model |
| Tree HP | `mob_proto.MAX_HP` | `tree_proto.hp`, applied at spawn — tune without touching protos |
| Felling | `CHARACTER::Dead()` with guards | **no `Dead()`**: FELL packet + delayed destroy → regen respawn; fewer hooks |
| Atlas | hidden | **shown with its own marker type and color** |
| Placement | edit `tree.txt` coordinates | **`/tree_add` in game** writes the DB row at the GM's position and spawns it |
| Drop owner | TOP (ALL optional) | **TOP recommended and default**; `ALL` kept as a per-kind option for event trees |
| Toggle | — | **`#define ENABLE_TREE_SYSTEM`** on both sides; every hook wrapped; all logic in new files |

---

## 1. Goal

- Trees are placed **per map at explicit positions**, respawn after a configurable time, have
  **health points**, and are visible on the atlas with their own marker.
- A player can cut a tree only while an **axe is equipped** and they are **next to the tree**.
  Each swing plays the delivered chop animation on the player.
- Each swing removes HP; at 0 the tree falls (procedural animation) and **drops are rolled
  from a configurable table** (item, amount or amount range, chance). Optional per-swing drops.
- **Server-authoritative** (HP, distance, axe, timing, drops).
- **Portable**: all logic in new files, minimal `#ifdef ENABLE_TREE_SYSTEM` hooks, no renumbering
  of existing enums or item types, no new item type, C++11 subset, pointer-size clean, DX9/DX12
  neutral. Install = compile with the define, import one SQL file, add 13 proto rows, copy a
  handful of client files.

---

## 2. What the codebase gives us (verified)

| Finding | Where | Consequence |
|---|---|---|
| Mining is the template: ore veins are `TYPE NPC` / `NOMOVE` mobs; click → quest → `CHARACTER::mining()`; timed event; `HEADER_GC_DIG_MOTION` (vid, target, count) → client pushes `NAME_DIG` *count* times. | `game/src/mining.cpp`, `char.cpp`, client `PythonNetworkStreamPhaseGame.cpp` | Same shape, but the server ticks **per swing** and sends HP each time. |
| Destroying a character decrements its regen count; the regen event respawns it after `time`. Veins use exactly this (`kill_ore_load_event`). | `char.cpp` Destroy, `regen.cpp` | Fell = delayed destroy; respawn is free. |
| Game cores run SQL directly (`DBManager::DirectQuery`) and world objects already live in the `player` DB with `map_index, x, y` (`object`, `object_proto`). | `char_change_empire.cpp`, `player.sql` | DB-backed tree tables follow an existing pattern. |
| The target packet carries `bHPPercent`; `SetTarget` fills it for any non-PC with `MaxHP > 0`; `uitarget.py` draws the gauge. | `char.cpp` ~L4930, `root/uitarget.py` | Tree HP bar = re-broadcast target packet per hit. |
| Pickaxe client logic: `IsHoldingPickAxe()` (vnum 29101–29110) blocks attack/skills and keeps `MODE_GENERAL`; server equips `ITEM_PICK` like a weapon; `ITEM_PICK` rows have **subtype 0** and subtype is otherwise unused. | `InstanceBase.cpp` L952/1100/1120/2881, `item.cpp`, `char_item.cpp`, `item_proto.txt` L3382 | **Axe = `ITEM_PICK` subtype 1.** All equip code is reused; only mining's `Pick_Check` needs a subtype 0 guard. |
| Giant Axe 3080–3089 exists (two-handed weapon) with model `item/weapon/03080.gr2` and icon `03080.tga`. | `item_list.txt` L772, `item_proto.txt` L794 | Lumber axe reuses model + icon. |
| Actor tree models present: `npc2/christmas_tree/christmas_tree.gr2` (+ `wait.msa`, mob 20384 folder `christmas_tree`), `npc/mineral/fossil_tree.gr2` (vein 20049; its `motlist` maps every motion to `wait`). `pack/Tree/*.spt` are SpeedTree terrain props, not actors. | `npclist.txt` L1386, `mineral/motlist.txt` | Ship defaults on these; procedural fall removes the need for `dead` motions. |
| Instance rotation API: `SetRotation(yaw, pitch, roll)`, `SetRotationQuaternion`, `SetAlphaValue/BlendAlphaValue`. | `EterLib/GrpObjectInstance.h` L41–44, `InstanceBase.cpp` L800 | Procedural fall and shake are implementable in `CInstanceBase::Update`. |
| Atlas markers are dispatched by a type byte in `RegisterAtlasMark` (`TYPE_NPC`, `TYPE_WARP`), fed by `HEADER_GC_NPC_POSITION`, which the server fills from `InsertNPCPosition`. | `PythonMiniMap.cpp` L703–728, `regen.cpp` ~L688 | Add `TYPE_TREE` with its own marker/color; server inserts trees with a dedicated type byte. |
| Feature toggles: server `common/service.h` (`__PET_SYSTEM__`), client `UserInterface/Locale_inc.h` (`ENABLE_*`), exported to Python in `PythonApplicationModule.cpp`. | — | `ENABLE_TREE_SYSTEM` goes in both and is exported as `app.ENABLE_TREE_SYSTEM`. |
| The chop GR2 is a standard 32-bit LE Granny file (same format as the 8 900 shipped), Oodle-compressed, one skeleton (male warrior per screenshot). `dig.*` exist only for the four `pc/` models. | `pack/PC/**/general/dig.*` | Register the chop motion per folder where the file exists; fall back to `dig.msa` elsewhere until retargets exist. |
| DX12 is a mirror backend under `renderer dx12`; meshes/textures/effects build DX12 twins below `GameLib`. | `EterLib/GrpDevice.h`, `GrpBackendDX12.h`, `PythonSystem.cpp` L460 | No backend code; DX9 + DX12 test matrix only. |
| Free ids verified: mob vnums 20400–20499, item vnums 29200–29299 and 50740–50799, packet ids CG 120 / GC 139. | `mob_proto.txt`, `item_proto.txt`, `packet.h` | Reservations below. |

---

## 3. Reservations and naming (all in one shared header)

`Lead-Shared-Source/common/tree_system.h` — included by server and client, everything under
`#ifdef ENABLE_TREE_SYSTEM`, so a port changes numbers in one place:

```c
#define TREE_SYSTEM_PICK_SUBTYPE_AXE     1        // ITEM_PICK subtype that marks an axe
#define TREE_SYSTEM_MOB_VNUM_BEGIN       20400    // tree kinds (client model mapping)
#define TREE_SYSTEM_MOB_VNUM_END         20499
#define TREE_SYSTEM_ATLAS_TYPE           0x7E     // type byte in HEADER_GC_NPC_POSITION
#define TREE_SYSTEM_MAX_DISTANCE         300      // server-side swing distance (game units)
#define TREE_SYSTEM_MIN_SHARE_PCT        20       // contributor threshold for DROP_MODE ALL
#define HEADER_CG_TREE_CUT               120
#define HEADER_GC_TREE_CUT               139
```

| Thing | Value |
|---|---|
| Default tree kinds (mob vnums) | 20400 "Fir" → folder `christmas_tree`; 20401 "Dead Tree" → `mineral/fossil_tree`; 20402–20499 free for custom models |
| Axe items | 29201–29210 (`Lumber Axe+0 … +9`), `ITEM_PICK`, subtype 1, model `item/weapon/03080.gr2`, icon `icon/item/03080.tga` |
| Default wood items | 50740 Log, 50741 Resin, 50742 Bark, 50743 Heartwood (type `ITEM_MATERIAL`, stackable) |
| Motion | `CRaceMotionData::NAME_TREE_CUT` (appended last), Python `chr.MOTION_TREE_CUT` |
| Server module | `game/src/tree_system.h/.cpp`, namespace `tree_system` |
| Client module | `UserInterface/TreeSystem.h/.cpp` (packet recv, procedural fall/shake, helpers) |
| DB tables | `player.tree_proto`, `player.tree_drop`, `player.tree_spawn` |
| SQL | `Lead-Database-Scripts/migrations/<date>_tree_system.sql` (also usable standalone on any serverfile) |

---

## 4. Data model (database)

### 4.1 Why DB rather than files (recommendation: DB)

| | Text files | DB tables |
|---|---|---|
| Editing | shell + text editor on the server, one copy per locale directory | Navicat/phpMyAdmin/web panel, one copy for all channels and locales |
| Live placement | hand-edit coordinates, reload | `/tree_add` writes the row and spawns immediately |
| Portability | depends on each serverfile's locale path layout | one SQL file, works on every serverfile with a `player` DB |
| Reload | `/reload` re-reads files | `/reload tree` re-queries |
| Consistency across channels | files must be synced | automatic |

The only argument for files is "Ymir did it that way". Everything the owner asked for
(low configuration effort, works on every serverfile) points to the DB.

### 4.2 `tree_proto` — tree kinds

```sql
CREATE TABLE IF NOT EXISTS `tree_proto` (
  `vnum`            INT UNSIGNED NOT NULL,                 -- mob vnum (model/name come from mob_proto)
  `name`            VARCHAR(32)  NOT NULL DEFAULT '',      -- admin label only
  `hp`              INT UNSIGNED NOT NULL DEFAULT 300,     -- applied at spawn (SetMaxHP/SetHP)
  `hardness_pct`    SMALLINT UNSIGNED NOT NULL DEFAULT 100,-- swing damage * 100 / hardness
  `axe_grade_min`   TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- minimum axe refine level (+0..+9)
  `level_min`       TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `swing_ms`        SMALLINT UNSIGNED NOT NULL DEFAULT 1500,
  `fall_ms`         SMALLINT UNSIGNED NOT NULL DEFAULT 2500,-- procedural fall duration
  `linger_ms`       SMALLINT UNSIGNED NOT NULL DEFAULT 3000,-- fallen tree stays before destroy
  `hp_reset_sec`    SMALLINT UNSIGNED NOT NULL DEFAULT 600, -- untouched -> HP back to max; 0 = never
  `drop_mode`       ENUM('TOP','ALL') NOT NULL DEFAULT 'TOP',
  `owner_sec`       TINYINT UNSIGNED NOT NULL DEFAULT 15,  -- ground item ownership
  `exp`             INT UNSIGNED NOT NULL DEFAULT 0,       -- optional exp on fell (0 = none)
  PRIMARY KEY (`vnum`)
);
```

### 4.3 `tree_drop` — drop lines

```sql
CREATE TABLE IF NOT EXISTS `tree_drop` (
  `id`        INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `tree_vnum` INT UNSIGNED NOT NULL,
  `when`      ENUM('FELL','HIT') NOT NULL DEFAULT 'FELL',
  `item_vnum` INT UNSIGNED NOT NULL,
  `count_min` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  `count_max` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  `pct`       DECIMAL(7,4) NOT NULL DEFAULT 100.0000,       -- 0.0001 .. 100
  `grade_min` TINYINT UNSIGNED NOT NULL DEFAULT 0,          -- line only active from this axe grade
  PRIMARY KEY (`id`), KEY (`tree_vnum`)
);
```

Every line is rolled independently against `number(1, 1 000 000)` with `pct × 10 000`
(same precision as the `drop` type of `mob_drop_item.txt`). `count` is uniform in
`[count_min, count_max]`. `HIT` lines roll on every swing for the swinging player; `FELL`
lines roll once when the tree falls. Items are created, placed on the ground around the tree
with ownership (`AddToGround` + `StartDestroyEvent` + `SetOwnership`, as `mining::OreDrop`),
and logged (`ItemLog`, `SendMoneyLog`).

### 4.4 `tree_spawn` — positions per map

```sql
CREATE TABLE IF NOT EXISTS `tree_spawn` (
  `id`          INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `map_index`   INT NOT NULL,
  `x`           INT NOT NULL,            -- absolute world coordinates (like `object`)
  `y`           INT NOT NULL,
  `dir`         TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- 0..7 (regen dir), 8 = random
  `tree_vnum`   INT UNSIGNED NOT NULL,
  `respawn_sec` INT UNSIGNED NOT NULL DEFAULT 300,
  `enabled`     TINYINT(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`), KEY (`map_index`)
);
```

Loading builds one `REGEN` record per row (`type REGEN_TYPE_MOB`, `count 1`,
`time = respawn_sec`, zero jitter) through a small new entry point in `regen.cpp`
(`regen_insert_tree(REGEN&)`, under the define) so respawn, counting and `/reload` behave
exactly like file regens. Spawned trees get `SetMaxHP/SetHP` from `tree_proto.hp` and are
registered on the atlas with `TREE_SYSTEM_ATLAS_TYPE`.

### 4.5 Defaults shipped in the SQL file

- 2 kinds: 20400 Fir (hp 300, swing 1500, fall 2500, TOP) and 20401 Dead Tree
  (hp 600, hardness 150, axe grade ≥ 3, level ≥ 30).
- Drops: Fir → Log 2–4 @100 %, Resin 1 @25 %, Heartwood 1 @1 %; Bark 1 @10 % per HIT.
  Dead Tree → Log 4–6 @100 %, Heartwood 1 @5 %.
- No spawns (placed in game with `/tree_add`). Optional sample rows for `metin2_map_a1`
  commented out.
- All statements `CREATE TABLE IF NOT EXISTS` / `INSERT IGNORE`, so the file is idempotent.

### 4.6 Proto rows (the only non-SQL data step)

Shipped as copy-paste snippets for **both** formats (txt rows for this repo, SQL `INSERT`s for
serverfiles that keep protos in the DB), in `docs/tree-cutting/data/`:

- `mob_proto`: 20400, 20401 — cloned from vein 20047 (`KING NPC MELEE NOMOVE`, `ON_CLICK 0`,
  exp/gold 0), `FOLDER` = `christmas_tree` / `fossil_tree`. `MAX_HP` is irrelevant (overridden).
- `item_proto`: 29201–29210 — cloned from picks 29101–29110 with **subtype 1**, `refined_vnum`
  chain, `value0/1` = min/max swing damage (8/12 … 30/40), `value2` practice points,
  `value3` upgrade %, `value4` fail vnum. Model/icon via `item_list.txt`: `03080.gr2` / `03080.tga`.
- `item_proto`: 50740–50743 — `ITEM_MATERIAL`, stack 200.
- Names in `mob_names*.txt` / `item_names*.txt` (or DB `locale_name`).
- Client: `npclist.txt` lines `20400 christmas_tree` and `20401 fossil_tree mineral`, then
  regenerate the client `mob_proto`/`item_proto` dumps (unchanged tooling; no enum changes).

---

## 5. Server design

### 5.1 Module `tree_system.h/.cpp` (new files)

```
namespace tree_system
{
	bool  Initialize();                 // boot: load 3 tables; false aborts boot with a clear error
	bool  Reload();                     // /reload tree: swap tables atomically, re-apply hp to idle trees
	bool  IsTree(DWORD mobVnum);
	bool  IsAxe(const CItem* item);     // ITEM_PICK && subtype == TREE_SYSTEM_PICK_SUBTYPE_AXE
	void  SpawnAll(long mapIndex);      // called from SECTREE_MANAGER::Build after stone.txt
	void  OnTreeSpawned(LPCHARACTER tree);   // apply hp, atlas registration
	bool  Start(LPCHARACTER cutter, LPCHARACTER tree);
	void  Cancel(LPCHARACTER cutter, bool notify);
	void  OnTreeDestroyed(LPCHARACTER tree); // free per-tree state, cancel its cutters
	int   RefineAxe(LPCHARACTER ch, LPITEM axe);     // mirrors mining::RealRefinePick
	void  PracticeAxe(LPCHARACTER ch, CItem& axe);   // mirrors mining::PracticePick
	// GM
	bool  GMAdd(LPCHARACTER gm, DWORD treeVnum, int dir, int respawnSec);  // INSERT + spawn
	bool  GMRemove(LPCHARACTER gm, LPCHARACTER tree);                      // DELETE + destroy
	void  GMInfo(LPCHARACTER gm, LPCHARACTER tree);
}
```

State: per cutter `LPEVENT m_pkTreeCutEvent` + `DWORD m_dwTreeCutTargetVID` on `CHARACTER`
(two members, under the define); per tree a map keyed by VID inside the module
(contributors `pid → damage`, `lastHit`, `hpResetEvent`, `felled` flag, `removeEvent`).
Event payloads carry `pid`/`vid` only.

### 5.2 Flow

```
client                               server
  |-- CG_ON_CLICK(tree vid) ---------->| CHARACTER::OnClick: quest Click() first, then
  |                                    |   if tree_system::IsTree(race) -> Start(cutter, tree)
  |<-- GC_TREE_CUT START --------------| PacketAround; swing event in swing_ms
  |<-- GC_TREE_CUT HIT(hp%) -----------| validate, damage, SetHP, BroadcastTargetPacket,
  |<-- GC_TARGET(hp%) -----------------| HIT drops, PracticeAxe, reschedule
  |         ... repeat ...             |
  |<-- GC_TREE_CUT FELL(dir) ----------| hp<=0: felled=true, FELL drops, exp, cancel all cutters,
  |                                    |   remove event in fall_ms + linger_ms -> M2_DESTROY_CHARACTER
  |<-- GC_CHARACTER_DEL ---------------| regen count--, regen_event respawns after respawn_sec
  |-- CG_TREE_CUT STOP --------------->| explicit cancel (ESC / move key)
```

### 5.3 Validation (start and before every swing)

1. Tree exists, same map, `IsTree`, not felled, HP > 0.
2. Cutter not dead/stunned/mounted (`GetMountVnum`, `IsRiding`), no shop/exchange/safebox/cube
   open, `GetLevel() >= level_min`.
3. `GetWear(WEAR_WEAPON)` is an axe (`IsAxe`), `GetRefineLevel() >= axe_grade_min`.
4. `DISTANCE_APPROX <= TREE_SYSTEM_MAX_DISTANCE` (client walks to 150 before clicking).
5. Not cutting another tree; a second click on the same tree cancels (mining behaviour).

Failures send one `LC_TEXT` message + `GC_TREE_CUT STOP`.

### 5.4 Damage, HP, felling, drops

- `damage = number(axe.value0, axe.value1) * 100 / hardness_pct`, min 1.
- `tree->SetHP(hp - damage)` (not `PointChange`, which carries battle side effects), then
  `BroadcastTargetPacket()`; contributor map and `lastHit` updated; HP-reset event re-armed.
- Felling does **not** call `Dead()`; the tree stays a live but `felled` character for
  `fall_ms + linger_ms` (clicks rejected), then is destroyed. This avoids every exp/quest/PK
  side effect and needs no hooks in `Dead()`.
- `drop_mode TOP`: contributor with most damage owns FELL drops (ties → feller). `ALL`: each
  contributor with ≥ `TREE_SYSTEM_MIN_SHARE_PCT` gets an independent roll.
  **Recommendation: TOP.** It matches how mob loot ownership already works (most damage), it
  cannot be multiplied by parking alts around a tree, and drop rates stay exactly what the
  table says. `ALL` stays available per kind for event trees where sharing is the point.
- Optional `exp` on fell goes to the owner(s).

### 5.5 Cancel hooks (each one line, under the define)

| Trigger | Hook |
|---|---|
| Player moves | `CHARACTER::OnMove` (next to `mining_cancel()`) |
| Dies / stunned / warps / logs out | `Dead`, `Stun`, `WarpSet`, `Destroy` → `tree_system::Cancel` / `event_cancel` |
| Axe unequipped or swapped | `CHARACTER::UnequipItem` / `EquipItem` when `IsAxe(item)` |
| Mounts | `MountHorse` / `StartRiding` |
| Tree felled, destroyed or out of range | swing re-validation |
| Explicit stop | `HEADER_CG_TREE_CUT` |

### 5.6 Hook list in existing server files (the whole porting surface)

| File | Anchor to search | Insert |
|---|---|---|
| `common/service.h` | end | `#define ENABLE_TREE_SYSTEM` |
| `common/tree_system.h` | new | constants + packed packet structs |
| `char.h` | `// MINING` block | 2 members, `tree_cut_*` declarations |
| `char.cpp` | ctor/`Destroy` next to `m_pkMiningEvent`; `OnClick` after `quest::CQuestManager::instance().Click(...)`; `OnMove` next to `mining_cancel()` | init/cancel, start hook, cancel |
| `char_item.cpp` | `UnequipItem`/`EquipItem` | cancel when `IsAxe` |
| `char_battle.cpp` | `Dead(` top, `Stun` | cancel |
| `mining.cpp` | `Pick_Check`, `CHARACTER::mining` pick check | `&& item.GetSubType() == 0` |
| `regen.cpp` | after `regen_load` | `regen_insert_tree(REGEN&)`; in `InsertNPCPosition` branch use `TREE_SYSTEM_ATLAS_TYPE` for trees |
| `sectree_manager.cpp` | after `stone.txt` regen_load | `tree_system::SpawnAll(index)` |
| `char_manager.cpp` | `DestroyCharacter` | `tree_system::OnTreeDestroyed` |
| `main.cpp` | after drop tables load | `tree_system::Initialize()` |
| `packet_info.cpp` | `HEADER_CG_FISHING` line | register `HEADER_CG_TREE_CUT` |
| `input_main.cpp` | `case HEADER_CG_FISHING` | dispatch to `tree_system::Cancel` |
| `cmd.cpp` / `cmd_gm.cpp` | GM command table | `/tree_add`, `/tree_del`, `/tree_info`, `/tree_fell`, `/reload tree`, `/axe_max` |
| `questlua_pc.cpp` (optional) | `pc_mining` | `pc.cut_tree()`, `pc.refine_axe(cell)` |
| `Makefile` / `.vcxproj` | sources | `tree_system.cpp` |

Everything else is inside `tree_system.cpp`.

---

## 6. Client design

### 6.1 Shared packets (`common/tree_system.h`, inside the pack(1) region)

```c
enum { TREE_CUT_START = 0, TREE_CUT_HIT = 1, TREE_CUT_STOP = 2, TREE_CUT_FELL = 3 };
typedef struct { BYTE header; DWORD vid; DWORD target_vid; BYTE subheader; BYTE hp_pct; BYTE fall_dir; } TPacketGCTreeCut;  // 12 bytes
typedef struct { BYTE header; BYTE subheader; DWORD target_vid; } TPacketCGTreeCut;                                        // 6 bytes
```

`static_assert` on both sizes; both header maps register them (`packet_info.cpp`,
`CNetworkPacketHeaderMap`). `fall_dir` is the yaw (0–255 → 0–360°) away from the feller.

### 6.2 New file `TreeSystem.h/.cpp`

- `RecvTreeCutPacket()` dispatch.
- `CTreeFallController`: per tree instance a small state machine driven from
  `CInstanceBase::Update()`:
  - **shake** (on HIT): roll oscillation ±3° decaying over 350 ms, plus a reusable leaf/dust
    effect (`effect/etc/dig/dig.mse` as default; path configurable in Python).
  - **fall** (on FELL): pitch 0 → 85° toward `fall_dir` with ease-in (`t²`) over `fall_ms`,
    small bounce (−4°) at the end, impact effect + sound, then hold; alpha fade to 0 over the
    last 500 ms before the server deletes the instance. Implemented with
    `m_GraphicThingInstance.SetRotation(yaw, pitch, roll)`; pivot is the model origin, which
    for all shipped tree models is at the trunk base.
  - Works with **any** `.gr2`; if a model does have a `dead` motion a flag can prefer it.
- Helpers: `IsTreeVnum(vnum)` (range from the shared header), `IsAxeItem(itemVnum)` (type
  `ITEM_TYPE_PICK` and subtype 1 through `CItemManager`).

### 6.3 Hooks in existing client files

| File | Anchor | Insert |
|---|---|---|
| `Locale_inc.h` | end | `#define ENABLE_TREE_SYSTEM` |
| `PythonApplicationModule.cpp` | `ENABLE_COSTUME_SYSTEM` export | `app.ENABLE_TREE_SYSTEM` |
| `GameLib/RaceMotionData.h` | before `NAME_MAX_NUM` | `NAME_TREE_CUT` |
| `PythonCharacterModule.cpp` | `MOTION_DIG` | `MOTION_TREE_CUT`, `TREE_VNUM_BEGIN/END` |
| `PythonNetworkStream.h/.cpp`, `…PhaseGame.cpp` | `HEADER_GC_DIG_MOTION` lines | size map, `case`, `RecvTreeCutPacket`, `SendTreeCutStopPacket` |
| `PacketDebugRegGen.h` | dig motion entries | both packets |
| `InstanceBase.h/.cpp` | `IsHoldingPickAxe` and its 3 uses (`CanUseSkill`, `CanAttack`, motion mode) | `IsHoldingAxe()` + `IsTree()`; call `TreeSystem` update from `Update()` |
| `InstanceBaseBattle.cpp` ~L304, `InstanceBaseEffect.cpp` ~L302 | `IsResource()` | `|| IsTree()` (distance 150, no target effect) |
| `PythonPlayer.cpp` `__SendClickActorPacket` | pickaxe-on-horse check | tree + no axe / mounted → `OnCannotTreeCut(reason)` |
| `PythonMiniMap.h/.cpp` | `TYPE_WARP` enum + `RegisterAtlasMark` switch + render/`GetAtlasInfo` | `TYPE_TREE`: own vector, own marker image (`ui/minimap/tree_mark.sub`, or the NPC mark tinted via `SetDiffuseColor`), own text color (green) |
| `root/playersettingmodule.py` `SetGeneralMotions` | `dig.msa` line | register `tree_cut.msa` if the file exists in that folder else `dig.msa` for `MOTION_TREE_CUT` |
| `root/game.py` | `OnCannotMining` | `OnCannotTreeCut`, ESC → `net.SendTreeCutStop()` |
| `root/localeInfo.py`, `locale/<lang>/locale_game.txt` | — | 3 strings |
| `root/npclist.txt` | — | 2 lines |
| `root/uitooltip.py` (optional) | pick tooltip | label "Lumber axe" for subtype 1 |

All C++ hooks wrapped in `#ifdef ENABLE_TREE_SYSTEM`; Python hooks guarded by
`if app.ENABLE_TREE_SYSTEM:`.

### 6.4 Assets to copy into the client pack

| Asset | Source | Notes |
|---|---|---|
| `pc/<race>/general/tree_cut.gr2` + `tree_cut.msa` | `Han_Treechopper.GR2` | start with the warrior folder (confirmed rig); test the other 7; where it does not fit, the Python fallback uses `dig.msa` until a retarget exists |
| `ui/minimap/tree_mark.sub/.tga` | new 8×8 marker or tinted copy of the NPC mark | only new image |
| Tree models | already in pack (`christmas_tree`, `mineral/fossil_tree`) | none |
| Axe model + icon | already in pack (`03080`) | none |
| Sounds | reuse `sound/…` wood/metin hit; optional new | optional |

---

## 7. Rendering backends (DX9 / DX12)

No backend-specific code. The chop animation drives the existing skinned PC meshes through the
shared deform vertex buffers (`EterGrnLib/LODController.cpp`), which are `CGraphicVertexBuffer`s
with DX12 twins; tree models and effects use the standard loaders. The procedural fall only
changes the instance transform. Acceptance tests run twice (`renderer dx9`, `renderer dx12`).

---

## 8. Portability rules (every serverfile, 32- and 64-bit)

1. **No new enums or renumbering**: axe = existing `ITEM_PICK` + subtype; tree = existing NPC
   mob type; only `NAME_TREE_CUT` is appended at the *end* of the motion enum.
2. **All constants in one shared header**; packet ids and vnum ranges are the only things a
   port may need to change.
3. **C++11 subset** (no `std::optional`, structured bindings, `if constexpr`), compiles with
   clang/gcc on FreeBSD and VS2013+ on Windows.
4. Packed structs with `BYTE/WORD/DWORD/int32_t` only; `static_assert` sizes; no `long`,
   `size_t`, pointers or `time_t` on the wire or in DB rows (`TimeT64` for socket values).
5. Event payloads store `pid`/`vid`, never pointers (mirrors `mining_event_info`).
6. No dependency on this repo's x64 changes (`ItemCellType` etc.): use the types each
   serverfile defines.
7. SQL uses only MySQL/MariaDB 5.x-compatible syntax; the game core reads with `DirectQuery`
   on the `player` connection; nothing in the `db` core changes.
8. Python 2.7 compatible, guarded by `app.ENABLE_TREE_SYSTEM`.

---

## 9. Configuration effort after installation (what an admin actually does)

1. Compile server and client with `ENABLE_TREE_SYSTEM` defined.
2. Import `tree_system.sql` (tables + defaults).
3. Add the 2 mob rows, 10 axe rows, 4 wood rows and their names (snippets provided for txt
   and SQL protos), regenerate the client proto dumps, add 2 `npclist.txt` lines.
4. Copy `tree_cut.gr2/.msa` into the PC `general` folders and the atlas marker image; repack.
5. In game as GM: stand where a tree should be, `/tree_add 20400` (optional `dir`,
   `respawn_sec`). Repeat. `/tree_del` on a targeted tree removes it. `/reload tree` after DB
   edits. Done.

Give players an axe (`/item 29201`) or sell it at a shop; upgrade dialogue at NPC 20015 is
provided as `lumberjack.quest` (optional, mirrors `mining.quest`).

---

## 10. Implementation phases and verification

### Phase 1 — Shared + toggles
`common/service.h`, `common/tree_system.h`, `Locale_inc.h`, Python export. Check: both
builds compile with the define on and off (off = byte-identical behaviour).

### Phase 2 — Server
New `tree_system.cpp/h`; hooks per §5.6; SQL file with defaults; proto snippets.
Check without client: boot loads tables, `/tree_add` creates a DB row and a visible NPC on the
test core, `/tree_fell` triggers FELL drops + removal + respawn after `respawn_sec`,
`/reload tree` rejects a broken row and keeps the old tables, mining still refuses an axe.

### Phase 3 — Client
New `TreeSystem.cpp/h`; hooks per §6.3; Python; assets. Check: §11 matrix on DX9 and DX12.

### Phase 4 — Tune and document
Drop-rate check (fell 1 000 trees via a GM loop, compare counts with `pct`), tune `hp`,
`hardness_pct`, axe damage, `swing_ms` to the measured `.msa` duration. Write
`docs/tree-cutting/ADMIN.md` and the proto snippets in `docs/tree-cutting/data/`.

### Phase 5 (optional, asset work)
Retarget the chop animation to the other 7 rigs; a proper broadleaf/pine `.gr2` actor model;
stump actor (vnum 20450+) spawned at fell position; axe-specific model.

---

## 11. Test matrix

| # | Scenario | Expected |
|---|---|---|
| 1 | Click tree with no weapon / sword / pickaxe | client hint; server refuses; no motion |
| 2 | Click tree with axe in range | START, HIT every `swing_ms`, HP gauge drops, chop motion + shake each swing |
| 3 | Walk away / unequip axe / mount mid-cut | STOP within one swing |
| 4 | Two players, `TOP` | shared HP; top contributor owns FELL drops for `owner_sec` |
| 5 | Three players (50/30/20 %), `ALL` | three independent rolls |
| 6 | Axe grade / level too low | refused with message |
| 7 | Tree falls | FELL packet, procedural fall toward `fall_dir`, impact effect, items on ground, instance removed after `fall_ms + linger_ms`, respawn at same spot after `respawn_sec` |
| 8 | Tree at 1 HP untouched for `hp_reset_sec` | HP back to max |
| 9 | Logout / warp / death mid-cut | no dangling event (`/tree_info`, server log) |
| 10 | `/reload tree` with a bad row | reload refused, old data kept, error names table and id |
| 11 | Atlas on a map with trees | green tree markers with names, distinct from NPC/warp |
| 12 | Mining a vein while holding the lumber axe | refused ("need a pickaxe") |
| 13 | Scenarios 2, 7, 11 with `renderer dx12` | identical visuals, no DX12 errors |
| 14 | 200 swings with a 10 % HIT line | ≈ 20 ± statistical margin |
| 15 | Spam clicks / forged `CG_TREE_CUT` with foreign vid | one session; second click cancels; forged packet ignored |
| 16 | Build with `ENABLE_TREE_SYSTEM` undefined | compiles, no behaviour change |

---

## 12. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Chop GR2 fits only the male warrior rig | per-folder registration with `dig.msa` fallback; retarget in phase 5 |
| Shipped tree models look like a Christmas fir / fossil | `tree_proto.vnum → mob FOLDER` is the only thing to change when a better `.gr2` is sourced; the procedural fall needs no motions from it |
| Model origin not at trunk base on a custom model | fall pivot offset column (`pivot_z`) can be added to `tree_proto` later; defaults 0 |
| Packet id / vnum collisions on another serverfile | all in `common/tree_system.h` |
| Admin forgets client proto dumps | §9 checklist; symptom is a nameless tree, not a crash |
| Exploits (distance, no axe, timing) | every check repeated server-side each swing; HP and drops server-only |
