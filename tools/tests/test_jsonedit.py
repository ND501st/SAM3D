"""tools/recon/osa/jsonedit.py: list items removed and inserted in a hand-formatted JSON file, its layout and line
ends kept, the result still JSON. The cabin data is edited this way (panel_apply.py, flat_snap.py)."""
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, 'tools', 'recon', 'osa'))
import jsonedit as J  # noqa: E402

DOC = '{\r\n  "items": [\r\n    {"panel": "a", "n": 0},\r\n    {"panel": "b", "n": 1},\r\n    {"panel": "b", "n": 2},\r\n    {"panel": "b", "n": 3}\r\n  ],\r\n  "x": 1\r\n}\r\n'


def test_remove_the_list_tail_leaves_json():
    """The bug: the list's last items removed one by one left the comma of the one before them."""
    out = J.remove_items(DOC, ['items'], lambda it: it['panel'] == 'b')
    assert json.loads(out) == {'items': [{'panel': 'a', 'n': 0}], 'x': 1}
    assert '\r\n' in out and '\n' not in out.replace('\r\n', '')


def test_remove_from_the_middle_and_the_head():
    out = J.remove_items(DOC, ['items'], lambda it: it['n'] in (0, 2))
    assert [it['n'] for it in json.loads(out)['items']] == [1, 3]


def test_insert_before_and_at_the_end():
    out = J.insert_items(DOC, ['items'], ['{"panel": "c", "n": 9}'], before=1)
    assert [it['n'] for it in json.loads(out)['items']] == [0, 9, 1, 2, 3]
    out = J.insert_items(DOC, ['items'], ['{"panel": "c", "n": 9}'])
    assert [it['n'] for it in json.loads(out)['items']] == [0, 1, 2, 3, 9]


def test_set_value_keeps_the_rest():
    out = J.set_value(DOC, ['items', {'n': 2}, 'n'], 7.0, 0)
    assert json.loads(out)['items'][2]['n'] == 7
    assert out.replace('7', '2') == DOC
