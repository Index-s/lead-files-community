# Tree Cutting (Lumberjacking) — Design and Implementation Plan

Status: **plan only, nothing implemented yet.**
Scope: client (x64, DX9 + DX12 backends) and server (FreeBSD amd64), plus data files.
Companion asset: `docs/tree-cutting/assets/Han_Treechopper.GR2` (the player chop
animation delivered with the request, extracted from `Han's_Cutting_Tree_Animation_GR2.rar`).

---

## 1. Goal

Add cuttable trees to the game:

- Trees are placed **per map at explicit positions** through a data file, respawn after a
  configurable time, and have **health points**.
- A player can cut a tree only while an **axe is equipped in the weapon slot** and they are
  **standing next to the tree**. Each swing plays the delivered chop animation on the player.
- Each swing removes HP; when HP reaches 0 the tree falls and **drops are rolled from a
  configurable table** (item, amount or amount range, chance). Optional small drops per swing.
- Everything is **server-authoritative** (HP, distance, axe check, timing, drops).
- Works on the DX9 and DX12 client backends, and the code is written pointer-size clean so it
  compiles unchanged on a 32-bit target should one be re-added (see §9).

Non-goals for v1 (listed as follow-ups in §12): a lumberjack skill with levels, stump models,
tree growth stages, wood crafting recipes.

---

## 2. What the codebase already gives us (findings that shaped the design)

| Finding | Where | Consequence |
|---|---|---|
| Mining is the exact analogue: ore veins are `TYPE NPC`, `AI_FLAG NOMOVE` mobs (vnums 20047–20059, 30301–30305) with `MAX_HP 120`; clicking one triggers `pc.mining()` from `mining.quest`; the server runs a timed event and sends `HEADER_GC_DIG_MOTION` (vid, target_vid, count) and the client pushes `NAME_DIG` *count* times. | `Lead-Server-Source/game/src/mining.cpp`, `char.cpp` (`CHARACTER::mining*`), `Lead-Client-Source/UserInterface/PythonNetworkStreamPhaseGame.cpp` (`RecvDigMotionPacket`) | Copy the structure, but make the server tick per swing instead of fire-and-forget, because trees have HP that must be updated per hit. |
| Veins self-destroy after 7–15 min (`kill_ore_load_event`) and come back through the regen system; destroying a character decrements its regen count and the regen event respawns it. | `char.cpp` SetProto / Destroy, `regen.cpp` | Tree respawn is free: fell = destroy character, regen `time` column = respawn delay. |
| Per-map spawn files already exist (`regen.txt`, `npc.txt`, `boss.txt`, `stone.txt`) in `share/locale/<lang>/map/<map>/` and are all loaded with `regen_load()`. | `sectree_manager.cpp` ~L761–771 | Add a `tree.txt` per map in the same format: zero new parsing code. |
| Drop tables are group text files with per-item `vnum count pct` lines. | `share/locale/<lang>/mob_drop_item.txt`, `item_manager_read_tables.cpp` | Reuse the group-file parser for `tree_drop.txt`. |
| `TPacketGCTarget` carries `bHPPercent`; `SetTarget` computes it for any non-PC with `MaxHP > 0`, and `uiTarget.py` shows the gauge. | `char.cpp` L4930+, `root/uitarget.py` `SetHP` | Tree HP bar works with the existing target packet; we only need to re-broadcast on each hit. |
| Pickaxe handling on the client is hard-coded by vnum range (`IsHoldingPickAxe` 29101–29110) and blocks `CanAttack`/`CanUseSkill`; motion mode stays `MODE_GENERAL`. | `InstanceBase.cpp` L952, L1100, L1120, L2881 | Axe gets the same treatment, but keyed on the item *type* (data-driven), not a vnum range. |
| `ITEM_PICK` is a top-level item type (24); equip rules for it live in `item.cpp` (`IsEquipable`, `SetPart`) and `char_item.cpp` (use = equip/unequip). Server db `ProtoReader.cpp`, the client `ItemData.h` and `Lead-Tools/DumpProto` all hold the string → enum table. | `Lead-Shared-Source/common/item_length.h` | New `ITEM_AXE` type must be added in all four places plus the proto dump regenerated for every client locale. |
| Motion ids live in `CRaceMotionData::EMotionName` and are exposed to Python as `chr.MOTION_*`; PC motions are registered per folder in `root/playersettingmodule.py` (`SetGeneralMotions`, `dig.msa`). | `GameLib/RaceMotionData.h`, `PythonCharacterModule.cpp` | Add `NAME_TREE_CUT` and register `tree_cut.msa` per PC folder. |
| The delivered animation is one Granny file (13.6 KB, Oodle compressed, standard 32-bit LE Granny magic, same format as the 8 900 `.gr2` files already in the client pack). The screenshot shows it on the **male warrior** skeleton. `dig.gr2/msa` exist only for the four `pc/` models, not the `pc2/` gender variants. | `Lead-Client/pack/PC/ymir work/pc/*/general/dig.*` | A GR2 animation is bound to one skeleton. It must be verified on all 8 player models and retargeted where the bone set differs (asset task, §10). |
| DX12 is a mirror backend selected by `renderer dx12` in the client config; meshes go through `CGraphicVertexBuffer`/`CGraphicIndexBuffer` (which own DX12 twins) and textures build DX12 twins in their loaders. Nothing in `EterGrnLib`/`GameLib` is backend specific. | `EterLib/GrpDevice.h`, `GrpBackendDX12.h`, `PythonSystem.cpp` L460 | No backend-specific work. The new animation, tree model and effects ride the existing path; DX12 is a *test matrix* item, not a code item. |
| The client solution contains only `Debug|x64` / `Release|x64`; the server is amd64 only (`README.md`). | `Lead-Client-Source.sln`, all `.vcxproj` | There is no 32-bit build to target today. The plan commits to 32/64-bit *portable* code (§9) so a Win32 configuration would compile without changes. |
| Free ids verified: mob vnums **20400–20499** unused, item vnums **29200–29299** unused, item vnums **50740–50799** unused, packet ids **CG 120**, **GC 139** unused, item type **35** is the next free slot after `ITEM_BELT` (34). | `mob_proto.txt`, `item_proto.txt`, `packet.h`, `item_length.h` | Reservations in §3. |

