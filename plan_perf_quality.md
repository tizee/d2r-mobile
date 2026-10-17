# Plan: heat / performance, quality presets, developer section

## Evidence (current state)
- Output = full native resolution: drawableSize = bounds x nativeScale (dm/ios/Sources/front_ios.m:922-923)
  -> iPhone 17 Pro 2622 x 1206 = 3.16 MP for UI + final pass.
- 3D target = 0.75 x output on iOS (dm/host/hle_oe.c:136-159) = 1966 x 904 = 1.78 MP, dynamic floor 0.60,
  driven by dropped frames only, not by heat (dm_3d_scale_hot is a no-op, hle_oe.c:234).
- Switch handheld renders 1280 x 720 = 0.92 MP. We push ~2x its 3D pixels and ~3.4x its UI pixels at 60 fps.
- Frame rate: 60 or 30 (eco_fps). No 40 fps step, no user choice of resolution.
- Existing telemetry: perf.txt every 10 s (diag.c:1501), long-frame lines, thermalState logged only.
Conclusion: heat is mostly GPU fill rate at native resolution x 60 fps, not CPU emulation.
The game logic is 25 ticks/s; nothing needs 60 fps except camera smoothness.

## 1. Quality presets (Custom page, one radio row "画质")
| Preset | Output scale | 3D scale | FPS | Approx GPU pixels/s vs now |
|---|---|---|---|---|
| 原生高清 | 1.0 | 0.85 | 60 | 1.2x |
| 高（current default） | 1.0 | 0.75 | 60 | 1.0x |
| 均衡（new default） | 0.75 | 0.75 | 60 | 0.56x |
| 流畅省电 | 0.67 | 0.70 | 40 | 0.30x |
| 极致省电 | 0.6 | 0.65 | 30 | 0.18x |
- Output scale = drawableSize = bounds x nativeScale x s (CAMetalLayer upscales for free in the compositor).
  Must re-run the game's resolution-change path (main+0x8582f0) like the 3D scale does.
- 40 fps: ProMotion supports 120/3; set CADisplayLink preferredFrameRateRange {40,40,40} and the
  pacing target in nvn_mtl.m mtl_fps_target (today returns 30/60 from dm_fps_setting).
- Replaces eco_fps and q3d rows (fold into presets); keep dynamic 3D floor in all but 原生高清.
- Thermal guard: at thermalState >= serious, drop one preset step automatically, restore at nominal (logged).

## 2. Developer section (Custom page, rows under a "开发者" title)
- 显示性能浮层 (switch): small UIKit label top-left, 2 Hz update:
  FPS (present intervals), 1% low, GPU ms (MTLCommandBuffer GPUStartTime/GPUEndTime),
  CPU % of process (task_info thread times), 3D scale, output size, thermalState, battery % and
  drain %/h (UIDevice batteryLevel slope; iOS has no direct power API), low power mode.
- 记录性能到文件 (switch): perf.txt line every 2 s instead of 10 s, same format + new fields.
- 分享日志 (button): reuse dm_sharelog.m.
- All new strings in dm/i18n/ui_text.tsv.

## 3. Measurement before/after (decides defaults)
- Device: iPhone 17 Pro, same save, same route (town -> waypoint -> area clear), 10 min each preset.
- Record from the developer overlay / perf.txt: FPS, 1% low, GPU ms, thermalState timeline, battery drain.
- Accept: 均衡 keeps 60 fps 1% low >= 50 and stays thermal "nominal/fair" for 10 min.

## Phases
1. Developer overlay + perf fields (needed to measure). Check: unit test for stats ring; device screenshot.
2. Output scale plumbing + 40 fps pacing. Check: Mac host runs at s = 0.75 (picture gate unaffected at 1.0);
   device: drawableSize logged, fps steady at 40.
3. Preset row + migration of eco_fps / q3d values. Check: dm_custom load/save test.
4. Thermal guard. Check: DM_TEST_THERMAL=<state> switch drives the step in a host test.
5. Device measurement table -> pick default.

## Open questions
1. Default preset: 均衡 (recommended) or keep 高?
2. OK to remove the separate 省电模式 and 3D 画质 rows (folded into presets)?
