"""自定义页选择行的跨语言契约：换组键位置这一行由「◀ 值 ▶」构成，宿主键表顺序与页面行序一致。 | Cross-language contract for the Custom page selector rows: the skill-set switch position row is "◀ value ▶", and the host key table follows the page row order. | Cross-language contract for the Custom page selector rows: the skill-set switch position row is "◀ value ▶", and the host key table follows the page row order."""
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'ios/tools'))
import touch_layout as layout

out, _ = layout._custom_page('"name": "VideoOptionsPanel", "name": "OptionsTable"')
page = json.loads(re.sub(r',\s*([}\]])', r'\1', out))
nodes = {}
def walk(node):
    if isinstance(node, dict):
        if 'name' in node:
            assert node['name'] not in nodes, node['name']
            nodes[node['name']] = node
        for child in node.values():
            walk(child)
    elif isinstance(node, list):
        for child in node:
            walk(child)
walk(page)
# 左右两个箭头都是原版大厅的同款小箭头（ButtonWidget），右侧那颗占勾选框那一列，点击发 ClosePanel 给宿主 | both arrows are the original lobby small arrows (ButtonWidget); the right one sits in the checkbox column and sends ClosePanel to the host
for name in ('TouchCustom_touch_swap_above_L', 'TouchCustom_touch_swap_above'):
    assert nodes[name]['type'] == 'ButtonWidget'
    assert nodes[name]['fields']['onClickMessage'].endswith(name)
    assert nodes[name]['fields']['filename'] == 'Lobby/Final/LobbyArrowSmall' + ('L' if name.endswith('_L') else 'R')
assert 'TouchCustom_touch_swap_above_Text' in nodes and 'TouchCustom_touch_swap_above_Val' in nodes
assert nodes['TouchCustom_touch_swap_above_Text']['fields']['text'] == '@dmCustomTouchSwapAbove'
# 两种取值的中英文字都在表里（宿主 sel_text 用 dm_str 取） | both values' bilingual strings are in the table (the host's sel_text uses dm_str)
tsv = (root / 'i18n/ui_text.tsv').read_text()
for suffix in ('Near', 'Far'):
    assert ('dmCustomTouchSwap' + suffix + '\t') in tsv
# 宿主键表：顺序 = 页面行序（去掉 '#分节' 与 '!动作' 行），换组默认 0（近位） | host key table: same order as the page rows (without '#section' / '!action' rows), switch default 0 (near)
host = (root / 'host/dm_custom.c').read_text()
keys = re.findall(r'\{\s*"([a-z_0-9]+)",\s*(-?\d+),\s*(\d+)\s*\}', re.search(r'k_tab\[\]\s*=\s*\{(.*?)\};', host, re.S)[1])
assert [k for k, _, _ in keys] == [key for key, _ in layout._CUSTOM_ALL if not key.startswith(('#', '!'))]
assert [(int(d), int(n)) for _, d, n in keys][[k for k, _, _ in keys].index('touch_swap_above')] == (0, 2)
print('PASS touch layout options: generated selector row, lobby arrows, labels, host order and default')