---

## 3. Reservations and naming

| Thing | Value |
|---|---|
| Tree mob vnums | 20400–20449 (trees), 20450–20499 reserved for stumps (v2) |
| Axe item vnums | 29201–29210 (`Axe+0` … `Axe+9`, refine chain like picks 29101–29110) |
| Wood / drop item vnums (examples) | 50740 Oak Log, 50741 Pine Log, 50742 Resin, 50743 Bark, 50744 Rare Heartwood |
| Item type | `ITEM_AXE = 35` (shared), `ITEM_TYPE_AXE` (client) |
| Packets | `HEADER_CG_TREE_CUT = 120`, `HEADER_GC_TREE_CUT = 139` |
| Motion | `CRaceMotionData::NAME_TREE_CUT`, Python `chr.MOTION_TREE_CUT` |
| Server module | `Lead-Server-Source/game/src/tree_cut.h/.cpp`, namespace `tree_cut` |
| Data files | `share/locale/<lang>/tree_proto.txt`, `share/locale/<lang>/tree_drop.txt`, `share/locale/<lang>/map/<map>/tree.txt` |
| Client assets | `pack/PC/ymir work/pc{,2}/<race>/general/tree_cut.{gr2,msa}`, `pack/NPC/ymir work/npc/tree_<kind>/…`, `pack/icon/item/2920x.tga` |

---

## 4. Data and configuration design

### 4.1 Tree prototypes (gameplay parameters) — `tree_proto.txt`

Tab separated, one row per tree vnum. Loaded at boot and by `/reload tree`.

```
VNUM	NAME	HARDNESS_PCT	AXE_GRADE_MIN	LEVEL_MIN	SWING_MS	HIT_GROUP	FELL_GROUP	FELL_DELAY_MS	HP_RESET_S	DROP_MODE	OWNER_S
20400	oak	100	0	1	1500	OakChips	OakLogs	4000	600	TOP	15
20401	pine	80	0	1	1500	-	PineLogs	4000	600	TOP	15
20402	ironwood	250	5	40	1800	-	IronwoodLogs	5000	300	ALL	15
```

