// 「自定义」设置：我们自己加的功能的启停开关。接口见 dm_custom.h。
// 存储：存档提交目录旁的 sw_custom.txt（iOS 上在 Documents 里，升级包不丢），一行一个「键=值」；没有这个文件 / 没写的键 = 各键缺省（见下面键表）；不认识的行（旧版的 drop_filter、ver）读时跳过。
// 掉落筛选照红叶 mod 一件都不藏，整块拆掉，drop_filter 键与它的 ver=2 迁移一起删了
// 不写进游戏自己的设置存档：不依赖它的格式与写盘时机，应用 / 放弃 / 默认由我们自己按原版选项页的语义做。
// 界面：选项面板「自定义」页（借原版视频页 VideoOptionsPanel），每行一个原版勾选框，控件名 TouchCustom_<键>（布局 touch_layout.py 的 _custom_page）。勾选只记成待定值，
// 「应用」（OptionsPanelMessage:AcceptChanges）才生效并存盘，「放弃」（CancelChanges）丢掉待定值，「默认」（ResetToDefaults）待定值回各键缺省（还要点应用）
// 「自定义」(Custom) settings: on/off switches for the features we added ourselves. For the interface see dm_custom.h.
// Storage: sw_custom.txt next to the save commit directory (on iOS it is in Documents and survives app upgrades), one 'key=value' per line; without this file / for keys not written = each key's default; unrecognized lines (drop_filter and ver from old versions) are skipped when reading.
// Drop filtering follows the Hongye community mod and hides nothing, so the whole block was removed, and the drop_filter key was deleted together with its ver=2 migration
// Not written into the game's own settings save: no dependence on its format or on when it writes to disk; apply / discard / default are done by ourselves following the semantics of the original game's options page.
// UI: the 「自定义」(Custom) page of the options panel (borrowing the original game's video page VideoOptionsPanel), one original-game checkbox per row, widget name TouchCustom_<key> (_custom_page in the layout touch_layout.py). Checking only records a pending value;
// only 「应用」(Apply) (OptionsPanelMessage:AcceptChanges) makes it take effect and saves to disk, 「放弃」(Discard) (CancelChanges) throws away the pending values, and 「默认」(Default) (ResetToDefaults) sets the pending values back to each key's default (Apply still has to be tapped)
#include "dm_custom.h"
#include "dm_widget.h"
#include "dm_cursor.h"
#include "hle.h"
#include "dm_input.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 键表：顺序 = 自定义页从上到下的行序（touch_layout.py _CUSTOM_KEYS，语言行在最前）；手柄在这页按这个顺序上下选行。
// n = 0：勾选框（值 0 / 1）；n > 1：左右选择行（值 0..n-1，页上「◀ 值 ▶」，到头不回绕）。
// 画质 gfx_q：0 原生高清 / 1 高 / 2 均衡（缺省）/ 3 流畅 / 4 低（输出分辨率与 3D 比例，见 hle_oe.c dm_gfx_*）；最高帧率 fps_max：30 / 40 / 60（缺省 60）。
// 掉率 dq_*：×1 / ×2 / ×3 / ×5 / ×10（缺省 ×1 = 原版），生效见 dm_elysium.c。开发者 perf_hud / perf_log：性能浮层与 2 秒一行的 perf.txt（缺省关）。
// 旧版的 eco_fps（省电模式）、q3d（3D 画质）两键并进画质 / 帧率两行，旧文件里的这两行读时跳过
// Key table: order = row order of the Custom page from top to bottom (touch_layout.py _CUSTOM_KEYS, the language row first); the controller selects rows up/down on this page in this order.
// n = 0: checkbox (value 0 / 1); n > 1: left/right selector row (value 0..n-1, shown as "◀ value ▶" on the page, no wrap-around at the ends).
// Graphics gfx_q: 0 native / 1 high / 2 balanced (default) / 3 smooth / 4 low (output resolution and 3D scale, see hle_oe.c dm_gfx_*); max frame rate fps_max: 30 / 40 / 60 (default 60).
// Drop rates dq_*: x1 / x2 / x3 / x5 / x10 (default x1 = original game), applied in dm_elysium.c. Developer perf_hud / perf_log: performance overlay and a perf.txt line every 2 s (default off).
// The old eco_fps (power saving) and q3d (3D quality) keys are folded into the graphics / frame-rate rows; those lines in old files are skipped on read
typedef struct { const char *key; int def, n; } ckey;
static const ckey k_tab[] = {
    { "lang_en", 0, 0 }, { "gfx_q", 2, 5 }, { "fps_max", 2, 3 },
    { "auto_mana", 1, 0 }, { "merc_pot", 1, 0 }, { "drop_light", 1, 0 }, { "map_mark", 1, 0 }, { "item_notes", 1, 0 }, { "icloud", 1, 0 }, { "pot_pick", 1, 0 },
    { "drop_haptic", 1, 0 }, { "gem_pick", 1, 0 }, { "store_btn", 1, 0 }, { "loot_filter", 1, 0 }, { "auto_buff", 1, 0 }, { "nav_pad", 1, 0 }, { "touch_swap_above", 0, 2 },
    { "dq_unique", 0, 5 }, { "dq_set", 0, 5 }, { "dq_rare", 0, 5 }, { "dq_magic", 0, 5 },
    { "perf_hud", 0, 0 }, { "perf_log", 0, 0 } };
