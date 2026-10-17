# Plan: per-quality drop rate settings (Custom page)

## Decisions (user, 2026-10-16)
- One row per quality: Unique / Set / Rare / Magic.
- Values per row: Off / x1 / x2 / x3 / x5 / x10 (default x1).
- Multiply with Elysium (Elysium unique x5 * row x10 = x50).
- "Off" = player farms only what they want.

## How the rate is computed (read from main 3.3.94012, 0x65c7c4..0x65c928)
All 32-bit signed:
```
chance = (base - lvl_diff / divisor) << 7            ; MF: * 12800 / (100 + mf') instead
chance = max(chance, min)
chance -= chance * tc_bonus / 1024                   ; tc_bonus = u16 from treasure class
chance < 1 -> always; else roll % chance < 128 -> hit
```
Record (itemratio, 0x44 B): unique +0x00, rare +0x0c, set +0x18, magic +0x24
(base / divisor / min). Unique and set offsets confirmed in code; rare / magic offsets must be
confirmed by a DM_ELYSIUM_LOG dump against itemratio.txt column order before use.
"x N" = base / N, divisor * N, min / N (same as Elysium, dm_elysium.c:9-11).

## Why "Off" is not done in the ratio table
- tc_bonus can cut chance to 0 (always hit) whatever min is: boss TCs would still drop it.
- `chance * tc_bonus` is a 32-bit mul: a huge min overflows (min must stay < 2^31/1024).
So "never generate" cannot be made exact this way. Fail fast: do not ship a fake "off".
Off = hide with the game's own loot filter (dm_lootfilter.c): equipmentRarity rule.
Item still drops but has no name label and the controller does not target it.
Limit: the loot filter only works in Reign of the Warlock mode (mode gate in 0x37f3d4).
In other modes the "Off" value is shown grey and not selectable.

## Mechanism
- Rates: dm_elysium.c snapshot/restore stays the single owner of itemratio. `apply()`
  takes 4 multipliers = Elysium mul * setting mul. Normal char + all x1 = original table
  (keep the checksum red log for that case).
- Setting change on Apply: main-thread pump restores and re-applies at once (no run restart).
- Off: dm_lootfilter.c profile files are loaded only at startup, so generate one profile per
  combination at save mount: 3 tiers x 2^4 off-masks = 48 small .fltr files, names <= 10 chars
  (e.g. `dmH0101`). Runtime picks the name like today (in-place rename of current profile).
- Cap note: x10 on an Elysium char can push chance < 128 -> 100% unique; set / rare then
  never roll for that item. Log it once; no UI clamp.

## Steps
1. Confirm rare / magic offsets (dump + itemratio.txt). Check: log shows expected values.
2. dm_custom.c: multi-value keys (today 0/1 only, `eq[1] != '0'`). Keys `dq_unique`,
   `dq_set`, `dq_rare`, `dq_magic` = 0..5 (Off,x1,x2,x3,x5,x10). Default/discard/apply as today.
3. UI row type "cycle": label + value text, tap (or pad A) cycles to next value.
   touch_layout.py `_custom_cycle_row`; ui_text.tsv zh/en.
4. dm_elysium.c: parameterised apply + re-apply on setting generation change.
5. dm_lootfilter.c: off-mask in profile choice + 48 generated profiles.
6. Verify:
   - `make -C dm/ios/tests test` and host tests build.
   - Mac host, DM_ELYSIUM_LOG=1: dump at x1 / x5 per quality; restore checksum on exit = 相符.
   - Mac host: unique Off -> unique drop has no label; switch back -> label returns without restart.
   - Normal char all x1 -> no red log.

## Open questions
- None.