| Column | Meaning |
|---|---|
| `HARDNESS_PCT` | Swing damage multiplier: `damage = roll(axe) * 100 / HARDNESS_PCT` (100 = normal, 250 = takes 2.5× longer). |
| `AXE_GRADE_MIN` | Minimum axe refine level (+0 … +9) required to damage this tree. |
| `LEVEL_MIN` | Minimum player level. |
| `SWING_MS` | Server time between swings. Should match the `.msa` `MotionDuration` of the chop animation (see §10) so the loop looks continuous. |
| `HIT_GROUP` | Drop group rolled **on every swing** (chips, bark) or `-`. |
| `FELL_GROUP` | Drop group rolled **when the tree falls**. |
| `FELL_DELAY_MS` | How long the fallen tree stays before the character is destroyed (length of the tree's `dead` motion). |
| `HP_RESET_S` | If nobody hits the tree for this many seconds, HP goes back to max (prevents "pre-cut" trees sitting at 1 HP). 0 = never. |
| `DROP_MODE` | `TOP` = the player who dealt the most damage owns the fell drops; `ALL` = every contributor with ≥ 20 % of the damage gets an independent roll. |
| `OWNER_S` | Ground-item ownership time for the dropper (as mining uses 15 s). |

**Tree HP** is *not* in this file: it is `MAX_HP` in `mob_proto` (so `GetMaxHP()`, the target
HP bar and `/reload` all keep working without special cases). Respawn time is in the per-map
spawn file (§4.3) because that is where the regen system reads it.

### 4.2 Drop tables — `tree_drop.txt`

Same group syntax as `mob_drop_item.txt`, so the existing group loader
(`CGroupTextParseTreeLoader`) is reused:

```
Group	OakLogs
{
	Type	fell			-- fell | hit
	Tree	20400
	1	50740	2~4	100.0		-- Oak Log: always 2 to 4
	2	50742	1	25.0		-- Resin: 25 %
	3	50744	1	0.5		-- Rare Heartwood: 0.5 %
}

Group	OakChips
{
	Type	hit
	Tree	20400
	1	50743	1	10.0		-- Bark: 10 % per swing
}
```

Rules:

- `count` is `n` or `n~m` (uniform random, inclusive).
- `pct` is a float with up to 4 decimals; stored as an integer ×10 000 and rolled against
  `number(1, 1 000 000)` exactly like the `drop` type of `mob_drop_item.txt`.
- Each line is rolled **independently** (several lines can all drop). A `MaxLines` optional key
  (`MaxLines 2`) limits how many lines may succeed per roll, for "pick one of" tables.
- A group may list several `Tree` vnums; a tree vnum may appear in one `fell` and one `hit`
  group.
- Items are created with `ITEM_MANAGER::CreateItem`, placed on the ground around the tree
  (`AddToGround` + `StartDestroyEvent` + `SetOwnership`) exactly as `mining::OreDrop` does, and
  logged through `LogManager::ItemLog` / `SendMoneyLog`.

### 4.3 Spawn positions per map — `map/<map>/tree.txt`

The regen format already used by `npc.txt` / `stone.txt`, loaded by the same `regen_load()`:

```
//type	cx	cy	sx	sy	z	dir	time	percent	count	vnum
m	612	540	0	0	0	3	5m	100	1	20400
m	618	552	0	0	0	1	5m	100	1	20400
m	700	490	4	4	0	0	10m	100	3	20401
```

- `cx cy` are map-local coordinates in the usual regen units; `sx sy` jitter (0 = exact);
  `dir` rotation 1–8 (lets the same model face different ways); `time` respawn delay after
  felling; `count` how many trees live in that spot at once.
- `percent 100`, `count 1`, `sx sy 0` gives the "exact position" case the request asks for.
- Loading is added in `SECTREE_MANAGER::Build` right after `stone.txt`.
- `regen_load` registers `TYPE NPC` mobs on the atlas (`InsertNPCPosition`). Trees must be
  skipped there (`tree_cut::IsTree(vnum)` check in the NPC branch), otherwise every tree becomes a
  named marker on the minimap.

### 4.4 Mob prototype rows (server `share/conf/mob_proto.txt`, client `mob_proto` per locale)

Modelled on the ore vein rows (`20047`):

```
20400	Oak	KING	NPC	MELEE	1		NOMOVE	0		STUN,SLOW,CURSE,TERROR	0	tree_oak	0	0 0 0 0	0	0	MAX_HP=300	REGEN_CYCLE=0	REGEN_PERCENT=0	gold 0 0	EXP 0	…
```

Key fields: `TYPE NPC`, `RANK KING` (prevents the "monster" level/colour logic), `AI_FLAG NOMOVE`,
`FOLDER tree_oak` (client model folder), `MAX_HP` = tree health, `REGEN_PERCENT 0`
(no natural HP regen), `EXP 0`, `GOLD 0 0`, `DROP_ITEM 0`, `ON_CLICK 0`.
Add names to `mob_names*.txt` for every language.

### 4.5 Axe item rows (`share/conf/item_proto.txt`, client `item_proto` per locale)

Modelled on the pickaxe rows (`29101`), type `ITEM_AXE`, `WEAR_WEAPON`, size 2, `LIMIT LEVEL n`:

| Value slot | Meaning (mirrors the pick where sensible) |
|---|---|
| `value0` | min damage per swing |
| `value1` | max damage per swing |
| `value2` | practice points needed for the next grade (socket0 holds current points, as picks do) |
| `value3` | upgrade success % at the lumberjack NPC |
| `value4` | vnum given on failed upgrade |
| `refined_vnum` | next grade vnum (29201 → 29202 …) |

The existing pick refine flow (`mining::RealRefinePick`, `mining.quest` dialogue at NPC 20015)
is generalised or duplicated for axes in phase 2 (`tree_cut::RealRefineAxe`, `pc.refine_axe`).

### 4.6 Locale strings

New `LC_TEXT` keys (added to every `locale_string*.txt`, English fallback is automatic):
"You need an axe to cut trees.", "You are too far from the tree.", "You cannot cut trees while
mounted.", "Your axe is not strong enough for this tree.", "You need level %d to cut this tree.",
"The tree fell!", "Cutting is finished.", "Your lumberjack points have increased! (%lld/%d)".
Client side: `localeInfo` keys for the client-only hints (§6.5).

### 4.7 Database

No schema change. All configuration is file based (consistent with regen and drop tables, and
reloadable without a DB round trip). Drops and axe upgrades are logged into the existing `log`
database tables through `LogManager`. A DB-backed spawn table was considered and rejected: it
would need new db-core packets and would diverge from how every other spawn is configured.

---

## 5. Server design

### 5.1 New module `tree_cut.h` / `tree_cut.cpp`

```
namespace tree_cut
{
	bool  LoadProto(const char* file);          // tree_proto.txt
	bool  LoadDrops(const char* file);          // tree_drop.txt
	bool  Reload();                             // both; used by /reload tree
	bool  IsTree(DWORD vnum);
	const TTreeProto* GetProto(DWORD vnum);

	bool  Start(LPCHARACTER cutter, LPCHARACTER tree);   // validation + first swing event
	void  Cancel(LPCHARACTER cutter, bool notify);      // used from all cancel hooks
	bool  IsCutting(LPCHARACTER cutter);

	// axe helpers (parallel to mining::*Pick*)
	bool  IsAxe(const CItem& item);
	int   RealRefineAxe(LPCHARACTER ch, LPITEM axe);
	void  PracticeAxe(LPCHARACTER ch, CItem& axe);
}
```

Internal state:

- **Per cutter** (`CHARACTER`): `LPEVENT m_pkTreeCutEvent`, `DWORD m_dwTreeCutTargetVID`.
  Event info holds `pid` + `target vid` only (never pointers), like `mining_event_info`.
- **Per tree** (map keyed by tree VID inside the module, cleaned on destroy):
  `std::map<DWORD /*pid*/, int /*damage*/> contributors`, `DWORD lastHitTime`,
  `LPEVENT hpResetEvent`.

### 5.2 Flow

```
client                          server
  |-- CG_ON_CLICK(tree vid) ------>|  CHARACTER::OnClick
  |                                |   quest Click() first (lets quests intercept)
  |                                |   if tree_cut::IsTree(race) -> tree_cut::Start(cutter, tree)
  |<-- GC_TREE_CUT START ----------|  (PacketAround: everyone nearby sees the stance)
  |                                |  event in SWING_MS
  |<-- GC_TREE_CUT HIT (hp%) ------|  swing: re-validate, damage, tree->SetHP, drops(hit),
  |<-- GC_TARGET (hp%) ------------|  BroadcastTargetPacket, PracticeAxe, reschedule
  |        ... repeat ...          |
  |<-- GC_TREE_CUT FELL -----------|  hp <= 0: fell drops, tree->Dead() (plays dead motion),
  |<-- GC_DEAD (tree) -------------|  dead_event destroys after FELL_DELAY_MS, regen respawns
  |                                |
  |-- CG_TREE_CUT STOP ----------->|  optional explicit cancel (ESC / click elsewhere)
  |<-- GC_TREE_CUT STOP -----------|
```

### 5.3 Validation (at start **and** before every swing)

1. `tree` still exists, same map index, `IsTree`, `!IsDead`, HP > 0.
2. Cutter: `!IsDead`, `!IsStun`, `!IsRiding/GetMountVnum`, no shop/exchange/safebox/cube open
   (same guard list as `OnClick`), `GetLevel() >= LEVEL_MIN`.
3. `GetWear(WEAR_WEAPON)` exists and `GetType() == ITEM_AXE`; `GetRefineLevel() >= AXE_GRADE_MIN`.
4. Distance: `DISTANCE_APPROX(dx, dy) <= TREE_CUT_MAX_DISTANCE` (300 units; the client walks to
   150 before it sends the click, so there is slack for lag). Checked every swing so walking
   away stops cutting even if `OnMove` misses.
5. Not already cutting another tree (a second click on the same tree = cancel, as mining does).

Failure sends one `LC_TEXT` reason and `GC_TREE_CUT STOP`.

### 5.4 Damage, HP, felling

- `damage = number(axe.value0, axe.value1) * 100 / HARDNESS_PCT`, minimum 1.
- `tree->PointChange(POINT_HP, -damage)` is **not** used (it carries battle side effects);
  use `tree->SetHP(max(0, hp - damage))` then `tree->BroadcastTargetPacket()`.
- Contribution map updated; `lastHitTime` set; HP reset event re-armed for `HP_RESET_S`.
- Fell: `Dead(cutter)` is reused so the client receives `HEADER_GC_DEAD` and plays the tree's
  `dead` motion and so `dead_event` + regen handle removal/respawn. Guards are added at the top of
  `CHARACTER::Dead` (and in `Reward`/`CreateDropItem` call sites) with `tree_cut::IsTree(GetRaceNum())`
  so a tree gives **no exp, no mob drop, no quest `kill` trigger, no PK/arena bookkeeping**. The
  dead-event delay for trees is `FELL_DELAY_MS`.
- Fell drops go to `TOP` contributor or `ALL` ≥ 20 % per `DROP_MODE`; `hit` drops go to the
  swinging player. Every created item is logged.

### 5.5 Cancel hooks (mirror of `mining_cancel()` call sites, plus a few more)

| Trigger | Where |
|---|---|
| Player moves | `CHARACTER::OnMove` (where `mining_cancel()` is called today) |
| Player dies / is stunned / warps / logs out | `CHARACTER::Dead`, `Stun`, `WarpSet`, `Destroy` (`event_cancel(&m_pkTreeCutEvent)`) |
| Axe unequipped or swapped | `CHARACTER::UnequipItem` / `EquipItem` for `ITEM_AXE` |
| Mounts a horse | `CHARACTER::MountHorse` / `StartRiding` |
| Tree destroyed or felled by someone else | swing re-validation (VID lookup fails / HP 0) |
| Explicit stop | `HEADER_CG_TREE_CUT` subtype `STOP` |
| Second click on the same tree | `tree_cut::Start` when already cutting |

### 5.6 Other server touch points

- `CHARACTER::SetProto`: nothing (trees do **not** get the vein self-destroy timer).
- `regen.cpp`: skip `InsertNPCPosition` for trees (§4.3).
- `sectree_manager.cpp`: load `tree.txt`.
- `main.cpp` boot: `tree_cut::LoadProto/LoadDrops` next to the drop-table loads; fail boot
  loudly on parse errors (same policy as `mob_drop_item.txt`).
- `cmd_gm.cpp`: `/reload tree`, `/tree_info` (target tree: HP, contributors), `/tree_fell`,
  `/tree_spawn <vnum> [dir]` (spawns at the GM's position for level design; prints the `tree.txt`
  line to copy), `/axe_max` (fills practice points, like `CHEAT_MAX_PICK`).
- `questlua_pc.cpp`: `pc.cut_tree()` (start on the current quest NPC), `pc.refine_axe(cell)`;
  `questlua_global.cpp`: `is_tree(vnum)`.
- `packet_info.cpp`: register `HEADER_CG_TREE_CUT` (size, name, `true` = allowed in game phase).
- `input_main.cpp`: dispatch `HEADER_CG_TREE_CUT` → `tree_cut::Cancel(ch, true)`.
- `item.cpp` / `char_item.cpp`: `ITEM_AXE` in `IsEquipable`, `SetPart(PART_WEAPON)` on equip,
  use-item = equip/unequip, and blocked while riding (same lines as `ITEM_PICK`).
- `db/src/ProtoReader.cpp`: `"ITEM_AXE"` appended to the type string table (index 35).

---

## 6. Client design

### 6.1 Shared headers (`Lead-Shared-Source`)

`packet.h` (inside the existing `#pragma pack(1)` region, fixed-width fields only):

```c
enum { TREE_CUT_START = 0, TREE_CUT_HIT = 1, TREE_CUT_STOP = 2, TREE_CUT_FELL = 3 };

typedef struct packet_gc_tree_cut {  BYTE header; DWORD vid; DWORD target_vid; BYTE subheader; BYTE hp_pct; } TPacketGCTreeCut;
typedef struct packet_cg_tree_cut {  BYTE header; BYTE subheader; DWORD target_vid; } TPacketCGTreeCut;
```

`common/item_length.h`: `ITEM_AXE` after `ITEM_BELT`. `length.h`: nothing.

### 6.2 Motion

- `GameLib/RaceMotionData.h`: `NAME_TREE_CUT` appended **after `NAME_JOY`, before
  `NAME_MAX_NUM`** so no existing index shifts.
- `UserInterface/PythonCharacterModule.cpp`: `MOTION_TREE_CUT` constant.
- `root/playersettingmodule.py` `SetGeneralMotions`: `RegisterCacheMotionData(mode, chr.MOTION_TREE_CUT, "tree_cut.msa")`
  right after the `dig.msa` line. Because `SetGeneralMotions` is called for every PC folder,
  the `.msa/.gr2` pair must exist in all 8 `general/` folders (`pc/` and `pc2/` × 4 races).

### 6.3 Instance logic (`UserInterface/InstanceBase*.cpp`)

- `IsTree()`: vnum in 20400–20499 (single constant pair, exposed to Python as
  `chr.TREE_VNUM_START/END`). Kept as a range like `IsResource()`; the client mob proto has no
  flag column to carry it.
- `IsHoldingAxe()`: look up `m_awPart[PART_WEAPON]` in `CItemManager` and test
  `GetType() == ITEM_TYPE_AXE` (data-driven, unlike the pickaxe range).
- Same guards as the pickaxe: `CanUseSkill()`, `CanAttack()` return false while holding an axe;
  `__GetMotionModeByWeapon` → `MODE_GENERAL` (or `MODE_HORSE` when mounted); the axe model is
  attached to the weapon bone through the normal `PART_WEAPON` path, so it is in hand during the
  chop animation as in the screenshot.
- `IsTree()` is added wherever `IsResource()` gates behaviour: clickable distance (150 instead of
  the resource 100, `InstanceBaseBattle.cpp` ~L304), target-effect suppression
  (`InstanceBaseEffect.cpp` ~L302), `IsAttackableInstance`/`IsEnemy` false, cursor = "talk/use".
- `CPythonPlayer::__SendClickActorPacket`: before sending the click, if target `IsTree()`:
  not holding axe → `game.OnCannotTreeCut("NEED_AXE")` (client hint only), mounted →
  `OnCannotTreeCut("MOUNTED")`. The server validates again regardless.

### 6.4 Network (`PythonNetworkStream*.cpp`, `PacketDebugRegGen.h`)

- Register `HEADER_GC_TREE_CUT` with `sizeof(TPacketGCTreeCut)` static size; add
  `RecvTreeCutPacket()`:
  - `START`: cutter `NEW_LookAtDestInstance(tree)`, `NEW_Stop()`.
  - `HIT`: cutter `PushOnceMotion(NAME_TREE_CUT)`; tree `PushOnceMotion(NAME_DAMAGE)` if the
    model has one (shake); if the cutter is the main player, `uiTarget.SetHP(hp_pct)` is already
    driven by the `GC_TARGET` packet, nothing extra.
  - `STOP`: nothing to undo (one-shot motions return to wait on their own); clear any client
    "cutting" flag used for the UI hint.
  - `FELL`: play the wood-fall sound/effect at the tree; the actual fall animation comes from
    the normal `GC_DEAD` handling of the tree instance.
- `SendTreeCutStopPacket()` called from `game.py` on ESC / move keys so the stop is immediate
  rather than waiting for the server's next swing re-validation.
- Add both packets to `PacketDebugRegGen.h` so the packet debugger prints them.

### 6.5 Python (`Lead-Client/pack/root`, `uiscript`)

- `game.py`: `OnCannotTreeCut(reason)` → chat info; `SendTreeCutStop` on ESC.
- `localeInfo.py` + `locale/<lang>/locale_game.txt`: `GAME_CANNOT_TREE_CUT_NEED_AXE`,
  `..._MOUNTED`, `..._TOO_FAR`.
- `uitarget.py`: no change needed for the HP bar; optionally hide the attack/whisper buttons for
  trees (treat like a resource).
- `npclist.txt`: one line per tree vnum → model folder (`20400	tree_oak	`).
- `uiinventory`/tooltips: `ITEM_TYPE_AXE` gets the same tooltip block as `ITEM_TYPE_PICK`
  (damage, practice points `socket0/value2`, grade).

### 6.6 Client data regeneration

After editing `item_proto.txt` / `mob_proto.txt` and the per-language name files, regenerate the
binary `item_proto` / `mob_proto` for **every** locale pack (`pack/locale_*/locale/*/`) with
`Lead-Tools/DumpProto` (its `ItemCSVReader.cpp` string table also receives `"ITEM_AXE"`).
Add item icons `pack/icon/item/29201.tga … 29210.tga` and `5074x.tga`.

---

## 7. Rendering backends (DX9 / DX12)

Nothing in the feature touches the graphics layer:

- The chop animation is a Granny track group applied to the existing skinned PC meshes; the
  deformed vertices are uploaded through the shared deform vertex buffers in
  `EterGrnLib/LODController.cpp`, which are `CGraphicVertexBuffer`s and therefore already have
  DX12 twins.
- The tree model is a normal NPC `.gr2` + `.msm`; its textures take the standard loader path
  that builds DX12 texture twins when the backend is live.
- Hit/fall effects are `.mse` files rendered by the effect pool, also backend-agnostic.

What the plan *does* require is that the acceptance tests (§11) run twice, with `renderer dx9`
and `renderer dx12` in the client config, and compare: animation plays, axe stays attached,
tree damage shake and fall render, HP gauge updates, no validation-layer errors in the DX12 log.

---

## 8. Shared packet/struct discipline

- All new packets are declared inside the existing `#pragma pack(1)` section of `packet.h`
  using `BYTE`/`WORD`/`DWORD`/`int32_t` only; sizes are asserted at compile time
  (`static_assert(sizeof(TPacketGCTreeCut) == 11)`).
- Both sides register the packet size in their header maps (`packet_info.cpp` on the server,
  `CNetworkPacketHeaderMap` on the client), as every existing packet does.

---

## 9. 32-bit / 64-bit portability rules for this feature

The repo currently builds **x64 only** (client) and **amd64 only** (server). The feature is
written so that re-adding a Win32 configuration needs no changes here:

1. Event payloads store `pid`/`vid`, never object pointers (same as `mining_event_info`).
2. No `long` in data that crosses the wire or is stored; `long` is 32-bit on Win64 and 64-bit on
   FreeBSD amd64. Use `int32_t`/`int64_t`.
3. Practice points on the axe use the item socket (`TimeT64` as picks do); printing uses `%lld`.
4. No pointer ↔ `DWORD` casts; container sizes use `size_t` locally and are narrowed explicitly
   when sent.
5. Python bindings use `Py_BuildValue("(ii)")` with `int`s; no `Py_ssize_t`/pointer leakage.
6. Packed structs are declared once in the shared header and included by both builds.

---

## 10. Asset tasks (needed before the client phase can be verified)

1. **Chop animation** (`Han_Treechopper.GR2`):
   - Open in Granny Viewer / 3ds Max; record the track group (skeleton) name and the exact
     duration; write `tree_cut.msa` with that `MotionDuration` and an event at the impact frame
     (`MotionEventType 1` effect "wood chips" + `MotionEventType 10`-style sound), modelled on
     `dig.msa`.
   - Set `SWING_MS` in `tree_proto.txt` to the same duration (±100 ms) so the server loop and
     the motion loop stay in step.
   - Test on all 8 player models. The file was made on the male warrior rig (screenshot); the
     other 7 must be checked for bone-name mismatches (motion silently not playing) or
     proportions. Retarget/export per skeleton where needed.
2. **Tree model(s)**: a `.gr2` with `wait`, `damage` (shake) and `dead` (fall) motions, a `.msm`
   with a collision sphere sized to the trunk, textures, `motlist.txt`. For development the
   repo's `npc/christmas_tree` or `npc/mineral/fossil_tree` can be used as a stand-in (static,
   no fall). Note that `fossil_tree` is already vein vnum 20049; do not reuse that vnum.