#define NK (int)(sizeof k_tab / sizeof *k_tab)
static const int k_dq_mul[5] = { 1, 2, 3, 5, 10 }, k_fps[3] = { 30, 40, 60 };
static int sel_n(int i) { return k_tab[i].n; }
const char *dm_str(uint8_t *main_base, const char *key);
static uint8_t *main_base(void);
static void sel_text(int i, int v, char *out, size_t cap) {   // 选择行当前值的字 | text of a selector row's current value
    const char *k = k_tab[i].key;
    if (!strcmp(k, "gfx_q")) { char kk[16]; snprintf(kk, sizeof kk, "dmGfxQ%d", v); const char *t = main_base() ? dm_str(main_base(), kk) : NULL; snprintf(out, cap, "%s", t ? t : kk); }
    else if (!strcmp(k, "fps_max")) snprintf(out, cap, "%d", k_fps[v]);
    else if (!strcmp(k, "touch_swap_above")) { const char *t = main_base() ? dm_str(main_base(), v ? "dmCustomTouchSwapFar" : "dmCustomTouchSwapNear") : NULL; snprintf(out, cap, "%s", t ? t : (v ? "far" : "near")); }
    else snprintf(out, cap, "x%d", k_dq_mul[v]);
}
static _Atomic int g_val[NK];        // 已生效：1 开 / 0 关 | In effect: 1 on / 0 off
static _Atomic int g_pend[NK];       // 选项页上改了还没应用：-1 = 没改 | Changed on the options page but not applied yet: -1 = not changed
static _Atomic int g_loaded;         // 0 没读盘 / 1 已读 | 0 not read from disk / 1 read
// lang_en（语言开关，自定义页第一行）：勾上 = 英文界面 + 英文语音。它不是功能开关，生效方式不同——下次启动挂存档时由 hle_fs.c 按它改写原版设置文件的
// Language / VoiceLanguage（游戏运行中自己会把内存里的语言写回设置文件，所以只能在游戏读设置之前改；见 dm_custom_lang_explicit）。
// 文件里没有这个键（老玩家、还没点过应用）= 不动原版设置，勾选框按当前实际语言显示；「默认」不改它（按一下默认把语言切了不合适）
// lang_en (language switch, first row of the Custom page): checked = English UI + English voice. It is not a feature switch and takes effect differently -- at the next launch, when the save is mounted, hle_fs.c rewrites the original game's settings file's
// Language / VoiceLanguage according to it (while running, the game itself writes the in-memory language back to the settings file, so it can only be changed before the game reads its settings; see dm_custom_lang_explicit).
// Key absent from the file (an existing player who has not yet tapped Apply) = the original settings are left alone, and the checkbox shows the actual current language; 「默认」(Default) does not change it (switching the language at one press of Default would be inappropriate)
static _Atomic int g_lang_explicit = -1;   // 文件里写着的 lang_en：-1 没写 / 0 / 1 | lang_en as written in the file: -1 not written / 0 / 1
// merc_pot（佣兵自动喂药）原先缺省关、现在缺省开。旧文件每次存盘都把全部键写一遍，里面的 merc_pot=0 分不出「没动过」还是「自己关的」，
// 所以另记一行 merc_pot_v2：只有在缺省改成开之后玩家自己在页上改过这一项才写。读盘时有 merc_pot_v2 以它为准；没有 = 没在新缺省下动过 → 开
// （旧文件里的 merc_pot=1 也是开，结果一样）。merc_pot 那一行照旧写当前值，旧版本的包读它不出错
// merc_pot (mercenary auto-feed potions) used to be off by default and is now on by default. Old files write all keys on every save, and the merc_pot=0 in them cannot tell "never touched" from "turned off by the player",
// so a separate line merc_pot_v2 is recorded: it is written only when the player has changed this item on the page after the default was changed to on. On load, if merc_pot_v2 exists it is authoritative; absent = never touched under the new default → on
// (merc_pot=1 in an old file is also on, the same result). The merc_pot line is still written with the current value, so packages of older versions read it without error
static _Atomic int g_merc_explicit = -1;   // 文件里写着的 merc_pot_v2：-1 没写 / 0 / 1 | merc_pot_v2 as written in the file: -1 not written / 0 / 1
#define K_MERC 4
int dm_lang_zh(void);
static _Atomic int g_repaint;        // 默认 / 放弃之后：页上的勾选框按待定值 / 生效值重摆一遍 | After default / discard: lay out the checkboxes on the page again according to pending values / effective values

