#!/usr/bin/env python3
"""Parse a Wallpaper Engine scene.pkg: list entries, optionally extract."""
import struct, sys, os, json

def u32(b, o): return struct.unpack_from('<I', b, o)[0], o + 4

def sized_str(b, o):
    n, o = u32(b, o)
    return b[o:o+n].decode('utf-8'), o + n

def parse(b):
    magic, o = sized_str(b, 0)
    count, o = u32(b, o)
    ents = []
    for _ in range(count):
        name, o = sized_str(b, o)
        off, o = u32(b, o)
        size, o = u32(b, o)
        ents.append({'name': name, 'offset': off, 'size': size})
    data_start = o
    for e in ents:
        e['abs'] = data_start + e['offset']
        assert e['abs'] + e['size'] <= len(b), e
    return {'magic': magic, 'count': count, 'data_start': data_start, 'entries': ents}

if __name__ == '__main__':
    path = sys.argv[1]
    b = open(path, 'rb').read()
    p = parse(b)
    end = max(e['abs'] + e['size'] for e in p['entries'])
    print(json.dumps({'file': path, 'magic': p['magic'], 'count': p['count'],
                      'data_start': p['data_start'], 'file_size': len(b), 'last_entry_end': end}))
    if len(sys.argv) > 2:
        out = sys.argv[2]
        for e in p['entries']:
            dst = os.path.join(out, e['name'])
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, 'wb').write(b[e['abs']:e['abs'] + e['size']])
