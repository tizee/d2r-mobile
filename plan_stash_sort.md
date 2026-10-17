# Plan: shared-stash tab sort by long press

## Root cause (verified in main 3.3.94012, /tmp/main.orig.bin from dm-work ExeFS)
Original controller "hold to sort" (main+0x3b2e58..0x3b2ea0) reads the owner from the grid widget:
```
w2 = [grid+0x5c4]   owner unit id
w3 = [grid+0x5c8]   owner unit type
unit = (id == -1 && type == 6) ? NULL : 0x45f5f0(main+0x2d603a0 + type<<10, id & 0x7f, id, type)
if (0x3c213c(unit, [grid+0x630]) & 1) 0x3c2320([grid+0x5c4], [grid+0x630])
```
Ours (dm/host/dm_actions.c:580) always passes the player unit / player id.
Shared tabs are separate stash units (dm_pouch.c:198, dm_elysium.c:244), so a long press
on a shared tab sorts the personal tab instead (no packet if already sorted = "no reaction").

## Change
1. dm_cursor.c ui_long_pump (1463-1473): when the hit slot is a multi-cell grid
   (vtable main+0x2089ab8, check via dm_widget_is), read owner id +0x5c4, type +0x5c8,
   page +0x630; skip if type == 1 (NPC / vendor). Call `dm_inv_sort_owner(id, type, page)`.
   Delete the parent-chain name heuristic (stash/bank/cube strings) - the widget fields are the truth.
   Keep the "not a grid -> do nothing" path (advanced stash stacked cells, equipment, belt).
2. dm_actions.c: replace `dm_inv_sort(int page)` with `dm_inv_sort_owner(uint32_t id, uint32_t type, int page)`;
   pump replicates the original sequence above exactly (including the id==-1 && type==6 -> NULL case).
   Store the request as one packed atomic u64 (id | type<<32 | page<<40) to stay lock-free.
3. Update dm_actions.h comment; update stubs host/tests/t_touch_mac.c:40, t_touch_tick_stub.c:51.
4. Log: owner id / type / page and the 0x3c213c result.

## Verify
- Build host tests: `make -C dm/host/tests` (or the repo's test runner) - stubs compile.
- Mac host run (tools/mac_app.sh or guirun.sh): open stash, shared tab 1..N with scattered items,
  long-press an empty cell -> log `整理：属主 <id> 类 <type> 页 4 已调原版整理`, items move.
- Regression: long press empty cell in inventory (page 0), cube (page 3), personal tab (page 4) still sort.
- verify/touchcases/inv_long_press still passes.

## Open questions
- None blocking. If a shared tab still does not sort, next suspect is 0x3c213c rejecting
  the client stash unit (then read 0x3c213c).