const char *dm_save_commitdir(void);
static int key_index(const char *k) { if (k) for (int i = 0; i < NK; i++) if (!strcmp(k, k_tab[i].key)) return i; return -1; }
static char g_dir[1024];   // iOS 外壳启动时给的 Documents（main.m）：启动期（存档挂上之前）就要能读，覆盖层选择用 | Documents given by the iOS shell at startup (main.m): must be readable already during startup (before the save is mounted), used for overlay selection
void dm_custom_init(const char *dir) { if (dir) strlcpy(g_dir, dir, sizeof g_dir); }
static int file_path(char *p, size_t n) {
    if (g_dir[0]) { snprintf(p, n, "%s/sw_custom.txt", g_dir); return 1; }
    const char *c = dm_save_commitdir(); if (!c) return 0; snprintf(p, n, "%s/../sw_custom.txt", c); return 1; }

static void load_once(void) {
    if (atomic_load(&g_loaded)) return;
    char p[1100]; if (!file_path(p, sizeof p)) return;   // 存档还没挂上：先按缺省开答，挂上后再读 | The save is not mounted yet: answer with the default (on) for now, and read again after it is mounted
    int v[NK]; for (int i = 0; i < NK; i++) v[i] = k_tab[i].def;
    FILE *f = fopen(p, "r"); char ln[128];
    int lang = -1, merc = -1;
    while (f && fgets(ln, sizeof ln, f)) { char *eq = strchr(ln, '='); if (!eq) continue; *eq = 0; int i = key_index(ln); if (i >= 0) { if (sel_n(i)) { int d = atoi(eq + 1); v[i] = d >= 0 && d < sel_n(i) ? d : k_tab[i].def; } else v[i] = eq[1] != '0'; } if (i == 0) lang = v[0]; if (!strcmp(ln, "merc_pot_v2")) merc = eq[1] != '0'; }
    if (f) fclose(f);
    if (lang < 0) v[0] = !dm_lang_zh();   // 没写过：按当前实际语言显示 | Never written: display according to the current actual language
    atomic_store(&g_lang_explicit, lang);
    v[K_MERC] = merc < 0 ? k_tab[K_MERC].def : merc; atomic_store(&g_merc_explicit, merc);
    for (int i = 0; i < NK; i++) { atomic_store(&g_val[i], v[i]); atomic_store(&g_pend[i], -1); }
    if (!atomic_exchange(&g_loaded, 1)) { char all[512]; size_t L = 0; all[0] = 0; for (int i = 0; i < NK && L < sizeof all; i++) L += snprintf(all + L, sizeof all - L, " %s=%d", k_tab[i].key, v[i]);
        LOG("自定义设置：读盘 %s%s%s%s", p, f ? "" : "（没有文件，全按缺省）", lang < 0 ? "（语言没写，按当前语言）" : "", all); }
}
int dm_custom_get(const char *key) {
    load_once(); int i = key_index(key); if (i < 0) return 1;
    return atomic_load(&g_loaded) ? atomic_load(&g_val[i]) : k_tab[i].def;
}
int dm_custom_drop_mul(int q) { static const char *const k[4] = { "dq_unique", "dq_set", "dq_rare", "dq_magic" }; if (q < 0 || q > 3) return 1; int d = dm_custom_get(k[q]); return k_dq_mul[d >= 0 && d < 5 ? d : 0]; }
int dm_custom_gfx_quality(void) { int v = dm_custom_get("gfx_q"); return v >= 0 && v < 5 ? v : 2; }
static _Atomic int g_pad_edit_req;   // 自定义页「调整屏幕按键」点了、前端还没取走 | Custom page "Adjust on-screen keys" tapped, not yet taken by the front end
int dm_custom_take_pad_edit(void) { return atomic_exchange(&g_pad_edit_req, 0); }
static _Atomic unsigned g_gen;   // 每次「应用」改了值加一（dm_custom_gen） | incremented each time 「应用」(Apply) changes a value (dm_custom_gen)
unsigned dm_custom_gen(void) { return atomic_load(&g_gen); }
int dm_custom_lang_explicit(void) { load_once(); return atomic_load(&g_loaded) ? atomic_load(&g_lang_explicit) : -1; }
int dm_fps_setting(void) { int v = dm_custom_get("fps_max"); return k_fps[v >= 0 && v < 3 ? v : 2]; }   // 图形层 nvn_mtl.m mtl_fps_target 每帧读（弱符号的强定义） | Read every frame by mtl_fps_target in the graphics layer nvn_mtl.m (strong definition of a weak symbol)
static void save(void) {
    char p[1100], t[1120]; if (!file_path(p, sizeof p)) { LOG("【红】自定义设置：存档目录没挂上，没存盘"); return; }
    snprintf(t, sizeof t, "%s.tmp", p); FILE *f = fopen(t, "w"); int ok = f != NULL;
    for (int i = 0; ok && i < NK; i++) { if (i == 0 && atomic_load(&g_lang_explicit) < 0) continue; ok = fprintf(f, "%s=%d\n", k_tab[i].key, atomic_load(&g_val[i])) > 0; }   // lang_en 只在玩家真改过之后才写进文件 | lang_en is written to the file only after the player has actually changed it
    if (ok && atomic_load(&g_merc_explicit) >= 0) ok = fprintf(f, "merc_pot_v2=%d\n", atomic_load(&g_merc_explicit)) > 0;
    if (f) ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(t, p) == 0;   // 先写临时文件再换名：写到一半被杀也不会留下半个文件 | Write a temporary file first and then rename: being killed halfway through the write does not leave half a file behind
    LOG("自定义设置：存盘%s", ok ? "" : "【红】失败");
}
void dm_custom_on_uimsg(const char *cat, const char *act, const char *arg) {
    if (!cat || !act) return;
    // 选择行的左右箭头（ButtonWidget，点击发 PanelManager:ClosePanel:TouchCustom_<键>_L / TouchCustom_<键>，关一个不存在的面板 = 空操作）：待定值走一格，到头不动
    // Left/right arrows of a selector row (ButtonWidget; a tap sends PanelManager:ClosePanel:TouchCustom_<key>_L / TouchCustom_<key>, closing a panel that does not exist = no-op): the pending value moves one step, stopping at the ends
    if (!strcmp(cat, "PanelManager") && !strcmp(act, "ClosePanel") && arg && !strcmp(arg, "TouchCustom_padedit")) { atomic_store(&g_pad_edit_req, 1); LOG("自定义设置：点了「调整屏幕按键」"); return; }   // 前端取走后进布局编辑（front_ios.m） | the front end takes it and enters layout editing (front_ios.m)
    if (!strcmp(cat, "PanelManager") && !strcmp(act, "ClosePanel") && arg && !strncmp(arg, "TouchCustom_", 12)) { char k[40]; strlcpy(k, arg + 12, sizeof k); size_t L = strlen(k); int left = L > 2 && !strcmp(k + L - 2, "_L"); if (left) k[L - 2] = 0;
        load_once(); int i = key_index(k); if (i >= 0 && sel_n(i)) { int p0 = atomic_load(&g_pend[i]), sh = p0 >= 0 ? p0 : atomic_load(&g_val[i]), to = sh + (left ? -1 : 1); if (to < 0) to = 0; if (to >= sel_n(i)) to = sel_n(i) - 1;
            if (to != sh) { atomic_store(&g_pend[i], to); char t[48]; sel_text(i, to, t, sizeof t); LOG("自定义设置：%s → %s（点「应用」生效）", k, t); } }
        return; }
    if (strcmp(cat, "OptionsPanelMessage")) return;
    load_once();
    if (!strcmp(act, "AcceptChanges")) { int ch = 0;
        for (int i = 0; i < NK; i++) { int pd = atomic_exchange(&g_pend[i], -1); if (pd >= 0 && pd != atomic_load(&g_val[i])) { atomic_store(&g_val[i], pd); ch = 1; if (i == 0) atomic_store(&g_lang_explicit, pd); if (i == K_MERC) atomic_store(&g_merc_explicit, pd); if (sel_n(i)) { char t[48]; sel_text(i, pd, t, sizeof t); LOG("自定义设置：%s → %s（应用）", k_tab[i].key, t); } else LOG("自定义设置：%s → %s（应用%s）", k_tab[i].key, pd ? "开" : "关", i == 0 ? "；语言下次启动生效" : ""); } }
        if (ch) { atomic_fetch_add(&g_gen, 1); save(); } }
    else if (!strcmp(act, "CancelChanges")) { for (int i = 0; i < NK; i++) atomic_store(&g_pend[i], -1); atomic_store(&g_repaint, 1); }
    else if (!strcmp(act, "ResetToDefaults")) { for (int i = 1; i < NK; i++) atomic_store(&g_pend[i], k_tab[i].def); atomic_store(&g_repaint, 1); }   // 从 1 起：语言那一格不归「默认」管 | Starts from 1: the language cell is not governed by 「默认」(Default)
}

