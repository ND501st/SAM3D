"""Change single values in a hand-formatted JSON file without touching anything else: its layout, comments-in-notes
and line ends stay as they are (data/cabins/osa_ak_shell.json does not survive json.dumps).

    text = read(path)
    text = shift(text, ['floor', 'trough_right', 'x_from_m'], 0.302)          # a number moved by an amount
    text = set_value(text, ['panels', {'id': 'sp2m_panel'}, 'centre_m', 0], 0.914)
    text = append_to_string(text, ['panels', {'id': 'sp2m_panel'}, 'notes'], ' More.')
    write(path, text)

A path is keys and indices; a dict in it picks the list item whose fields equal the dict's.
"""
import json


def read(path):
    with open(path, encoding='utf-8', newline='') as f:
        return f.read()


def write(path, text):
    json.loads(text)                          # still JSON, or nothing is written
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(text)


def _skip_ws(s, i):
    while s[i] in ' \t\r\n':
        i += 1
    return i


def _scan(s, i, path, out):
    """Parse the value at s[i:], recording every scalar's span and every container's children. Returns the index after it."""
    i = _skip_ws(s, i)
    c = s[i]
    if c == '{':
        out[path] = ('object', i, None)
        i = _skip_ws(s, i + 1)
        while s[i] != '}':
            j = i
            i = _string_end(s, i)
            key = json.loads(s[j:i])
            i = _skip_ws(s, i)
            assert s[i] == ':'
            i = _scan(s, i + 1, path + (key,), out)
            i = _skip_ws(s, i)
            if s[i] == ',':
                i = _skip_ws(s, i + 1)
        out[('$end',) + path] = i + 1
        return i + 1
    if c == '[':
        out[path] = ('array', i, None)
        i = _skip_ws(s, i + 1)
        n = 0
        while s[i] != ']':
            i = _scan(s, i, path + (n,), out)
            n += 1
            i = _skip_ws(s, i)
            if s[i] == ',':
                i = _skip_ws(s, i + 1)
        out[path] = ('array', out[path][1], n)
        out[('$end',) + path] = i + 1
        return i + 1
    if c == '"':
        j = _string_end(s, i)
        out[path] = ('string', i, j)
        return j
    j = i
    while s[j] not in ',]} \t\r\n':
        j += 1
    out[path] = ('scalar', i, j)
    return j


def _string_end(s, i):
    assert s[i] == '"'
    i += 1
    while s[i] != '"':
        i += 2 if s[i] == '\\' else 1
    return i + 1


def spans(text):
    out = {}
    _scan(text, 0, (), out)
    return out


def resolve(text, path, sp=None):
    """A path with dict selectors as a plain path of keys and indices."""
    sp = sp or spans(text)
    doc = json.loads(text)
    plain, node = [], doc
    for step in path:
        if isinstance(step, dict):
            hits = [i for i, item in enumerate(node) if isinstance(item, dict) and all(item.get(k) == v for k, v in step.items())]
            if len(hits) != 1:
                raise KeyError('%s matches %d items' % (step, len(hits)))
            step = hits[0]
        plain.append(step)
        node = node[step]
    return tuple(plain), node, sp


def _decimals(token):
    return len(token.split('.')[1]) if '.' in token and 'e' not in token.lower() else 0


def set_value(text, path, value, decimals=None):
    plain, old, sp = resolve(text, path)
    kind, a, b = sp[plain]
    if kind != 'scalar':
        raise TypeError('%s is not a number' % (path,))
    d = max(3, _decimals(text[a:b])) if decimals is None else decimals
    return text[:a] + ('%.*f' % (d, value)) + text[b:]


def shift(text, path, by, decimals=None):
    plain, old, sp = resolve(text, path)
    return set_value(text, list(plain), float(old) + by, decimals)


def shift_column(text, path, column, by):
    """Every row of a list of rows: the number in one column moved by an amount."""
    plain, rows, sp = resolve(text, path)
    for i in reversed(range(len(rows))):          # from the end, so earlier spans stay good
        text = shift(text, list(plain) + [i, column], by)
    return text


def append_to_string(text, path, more):
    plain, old, sp = resolve(text, path)
    kind, a, b = sp[plain]
    if kind != 'string':
        raise TypeError('%s is not a string' % (path,))
    return text[:b - 1] + json.dumps(more, ensure_ascii=False)[1:-1] + text[b - 1:]


def _span(text, plain, sp):
    kind, a, b = sp[plain]
    return a, (sp[('$end',) + plain] if kind in ('object', 'array') else b)


def replace_value(text, path, raw):
    """The value at a path (a number, a string, a list or an object) replaced by raw JSON text, laid out as given."""
    plain, old, sp = resolve(text, path)
    a, b = _span(text, plain, sp)
    return text[:a] + raw + text[b:]


def add_key(text, path, key, raw):
    """A key added at the end of the object at a path, on the same line as its last value."""
    plain, old, sp = resolve(text, path)
    a, b = _span(text, plain, sp)
    j = b - 1                                      # the closing brace
    k = j
    while text[k - 1] in ' \t\r\n':
        k -= 1
    return text[:k] + ', ' + json.dumps(key) + ': ' + raw + text[k:]


def _line_start(text, i):
    return text.rfind('\n', 0, i) + 1


def _line_end(text, i):
    j = text.find('\n', i)
    return len(text) if j < 0 else j + 1


def remove_items(text, path, pred):
    """Every item of the list at a path for which pred(item) holds, removed with its lines. Each item must start and
    end its own lines (the cabin file's one item per line, or per few lines)."""
    plain, items, sp = resolve(text, path)
    gone = [i for i, it in enumerate(items) if pred(it)]
    for i in reversed(gone):
        a, b = _span(text, plain + (i,), sp)
        last = i == len(items) - 1 or all(k in gone for k in range(i + 1, len(items)))
        s, e = _line_start(text, a), _line_end(text, b)
        text = text[:s] + text[e:]
        if last and i > 0:                        # removing from the end (in reverse): the item before ends the list now
            pa, pb = _span(text, plain + (i - 1,), sp)
            if text[pb] == ',':
                text = text[:pb] + text[pb + 1:]
        sp = spans(text)
        plain, items_now, sp = resolve(text, list(plain), sp)
    return text


def insert_items(text, path, raws, before=None):
    """Raw JSON items inserted into the list at a path, one per line, before the item at index `before` (default: at
    the end), indented as the list's items are."""
    plain, items, sp = resolve(text, path)
    if not raws:
        return text
    n = len(items)
    ref = (before if before is not None and before < n else n - 1)
    a, b = _span(text, plain + (ref,), sp)
    s = _line_start(text, a)
    indent = text[s:a]
    nl = '\r\n' if '\r\n' in text else '\n'
    if before is not None and before < n:
        block = ''.join(indent + r + ',' + nl for r in raws)
        return text[:s] + block + text[s:]
    # at the end: after the last item, which gains a comma
    e = b
    block = ''.join(',' + nl + indent + r for r in raws)
    return text[:e] + block + text[e:]