3. **Axe model + icons**: a weapon `.gr2` for the 8 player rigs (or reuse an existing two-hand
   axe-like weapon model for the first pass) and `icon/item/2920x.tga`.
4. **Effects/sounds**: `effect/etc/tree/woodchips.mse`, `sound/tree_hit.wav`, `tree_fall.wav`.

---

## 11. Implementation phases, files and verification

Each phase ends with a build of both targets and the listed check before the next starts.

### Phase 0 — Asset preparation (parallel with phases 1–3)
§10. Check: animation plays on every PC model in the character viewer; tree model has a fall.

### Phase 1 — Shared definitions
Files: `Lead-Shared-Source/packet.h`, `Lead-Shared-Source/common/item_length.h`,
`Lead-Client-Source/GameLib/ItemData.h`, `GameLib/RaceMotionData.h`,
`Lead-Server-Source/db/src/ProtoReader.cpp`, `Lead-Tools/DumpProto/dump_proto/ItemCSVReader.cpp`.
Check: both solutions compile; `static_assert`s pass; DumpProto converts an `ITEM_AXE` row.

### Phase 2 — Server core
Files: new `game/src/tree_cut.{h,cpp}`; `char.h/.cpp` (members, `OnClick` hook, cancel hooks,
`Dead` guards, `UnequipItem`); `char_item.cpp`, `item.cpp` (`ITEM_AXE`); `regen.cpp`
(atlas skip); `sectree_manager.cpp` (`tree.txt`); `main.cpp` (loaders); `packet_info.cpp`,
`input_main.cpp` (CG packet); `cmd_gm.cpp`, `cmd.cpp` (commands); `questlua_pc.cpp`,
`questlua_global.cpp`; `Makefile`/`.vcxproj` entries for the new files.
Check (no client needed): boot with the data files from phase 3, `/tree_spawn`, `/tree_info`,
simulate swings with `/tree_fell`; log shows drops, regen respawns after `time`; parse errors
in `tree_drop.txt` abort boot with file/line.

