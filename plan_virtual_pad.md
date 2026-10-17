# Plan: full on-screen console buttons + layout editor (touch mode)

## Decisions (user, 2026-10-16)
- No physical controller. On-screen buttons only. Touch drag-drop and direct taps stay.
- In NPC dialog / trade / panels, the player selects / confirms / cancels with on-screen
  D-pad + A / B.
- Full console set: D-pad, A B X Y, L R ZL ZR, + / -, plus stick.
- Layout editor is required (position, size, opacity, show/hide per context).
- Device: iPhone 17 Pro. 402 x 874 pt (landscape 874 x 402), @3x, 460 ppi (Apple spec).
  1 pt = 0.166 mm; 44 pt (HIG min target) = 7.3 mm. Landscape safe area: about 59-62 pt
  left/right (Dynamic Island side), 21 pt bottom - read at runtime (dm_tc_set_safe_insets).
  Usable area about 750 x 381 pt. dm_tc_scale_for_size gives 1.0 here.

## Current state (evidence)
- Combat: stick + MAIN(A) + SKILL1-4 (B/X/Y/R, dm_cursor.c:1657) + swap (ZL set) + portal +
  belt + menu (dm_touch_controls.h:12-16). Positions are computed in dm_tc_layout_make; no editor.
- Panel / dialog: everything except the menu cross is disabled (front_ios.m:183-184,
  DM_TOUCH_NAV_PAD default 0). Stick disappears. Only direct taps.
- Missing keys: D-pad, L, ZR, -, and any button in panel / dialog mode.
- Physical pad path exists (dm_gamepad.m -> dm_pad_set, dm_padmode.c) but switches to the
  controller UI and is cancelled by any screen touch (front_ios.m:503). Not used here.

## Design
Stay in touch mode (touch layouts, touch drag-drop). Add data-driven buttons whose keys are
OR-ed into the buttons the gesture state machine already publishes (dm_input_set).

1. Control registry (pure C, `dm_vpad.c`): one entry per control
   `{id, key mask, kind (button / dpad / stick / skill / belt / portal), x, y, size, opacity,
   contexts (combat, panel, dialog)}`. Existing controls become entries with today's positions
   as defaults, so combat stays the same out of the box.
2. New entries: D-pad (8-way, repeat on hold by game), A B X Y (panel/dialog), L R ZL ZR, + / -.
   Defaults in panel/dialog: D-pad bottom-left, A/B/X/Y bottom-right, L/R top corners,
   opacity 0.5, size 48 pt, outside the stash+inventory panel rect.
   Combat: D-pad hidden by default (belt cells already cover D-pad = potions), user can show it.
3. Hit priority: vpad control > existing gesture targets > UI tap. A finger that starts on a
   vpad control is owned by it until release (slide off = release, no UI tap) -> no mis-taps.
4. Host side: pad keys must reach our own pages in touch mode. Today they check
   `dm_padmode_current()` (dm_custom.c:137, dm_kills.c:299, dm_padmode.c:266). Add
   `dm_pad_nav_active()` = padmode || vpad pad-keys visible, and use it there.
5. Layout editor: entry in the Custom page ("按键布局") and a long press on the HUD gear.
   Edit mode freezes input to the game, shows all controls of the chosen context, drag to move,
   pinch / slider to resize, opacity slider, hide toggle, reset to default, save.
   Stored in `sw_vpad.txt` (Documents, next to sw_custom.txt), atomic tmp + rename.
   Positions saved as fractions of the safe area, so rotation / size change keeps them.
6. Skill binding stays the game's own (skill tree -> Y -> ButtonBindingModal). Extra:
   basic skills like Throw (throwing potions) - verify reachability first (see below).

## Phases (each ends with a check)
0. Spike (Mac host, no product code kept): in touch layouts, open NPC dialog, vendor, stash,
   inventory; send D-pad / A / B / L / R via a touch script (`padkey` op or DM test switch).
   Confirm focus moves and A/B act. Also check whether Throw appears in the skill tree.
   Check: screenshots + log from `ios/tools/touch_e2e_mac.sh`. If D-pad focus is broken in
   touch layouts, stop and re-plan.
1. Registry + defaults (no visual change): move dm_tc_layout_make positions into dm_vpad.c.
   Check: `make -C dm/ios/tests test` - all existing t_touch_controls cases unchanged.
2. New buttons + panel/dialog contexts + hit priority + dm_pad_nav_active.
   Check: new unit cases (dpad 8-way, slide-off, multi-finger with drag-drop in other finger,
   context switch releases held keys); e2e script: NPC -> D-pad to Trade -> A -> buy -> B.
3. Drawing (DmTouchOverlay): button glyphs, opacity, pressed state.
   Check: screenshots per context on Mac host at 874 x 402.
4. Layout editor + sw_vpad.txt.
   Check: unit test load/save round trip + bad file -> defaults + red log;
   device: edit, kill app, relaunch, layout kept.
5. Device pass on iPhone 17 Pro: safe area, Dynamic Island side, home indicator strip.

## Side issue: throwing potions
Original D2 rule: equip, then use the basic skill Throw. The touch bind flow only walks the
class skill tree (dm_cursor.c:1636). If Throw is not reachable there (phase 0 check),
add a bind path for basic skills (Attack / Throw / Kick / Unsummon) in the bind flow.

## Scope boundaries
- Do not touch controller-UI layouts or physical-pad switching.
- Do not change gesture semantics inside panels (tap, double tap, long press, drag-drop).

## Known traps
- Pressing L in game = "show items" toggle; in panels L/R = prev/next tab (front_ios.m:193).
  Context matters; do not auto-press L.
- ZL must lead a skill key by 0.17 s (DM_TC_ZL_LEAD_S). A held on-screen ZL is fine; a
  ZL+skill combo button must reuse the existing defer logic.
- Context switch (combat <-> panel) must release all vpad keys (dm_tc_set_navigation_mode
  already cancels gestures; vpad must follow).
- Touch strings say "double-tap" instead of "press A" (touch_strings.tsv). Keep touch wording.

## Open questions
- None blocking. Phase 0 result decides whether phase 2 is feasible as written.
