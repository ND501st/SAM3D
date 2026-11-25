"""Fetch the Sketchfab models the register draws (tools/sketchfab/models.json, data/scenery/assets.json) onto this
machine, through Sketchfab's download API, with your own account's API token:

    set SKETCHFAB_API_TOKEN=...   (sketchfab.com > Settings > Password & API; never commit it)
    python tools/sketchfab_fetch.py [--all | <asset id> ...] [--out art/thirdparty/sketchfab]

Each model's GLB goes to <out>/<id>/source.glb (art/thirdparty/, ignored by git), checked against the register's
sha256 when it records one: a different file is kept as source.glb.new and reported, not used. Then prepare and import
them: tools/blender/sketchfab_prepare.py, tools/ue_import_sketchfab.py (docs/scenery/ASSETS.md).

Only models whose page still says a Creative Commons licence and downloadable are fetched; the token is read from the
environment and sent only to api.sketchfab.com.
"""
import argparse
import hashlib
import json
import os
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = 'https://api.sketchfab.com/v3/models/'
AGENT = 'sam3d-asset-fetch/1.0'
LICENCES = {'CC Attribution': 'CC-BY-4.0', 'CC0 Public Domain': 'CC0-1.0'}


def get_json(url, token=None):
    headers = {'User-Agent': AGENT}
    if token:
        headers['Authorization'] = 'Token ' + token
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as r:
        return json.load(r)


def download(url, dest):
    h = hashlib.sha256()
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': AGENT}), timeout=600) as r, \
            open(dest + '.part', 'wb') as f:
        while True:
            b = r.read(1 << 20)
            if not b:
                break
            h.update(b)
            f.write(b)
    return h.hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('ids', nargs='*')
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--out', default=os.path.join(REPO, 'art', 'thirdparty', 'sketchfab'))
    args = ap.parse_args(argv)
    token = os.environ.get('SKETCHFAB_API_TOKEN', '')
    if not token:
        print('SKETCHFAB_API_TOKEN is not set (your account: sketchfab.com > Settings > Password & API)')
        return 2
    with open(os.path.join(REPO, 'tools', 'sketchfab', 'models.json'), encoding='utf-8') as f:
        specs = json.load(f)['models']
    with open(os.path.join(REPO, 'data', 'scenery', 'assets.json'), encoding='utf-8') as f:
        sha = {a['id']: a.get('sha256', '') for a in json.load(f)['assets']}
    wanted = [s for s in specs if args.all or s['id'] in args.ids]
    if not wanted:
        print('nothing to fetch: name asset ids (tools/sketchfab/models.json) or --all')
        return 2
    bad = 0
    for s in wanted:
        dest_dir = os.path.join(args.out, s['id'])
        dest = os.path.join(dest_dir, 'source.glb')
        expected = sha.get('sketchfab_' + s['id'], '')
        if os.path.exists(dest) and expected and hashlib.sha256(open(dest, 'rb').read()).hexdigest() == expected:
            print('have', s['id'])
            continue
        meta = get_json(API + s['uid'])
        licence = LICENCES.get((meta.get('license') or {}).get('label', ''))
        if licence is None or not meta.get('isDownloadable'):
            print('SKIPPED', s['id'], ': its page no longer gives a Creative Commons download', s['page'])
            bad += 1
            continue
        links = get_json(API + s['uid'] + '/download', token)
        if 'glb' not in links:
            print('SKIPPED', s['id'], ': no GLB offered')
            bad += 1
            continue
        os.makedirs(dest_dir, exist_ok=True)
        got = download(links['glb']['url'], dest)
        if expected and got != expected:
            os.replace(dest + '.part', dest + '.new')
            print('CHANGED', s['id'], ': the file differs from the one registered (kept as source.glb.new, not used)')
            bad += 1
            continue
        os.replace(dest + '.part', dest)
        print('fetched', s['id'], got)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