// ---- 选项页上的勾选框
// ---- Checkboxes on the options page
static uint8_t *main_base(void) { static uint8_t *b; if (!b) for (int i = 0; i < g_nmod; i++) if (!strcmp(g_mod[i].name, "main")) b = (uint8_t *)g_mod[i].base; return b; }
static uint64_t find_w(uint64_t w, const char *name, int dep) {   // 按名字找控件：名字 +8、子控件数组 +0x58、个数 +0x60 | find a widget by name: name +8, child widget array +0x58, count +0x60
    if (!w || dep > 14) return 0; const char *nm = (const char *)*(uint64_t *)(w + 8); if (nm && !strcmp(nm, name)) return w;
    uint64_t ch = *(uint64_t *)(w + 0x58), n = *(uint64_t *)(w + 0x60); if (!ch || n > 256) return 0;
    for (uint64_t i = 0; i < n; i++) { uint64_t r = find_w(*(uint64_t *)(ch + 8 * i), name, dep + 1); if (r) return r; }
    return 0;
}
// 勾选框的状态（本机逐字节差分，点一下只变这几处）：+0xa88 是它自己的「勾上」标志（点击按它翻转）；
// +0x6e8..+0x6f4 是「常态 / 按下 / 悬停 / 禁用」四个帧号，勾上 = 4 5 6 -1、没勾 = 0 1 3 2（= 布局里 toggled* / untoggled* 那几项）；+0x610 是此刻画的帧
// 读写这几处之前核控件是勾选框那个类（dm_widget.h）：不是就当没勾、不写，并记红
// Checkbox state (byte-by-byte diff on this machine; one tap changes only these places): +0xa88 is its own "checked" flag (a click toggles it);
// +0x6e8..+0x6f4 are the four frame ids "normal / pressed / hover / disabled", checked = 4 5 6 -1, unchecked = 0 1 3 2 (= the toggled* / untoggled* entries in the layout); +0x610 is the frame drawn right now
// Before reading or writing these, check the widget is of the checkbox class (dm_widget.h): if not, treat it as unchecked, do not write, and log a failure
DM_WIDGET_RED_DEF
#define TOGGLED(w) (dm_widget_is(main_base(), (w), DM_VT_TOGGLE) && *(uint8_t *)((w) + 0xa88) != 0)
static void set_toggle(uint64_t w, int on) {
    if (!DM_WIDGET_OK(main_base(), w, DM_VT_TOGGLE, "自定义页")) return;
    static const int32_t fr[2][4] = { { 0, 1, 3, 2 }, { 4, 5, 6, -1 } };
    *(uint8_t *)(w + 0xa88) = on != 0; memcpy((void *)(w + 0x6e8), fr[on != 0], sizeof fr[0]); *(int32_t *)(w + 0x610) = fr[on != 0][0];
}
static void set_text(uint64_t w, const char *s) {   // 同 dm_kills.c kb_text：文字框（DM_VT_TEXT）赋值 0x136e5c + 重排 0xadc434，字没变不写 | Same as kb_text in dm_kills.c: text box (DM_VT_TEXT) assign 0x136e5c + relayout 0xadc434; not written if unchanged
    uint8_t *b = main_base(); if (!dm_widget_is(b, w, DM_VT_TEXT)) return;
    size_t n = strlen(s); if (*(uint64_t *)(w + 0x90) == n && !memcmp(*(const char **)(w + 0x88), s, n)) return;
    ((void *(*)(uint64_t, const char *, uint64_t))(b + 0x136e5c))(w + 0x88, s, n); ((void (*)(uint64_t))(b + 0xadc434))(w);
}
void dm_custom_ui_pump(unsigned long long root) {
    if (!root || !main_base()) return;
    static dm_wref seen[NK];   // 每行最近见到的勾选框（带界面代号：重建后取到 0 = 页刚出现） | Most recently seen checkbox of each row (with UI generation: 0 after a rebuild = page just appeared)
    uint64_t sp = dm_ui_top(root, "VideoOptionsPanel"); if (!sp || !*(uint8_t *)(sp + 0x50)) {   // 「自定义」页 = 借来的视频页（界面根的直接子，与设置页并列）。页不在屏上：没点「应用」的勾选作废（同原版不应用就离开），下次出现重摆 | The 「自定义」(Custom) page = the borrowed video page (a direct child of the UI root, alongside the settings page). Page not on screen: checkbox changes without tapping 「应用」(Apply) are discarded (same as leaving without applying in the original game), and it is laid out again the next time it appears
        int was = 0; for (int i = 0; i < NK; i++) was |= seen[i].w != 0;
        if (was) for (int i = 0; i < NK; i++) atomic_store(&g_pend[i], -1);
        memset(seen, 0, sizeof seen); return; }
    load_once(); if (atomic_exchange(&g_repaint, 0)) memset(seen, 0, sizeof seen);
    if (atomic_load(&g_lang_explicit) < 0) atomic_store(&g_val[0], !dm_lang_zh());   // 语言那一格没点过应用：生效值 = 此刻实际在跑的语言（读盘时游戏还没设语言，那时取到的不准） | Apply never tapped for the language cell: effective value = the language actually running right now (when read from disk the game has not set the language yet, so the value fetched then is not accurate)
    // 手柄模式：原版选项页的焦点只认表格里它自己的行，够不着我们的勾选框——十字键上下选行（原版手柄选中框 TouchCustomSel 框住这一行）、A 翻勾选（下面照「玩家点了」处理），其余键照旧归原版
    // Controller mode: the original options page's focus only recognizes its own rows in the table and cannot reach our checkboxes. D-pad up/down selects a row
    // (the original controller selection frame TouchCustomSel frames that row), A toggles the checkbox (handled below as "the player tapped it"), and all other
    // buttons go to the original as before
    { int dm_padmode_current(void); uint64_t dm_padmode_page_keys(uint64_t); static int sel; static dm_wref sel_sp; int pad = dm_padmode_current();
      { uint64_t cm = find_w(root, "ConfirmationModal", 1); if (cm && *(uint8_t *)(cm + 0x50)) pad = 0; }   // 原版「设定已更改，应用 / 放弃」确认框开着：键全还给它（实测：不让的话 A 落在我们这一行上、点不到「应用」） | The original "Settings changed, Apply / Discard" confirmation box is open: hand all keys back to it (measured: otherwise A lands on our row and "Apply" cannot be reached)
      if (dm_wref_get(&sel_sp) != sp) { dm_wref_set(&sel_sp, sp); sel = 0; }
      uint64_t dm = find_w(sp, "TouchCustomSel", 0), tw = 0, cw = 0; char nm[64];
      if (pad) { uint64_t ev = dm_padmode_page_keys(DM_BTN_UP | DM_BTN_DOWN | DM_BTN_A | DM_BTN_LEFT | DM_BTN_RIGHT);
          if (ev & DM_BTN_DOWN) sel = sel + 1 < NK ? sel + 1 : sel; else if (ev & DM_BTN_UP) sel = sel > 0 ? sel - 1 : 0;
          snprintf(nm, sizeof nm, "TouchCustom_%s", k_tab[sel].key); cw = find_w(sp, nm, 0); snprintf(nm, sizeof nm, "TouchCustom_%s_Text", k_tab[sel].key); tw = find_w(sp, nm, 0);
          if (sel_n(sel) && (ev & (DM_BTN_LEFT | DM_BTN_RIGHT | DM_BTN_A))) { char a[48]; snprintf(a, sizeof a, "TouchCustom_%s%s", k_tab[sel].key, (ev & DM_BTN_LEFT) ? "_L" : ""); dm_custom_on_uimsg("PanelManager", "ClosePanel", a); ev &= ~(uint64_t)DM_BTN_A; }   // 选择行：十字键左右 / A 走一格（A = 向右） | selector row: D-pad left/right / A step once (A = right)
          if ((ev & DM_BTN_A) && cw && dm_wref_get(&seen[sel]) == cw) {
              if (sel == 0) { int p0 = atomic_load(&g_pend[0]), sh = p0 >= 0 ? p0 : atomic_load(&g_val[0]); atomic_store(&g_pend[0], !sh); set_toggle(cw, !sh); { uint64_t z = find_w(sp, "TouchCustom_lang_zh", 0); if (z) set_toggle(z, sh); }   // 语言行是单选的两项：两个框当场摆好（只改待定值的话，下面读框状态会把没跟上的框当成玩家点的） | The language row is a single choice between two items: both boxes are set on the spot (if only the pending value were changed, the code below that reads the box states would take the box that has not caught up as a player tap)
                  LOG("自定义设置：语言 → %s（手柄 A 在两项间切换；点「应用」生效）", !sh ? "English" : "中文"); }
              else set_toggle(cw, !TOGGLED(cw)); } }
      if (dm) { int32_t *r = (int32_t *)(dm + 0x70), want[4] = { 99999, 0, r[2], r[3] };   /* 不用时只把 x 挪出屏：它在滚动区里，y 放远了会撑大可滚范围 | when unused only x is moved off screen: it is inside the scroll area, and a far-away y would enlarge the scrollable range */
          if (dm_padmode_current() && tw && cw) { int32_t *t = (int32_t *)(tw + 0x70), *c = (int32_t *)(cw + 0x70); int top = t[1] < c[1] ? t[1] : c[1], bot = t[1] + t[3] > c[1] + c[3] ? t[1] + t[3] : c[1] + c[3];   // 框住整行：文字与勾选框两者的上下沿取并，再各放 10 单位（原先只按文字框的高度算，文字框比字矮，框成了横穿文字中线的一条） | Frame the whole row: take the union of the top and bottom edges of the text and the checkbox, then pad 10 units on each side (previously computed from the text box height only; the text box is shorter than the glyphs, so the frame became a strip cutting across the middle of the text)
              want[0] = t[0] - 30; want[1] = top - 10; want[2] = c[0] + c[2] + 30 - want[0]; want[3] = bot - top + 20;
              { static int lg; if (lg++ < 2) LOG("自定义设置：手柄选中框 文字框 %d,%d %dx%d 勾选框 %d,%d %dx%d → 框 %d,%d %dx%d", t[0], t[1], t[2], t[3], c[0], c[1], c[2], c[3], want[0], want[1], want[2], want[3]); } }
          if (memcmp(r, want, sizeof want)) { memcpy(r, want, sizeof want); ((void (*)(uint64_t))*(uint64_t *)(*(uint64_t *)dm + 0x10))(dm); } }
      // 这页可以滚（各行在原版滚动区里，touch_layout.py _custom_page）：手柄选到列表框外的行时，让原版滚动区自己滚到位——发滚轮，每 4 帧一格，直到这一行整行在框内。
      // 已滚的量 = 内层视图（行容器的父控件）矩形 y 的相反数；框高 = 滚动区（再上一级）的高。只读这两个数，不写原版控件
      // This page can scroll (the rows are in the original scroll area, touch_layout.py _custom_page): when the controller selects a row outside the list box, let the original scroll area scroll into place by itself -- send wheel events, one notch every 4 frames, until the whole row is inside the box.
      // Amount scrolled = the negative of the rect y of the inner view (the parent widget of the row container); box height = the height of the scroll area (one level further up). Only these two numbers are read; the original game's widgets are not written
      if (pad && tw && cw) { uint64_t rw = find_w(sp, "TouchCustomRows", 0), view = rw ? *(uint64_t *)(rw + 0x30) : 0, sv = view ? *(uint64_t *)(view + 0x30) : 0; static uint64_t lf; uint64_t dm_guest_frame(void); uint64_t f = dm_guest_frame();
          if (sv && f - lf >= 4) { int32_t *t = (int32_t *)(tw + 0x70), *c = (int32_t *)(cw + 0x70); int top = (t[1] < c[1] ? t[1] : c[1]) - 10, bot = (t[1] + t[3] > c[1] + c[3] ? t[1] + t[3] : c[1] + c[3]) + 10;
              int off = -*(int32_t *)(view + 0x74), vh = *(int32_t *)(sv + 0x7c); if (vh <= 0) vh = 1260; void dm_ui_wheel_on(uint64_t, int); if (sel == 0) top = 0;   /* 选回第一行：一直滚到最顶，把标题行露出来 | selection back on the first row: scroll all the way to the top so that the header row is revealed */
              if (top < off) { lf = f; dm_ui_wheel_on(sv, 1); } else if (bot > off + vh) { lf = f; dm_ui_wheel_on(sv, -1); } } } }
    for (int i = 0; i < NK; i++) { char nm[48]; snprintf(nm, sizeof nm, "TouchCustom_%s", k_tab[i].key);
        uint64_t w = find_w(sp, nm, 0); if (!w || !*(uint8_t *)(w + 0x50)) continue;
        int pd0 = atomic_load(&g_pend[i]), want = pd0 >= 0 ? pd0 : atomic_load(&g_val[i]);
        if (sel_n(i)) {   // 选择行：值写进中间的文字控件（点箭头在 dm_custom_on_uimsg 里收） | Selector row: the value goes into the middle text widget (arrow taps are handled in dm_custom_on_uimsg)
            char vn[48], vt[48]; snprintf(vn, sizeof vn, "TouchCustom_%s_Val", k_tab[i].key); sel_text(i, want, vt, sizeof vt); set_text(find_w(sp, vn, 0), vt);
            continue; }
        uint64_t z = i == 0 ? find_w(sp, "TouchCustom_lang_zh", 0) : 0;   // 语言行：并排的「中文」「English」两项单选，存的仍是一个键 | Language row: side-by-side 「中文」/「English」 radio, still one key
        if (dm_wref_get(&seen[i]) != w) { dm_wref_set(&seen[i], w); set_toggle(w, want); if (z) set_toggle(z, !want); continue; }   // 页刚出现（布局里一律是没勾）：先摆成该显示的值，这一帧不当玩家点了 | The page has just appeared (in the layout everything is unchecked): first set it to the value it should show, and do not treat this frame as a player tap
        int on = TOGGLED(w), pd = atomic_load(&g_pend[i]), shown = pd >= 0 ? pd : atomic_load(&g_val[i]);
        if (z) {   // 点没选中的那一项 = 选它；点已选中的那一项不变（摆回勾上） | Tapping the item that is not selected = select it; tapping the one already selected changes nothing (it is put back to checked)
            int zon = TOGGLED(z), to = on != shown ? (on ? 1 : shown) : zon != !shown ? (zon ? 0 : shown) : shown;
            if (to != shown) { atomic_store(&g_pend[i], to); LOG("自定义设置：语言 → %s（点「应用」生效）", to ? "English" : "中文"); }
            if (TOGGLED(w) != to) set_toggle(w, to); if (TOGGLED(z) != !to) set_toggle(z, !to); continue; }
        if (on != shown) { atomic_store(&g_pend[i], on); LOG("自定义设置：%s 勾选框 → %s（点「应用」生效）", k_tab[i].key, on ? "勾上" : "取消"); } }
    // 语言那一行下面的「点应用后把 App 关掉重开」提示（TouchCustomLangHint，布局里先放在屏外）：勾选框此刻显示的值与正在跑的语言不一致时挪到标签正下方（横坐标取标签的）
    // The "tap Apply, then close and reopen the app" hint under the language row (TouchCustomLangHint, placed off screen in the layout): moved right under the label (taking the label's x) when the value the checkboxes currently show differs from the language now running
    { uint64_t lw = find_w(sp, "TouchCustom_lang_en_Text", 0); int pd = atomic_load(&g_pend[0]), shown = pd >= 0 ? pd : atomic_load(&g_val[0]), diff = shown != !dm_lang_zh();
      for (int k = 0; k < 2 && lw; k++) { uint64_t hw = find_w(sp, k ? "TouchCustomLangHintEn" : "TouchCustomLangHintZh", 0); if (!hw) continue;   // 提示用「选中的那种语言」写：选了中文显示中文那句，选了 English 显示英文那句 | The hint is written in "the language that was chosen": choosing 中文 shows the Chinese sentence, choosing English shows the English one
          int x = diff && shown == k ? ((int32_t *)(lw + 0x70))[0] : 99999; int32_t *r = (int32_t *)(hw + 0x70);
          if (r[0] != x) { r[0] = x; ((void (*)(uint64_t))*(uint64_t *)(*(uint64_t *)hw + 0x10))(hw); } } }
    // 「应用 / 放弃」亮不亮（自定义页改了勾选，「应用」仍是灰的）：原版各选项子页的每帧更新 main+0xae5f28 把自己 +0x1a0「有未应用的改动」
    // 交给 main+0xaed9e4(设置页, 亮否)，按钮 +0x57a 是灰的标志；原版的勾选框一改就由面板置 +0x1a0，我们的勾选框原版面板不认识，这一位一直是 0。
    // 按待定值与生效值比（改回原样 = 没改动 = 灭）写这一位，走原版自己的亮灭与「有改动离开要确认」。只在面板虚表里确有 0xae5f28 时写（确认 +0x1a0 是这个意思）
    // Whether 「应用 / 放弃」(Apply / Discard) light up (a checkbox on the custom page was changed, yet 「应用」(Apply) is still gray): the per-frame update main+0xae5f28 of each original-game option subpage passes its own +0x1a0 'has unapplied changes'
    // to main+0xaed9e4(settings page, lit or not), and button +0x57a is the gray flag; an original-game checkbox has +0x1a0 set by the panel as soon as it changes, but the original panel does not know our checkboxes, so this bit stays 0.
    // Write this bit by comparing pending values with effective values (changed back to the original = no change = off), using the original game's own lit / unlit behavior and its 'confirm when leaving with changes'. Written only when the panel vtable really contains 0xae5f28 (confirming that +0x1a0 has this meaning)
    static uint64_t vt_ok_for; static int vt_ok; if (vt_ok_for != *(uint64_t *)sp) { vt_ok_for = *(uint64_t *)sp; vt_ok = 0;
        for (int k = 0; k < 96 && !vt_ok; k++) vt_ok = *(uint64_t *)(vt_ok_for + 8 * k) == (uint64_t)main_base() + 0xae5f28;
        if (!vt_ok) LOG("【红】自定义设置：借来的视频页虚表里没有 main+0xae5f28，不写「有改动」位（原版版本变了？）"); }
    if (vt_ok) { int dirty = 0; for (int i = 0; i < NK; i++) { int pd = atomic_load(&g_pend[i]); dirty |= pd >= 0 && pd != atomic_load(&g_val[i]); }
        if (*(uint8_t *)(sp + 0x1a0) != dirty) { *(uint8_t *)(sp + 0x1a0) = (uint8_t)dirty; LOG("自定义设置：有未应用的改动 → %d（「应用 / 放弃」%s）", dirty, dirty ? "点亮" : "变灰"); } }
}