### Phase 3 — Server data
Files: `share/conf/mob_proto.txt` + `mob_names*.txt`, `share/conf/item_proto.txt` +
`item_names*.txt`, `share/locale/<lang>/tree_proto.txt`, `tree_drop.txt`,
`share/locale/<lang>/map/metin2_map_a1/tree.txt` (first map), `locale_string*.txt`,
`quest/lumberjack.quest` (axe upgrade dialogue at NPC 20015, mirroring `mining.quest`).
Check: `db` core loads the new protos; `/reload tree` works; the packaging script
(`Lead-Tools/freebsd-pkg/build.sh`) stages the new files (they live under `share/`, so no
change expected — verify).

### Phase 4 — Client
Files: `UserInterface/PythonNetworkStream.{h,cpp}`, `PythonNetworkStreamPhaseGame.cpp`,
`PacketDebugRegGen.h`, `InstanceBase.{h,cpp}`, `InstanceBaseBattle.cpp`, `InstanceBaseEffect.cpp`,
`PythonPlayer.cpp`, `PythonCharacterModule.cpp`, `PythonItemModule.cpp`,
`root/playersettingmodule.py`, `root/game.py`, `root/localeInfo.py`, `root/npclist.txt`,
`root/uitooltip.py`, `uiscript` if a hint is shown, locale packs (`locale_game.txt`,
regenerated `item_proto`/`mob_proto`), `pack/PC/**/general/tree_cut.*`, `pack/NPC/tree_*`,
`pack/icon/item/*`.
Check: see §11 test matrix; repack with `PackMakerLite`.

### Phase 5 — Test and tune
§11 matrix on DX9 and DX12; drop-rate verification script (fell 1 000 trees with `/tree_fell`
in a loop on a test core and compare counts with `tree_drop.txt` percentages); tune
`HARDNESS_PCT`, axe damage, `SWING_MS`.

### Phase 6 — Documentation
`docs/tree-cutting/ADMIN.md`: how to add a tree kind, how to place trees on a map, how to edit
drops, GM commands. Update `README.md` repository-layout table if a `docs/` folder is new.

### Test matrix (phase 5)

| # | Scenario | Expected |
|---|---|---|
| 1 | Click tree with no weapon / with a sword | client hint; server refuses with message; no motion |
| 2 | Click tree with axe, in range | START, HIT every `SWING_MS`, HP gauge decreases, chop animation each swing |
| 3 | Walk away mid-cut | STOP within one swing, "Cutting is finished." |
| 4 | Unequip axe / mount mid-cut | STOP immediately |
| 5 | Two players on one tree, `TOP` mode | shared HP; top contributor gets fell drops with ownership; other sees items but cannot pick for `OWNER_S` |
| 6 | `ALL` mode, 3 contributors (50/30/20 %) | three independent rolls |
| 7 | Axe grade below `AXE_GRADE_MIN` / level below `LEVEL_MIN` | refused with message |
| 8 | Tree falls | FELL packet, fall motion, items on ground, character removed after `FELL_DELAY_MS`, respawn after regen `time` at the same spot and direction |
| 9 | Leave a tree at 1 HP for `HP_RESET_S` | HP back to max |
| 10 | Logout / warp / death mid-cut | no dangling event (checked with `/tree_info` and server log) |
| 11 | `/reload tree` with a broken line | reload refused, old tables kept, error names file and line |
| 12 | Same scenarios 2, 5, 8 with `renderer dx12` | identical visuals, no DX12 errors |
| 13 | Per-swing `hit` drops at 10 % over 200 swings | ≈20 ± statistical margin |
| 14 | Spam clicking the tree | one cutting session; second click cancels; no faster than `SWING_MS` |
| 15 | Packet fuzz: CG_TREE_CUT with foreign vid / while not cutting | ignored, no crash |

---

## 12. Open decisions (defaults chosen, change before phase 2 if wanted)

| Decision | Default in this plan | Alternative |
|---|---|---|
| Who gets the drops when several people cut | `TOP` contributor per tree kind | `ALL` ≥ 20 % |
| Axe damage source | axe item `value0..1` scaled by tree `HARDNESS_PCT` | fixed per-tree damage, axe only gates grade |
| Axe progression | practice points + NPC upgrade, cloned from the pickaxe | none, or a real `SKILL_LUMBERJACK` with levels (v2) |
| Tree HP location | `mob_proto.MAX_HP` | column in `tree_proto.txt` |
| Hit drops | optional `hit` group per tree | fell-only |
| Can trees be cut while mounted | no (as mining) | yes with a mounted animation (none delivered) |
| Trees on the atlas | hidden | shown as NPC markers |
| v2 ideas | stump mob (20450+) spawned at fell position, tree growth stages, wood crafting, lumber quests | — |

---

## 13. Risks and mitigations

| Risk | Mitigation |
|---|---|
| The single GR2 only fits the male warrior rig; other models play nothing or deform | Phase 0 check on all 8 models before client work; budget retargeting; fall back to `dig.gr2` per model temporarily so the loop is testable |
| The `.msa` duration and `SWING_MS` drift | server is authoritative; client plays one-shot per HIT, so drift only causes a visual gap, never desync |
| `CHARACTER::Dead` side effects for an NPC-type kill (exp, quest kill, drop) | explicit `IsTree` guards + test 8; alternative is a custom "fall" packet without `Dead`, kept as plan B |
| `regen_load` lists trees on the atlas | skip in the NPC branch (§4.3), test by opening the atlas |
| Packet id or item-type collision with other forks' systems merged later | ids chosen from verified free ranges and centralised in one place each |
| Client proto dumps forgotten for one locale | phase 4 check list includes every `locale_*` pack; DumpProto run scripted |
| Exploit: fake client sends clicks from afar / without axe | all checks repeated server-side every swing; distance limit 300 units; logs on refusal in test-server mode |
