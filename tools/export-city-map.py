#!/usr/bin/env python3
"""Genera el mapa de la ciudad del juego original para el port C++ (sin Node).

Reproduce paso a paso src/world/bogota/{StreetLayout,CityLayout,LakePark,GasStation,D69Store,Colpatria,
ColpatriaViaduct,SmallBuilding,TransitTerminal}.js y la funcion _layout de src/world/districts/Bogota.js, y escribe
cpp/assets/data/city-map.json con la rejilla de 120 x 198 baldosas (37 filas de la estacion al norte + las 161 del
original), las bandas de calles y las posiciones reservadas.  Uso: python tools/export-city-map.py
"""
import json
import math
import pathlib

TILE = 2.0

# ---------------------------------------------------------------- StreetLayout.js
ORIGINAL_SIZE = 48
ORIGINAL_CARRERAS = [[6, 7, 'Carrera 15'], [40, 41, 'Carrera 10']]
ORIGINAL_CARACAS = {'lanes': [[22, 23], [26, 27]], 'busway': [24, 25]}
ORIGINAL_CALLES = [[12, 13, 'Calle 10'], [30, 31, 'Calle 9']]
SIDEWALK_TILES = 2


def js_round(value):
    return math.floor(value + 0.5)


def axis(bands, sidewalk_width=SIDEWALK_TILES):
    def is_road(i):
        return any(a <= i <= b for a, b, *_ in bands)
    widths = [3 if is_road(i) else sidewalk_width if (is_road(i - 1) or is_road(i + 1)) else 1 for i in range(ORIGINAL_SIZE)]
    starts, size = [], 0
    for width in widths:
        starts.append(size)
        size += width

    def tile(value):
        i = max(0, min(ORIGINAL_SIZE - 1, math.floor(value + 0.5)))
        return starts[i] - 0.5 + (value - i + 0.5) * widths[i]

    def band(b):
        return [starts[b[0]], starts[b[1]] + widths[b[1]] - 1, *b[2:]]
    return {'widths': widths, 'size': size, 'tile': tile, 'band': band, 'world': lambda value: tile(value) * TILE}


vertical_bands = [*ORIGINAL_CARRERAS, *ORIGINAL_CARACAS['lanes'], ORIGINAL_CARACAS['busway']]
STREET_X = axis(vertical_bands)
STREET_Z = axis(ORIGINAL_CALLES)
CARRERAS = [STREET_X['band'](b) for b in ORIGINAL_CARRERAS]
CALLES = [STREET_Z['band'](b) for b in ORIGINAL_CALLES]
CARACAS = {'lanes': [STREET_X['band'](b) for b in ORIGINAL_CARACAS['lanes']], 'busway': STREET_X['band'](ORIGINAL_CARACAS['busway'])}
WX, WZ = STREET_X['world'], STREET_Z['world']

# ---------------------------------------------------------------- CityLayout.js
CITY_WIDTH, CITY_HEIGHT = 120, 161
ORIGINAL_CITY_WIDTH, ORIGINAL_CITY_HEIGHT = STREET_X['size'], STREET_Z['size']
CITY_CARRERAS = [*CARRERAS, [84, 89, 'Carrera 8'], [106, 111, 'Carrera 6']]
CITY_CALLES = [*CALLES, [72, 77, 'Calle 8'], [94, 99, 'Calle 7']]
EXTENSION_PARKS = [
    {'x0': 92, 'x1': 103, 'z0': 21, 'z1': 34, 'name': 'Parque de la Estación'},
    {'x0': 69, 'x1': 81, 'z0': 80, 'z1': 91, 'name': 'Parque del Sur'},
]

# ---------------------------------------------------------------- Colpatria.js (rodeo del bus)
BUS_DETOUR = {'z0': 80, 'z1': 260, 'offset': 44}


def clamp(value, low, high):
    return max(low, min(high, value))


def bridge_center(z):
    d = BUS_DETOUR
    t = clamp((z - d['z0']) / (154 - d['z0']) if z < 154 else (d['z1'] - z) / (d['z1'] - 182), 0, 1)
    ease = t * t * t * (10 + t * (-15 + 6 * t))
    return 75 + d['offset'] * ease


def bridge_normal(z):
    dx = (bridge_center(z + .01) - bridge_center(z - .01)) / .02
    scale = math.hypot(1, dx)
    return {'x': 1 / scale, 'z': -dx / scale}


def bus_path_x(z, lane_x=78.5):
    if z <= BUS_DETOUR['z0'] - 5 or z >= BUS_DETOUR['z1'] + 5:
        return lane_x
    offset = lane_x - 75
    lo, hi = z - 10, z + 10
    for _ in range(28):
        t = (lo + hi) / 2
        if t + bridge_normal(t)['z'] * offset < z:
            lo = t
        else:
            hi = t
    t = (lo + hi) / 2
    return bridge_center(t) + bridge_normal(t)['x'] * offset


def bus_heading(z, lane_x=78.5):
    dx = bus_path_x(z - .01, lane_x) - bus_path_x(z + .01, lane_x)
    scale = math.hypot(dx, .02)
    return {'x': dx / scale, 'z': -.02 / scale}


# ---------------------------------------------------------------- viaducto y lago
BRIDGE_RISE = 7.8
LAKE_PARK = {'z0': 222, 'z1': 318, 'lakeX': 75, 'lakeZ': 262, 'radiusX': 50, 'radiusZ': 26}
LAKE_BRIDGE = {'x0': 68, 'x1': 82, 'z0': 220, 'z1': 318}
VIADUCT = {'start': 15.43, 'crestStart': 39, 'join': 220, 'end': LAKE_BRIDGE['z1'], 'width': 14}


def on_lake_bridge(x, z):
    b = LAKE_BRIDGE
    return b['z0'] <= z <= b['z1'] and bus_path_x(z, b['x0']) <= x <= bus_path_x(z, b['x1'])


def edge(z, side):
    normal = bridge_normal(z)
    offset = side * VIADUCT['width'] / 2
    return (bridge_center(z) + normal['x'] * offset, z + normal['z'] * offset)


# ---------------------------------------------------------------- Bogota.js _layout
CARTUCHO = {'x0': 8, 'x1': 21, 'z0': 14, 'z1': 29}


def layout_center():
    S = ORIGINAL_SIZE
    m = [['building'] * S for _ in range(S)]

    def put(x, z, value):
        if 0 <= x < S and 0 <= z < S:
            m[z][x] = value
    for i in range(S):
        put(i, 0, 'wall'); put(i, S - 1, 'wall'); put(0, i, 'wall'); put(S - 1, i, 'wall')
    for i in range(1, S - 1):
        for a, b, *_ in ORIGINAL_CARRERAS:
            for x in range(a, b + 1): put(x, i, 'road')
        for a, b in ORIGINAL_CARACAS['lanes']:
            for x in range(a, b + 1): put(x, i, 'road')
        for x in range(ORIGINAL_CARACAS['busway'][0], ORIGINAL_CARACAS['busway'][1] + 1): put(x, i, 'busway')
        for a, b, *_ in ORIGINAL_CALLES:
            for z in range(a, b + 1): put(i, z, 'road')
    for a, b, *_ in ORIGINAL_CALLES:
        for z in range(a, b + 1):
            for x in range(ORIGINAL_CARACAS['busway'][0], ORIGINAL_CARACAS['busway'][1] + 1): put(x, z, 'road')
    street = lambda v: v in ('road', 'busway')
    for z in range(1, S - 1):
        for x in range(1, S - 1):
            if m[z][x] != 'building':
                continue
            near = any(street(m[z + dz][x + dx]) for dz in (-1, 0, 1) for dx in (-1, 0, 1))
            if near:
                m[z][x] = 'sidewalk'
    c = CARTUCHO
    for z in range(c['z0'] + 1, c['z1']):
        for x in range(c['x0'] + 1, c['x1']):
            if m[z][x] == 'building': m[z][x] = 'ruin'
            if z in (21, 22): m[z][x] = 'dirt'
            if 12 <= x <= 17 and 18 <= z <= 25: m[z][x] = 'dirt'
    for z in range(33, 36):
        for x in range(13, 17): m[z][x] = 'floor'
    for z in range(33, 46):
        for x in range(29, 39): m[z][x] = 'plaza' if (x == 33 or x == 34 or z == 39) else 'grass'
    for z in range(33, 46):
        for x in range(43, 47): m[z][x] = 'dirt'
    return m


def expand_streets(m):
    rows = []
    for z, row in enumerate(m):
        expanded = []
        for x, value in enumerate(row):
            expanded.extend([value] * STREET_X['widths'][x])
        for _ in range(STREET_Z['widths'][z]):
            rows.append(list(expanded))
    return rows


def expand_city(center):
    H, W = CITY_HEIGHT, CITY_WIDTH
    m = [['building'] * W for _ in range(H)]
    original = lambda x, z: x < ORIGINAL_CITY_WIDTH - 1 and z < ORIGINAL_CITY_HEIGHT - 1
    for z in range(ORIGINAL_CITY_HEIGHT - 1):
        for x in range(ORIGINAL_CITY_WIDTH - 1):
            m[z][x] = center[z][x]
    for z in range(1, H - 1):
        for a, b, *_ in [*CITY_CARRERAS, *CARACAS['lanes']]:
            for x in range(a, b + 1):
                if not original(x, z): m[z][x] = 'road'
        for x in range(CARACAS['busway'][0], CARACAS['busway'][1] + 1):
            if not original(x, z): m[z][x] = 'busway'
    for a, b, *_ in CITY_CALLES:
        for z in range(a, b + 1):
            for x in range(1, W - 1):
                if not original(x, z): m[z][x] = 'road'
    street = lambda v: v in ('road', 'busway')

    def at(x, z):
        return m[z][x] if 0 <= z < H and 0 <= x < W else None
    for z in range(1, H - 1):
        for x in range(1, W - 1):
            if original(x, z) or m[z][x] != 'building':
                continue
            near = False
            for dz in range(-SIDEWALK_TILES, SIDEWALK_TILES + 1):
                for dx in range(-SIDEWALK_TILES, SIDEWALK_TILES + 1):
                    if street(at(x + dx, z + dz)):
                        near = True
                        break
                if near:
                    break
            if near: m[z][x] = 'sidewalk'
    for park in EXTENSION_PARKS:
        path_x = (park['x0'] + park['x1']) // 2
        path_z = (park['z0'] + park['z1']) // 2
        for z in range(park['z0'], park['z1'] + 1):
            for x in range(park['x0'], park['x1'] + 1):
                if m[z][x] != 'building': continue
                m[z][x] = 'plaza' if (x == path_x or z == path_z) else 'grass'
    # paintLakePark
    p = LAKE_PARK
    for z in range(111, len(m) - 1):
        for x in range(1, len(m[0]) - 1):
            wx, wz = x * 2, z * 2
            radius = math.hypot((wx - p['lakeX']) / p['radiusX'], (wz - p['lakeZ']) / p['radiusZ'])
            promenade = abs(radius - 1.14) < .045 or z == 113 or z == 155 or x == 10 or x == 112 or (x == 86 and (wz < 234 or wz > 290))
            m[z][x] = 'water' if radius < 1 else 'plaza' if promenade else 'grass'
            if on_lake_bridge(wx, wz): m[z][x] = 'busway'
    # paintGasStation, paintD69Store
    for z in range(45, 59):
        for x in range(73, 82): m[z][x] = 'floor'
    for z in range(80, 87):
        for x in range(22, 27): m[z][x] = 'storeD69'
    # paintColpatria
    z = BUS_DETOUR['z0']
    while z <= BUS_DETOUR['z1']:
        x = bus_path_x(z)
        h = bus_heading(z)
        along = -9.2
        while along <= 9.2:
            side = -3.0
            while side <= 3.0:
                tx = js_round((x + h['x'] * along - h['z'] * side) / 2)
                tz = js_round((z + h['z'] * along + h['x'] * side) / 2)
                if m[tz][tx] != 'road': m[tz][tx] = 'busway'
                side += .5
            along += .8
        z += .4
    for zz in range(78, 91):
        for xx in range(35, 47): m[zz][xx] = 'colpatria'
    for zz in range(80, 89):
        for xx in range(47, 52): m[zz][xx] = 'colpatriaAnnex'
    # paintViaductClearance
    z = VIADUCT['start']
    while z <= VIADUCT['end']:
        a, b = edge(z, -1), edge(z, 1)
        t = 0.0
        while t <= 1:
            x = js_round(((1 - t) * a[0] + t * b[0]) / 2)
            row = js_round(((1 - t) * a[1] + t * b[1]) / 2)
            if 0 <= row < len(m) and 0 <= x < len(m[0]) and m[row][x] in ('building', 'ruin'): m[row][x] = 'floor'
            t += .04
        z += .5
    # paintSmallBuilding, paintViaductGardens
    for zz in range(60, 66):
        for xx in range(35, 41): m[zz][xx] = 'smallBuilding'
    for zz in range(math.ceil(VIADUCT['crestStart'] / 2), int(math.ceil(VIADUCT['join'] / 2))):
        for xx in range(1, len(m[zz]) - 1):
            if m[zz][xx] == 'busway': m[zz][xx] = 'grass'
    for xx in range(CITY_WIDTH):
        m[0][xx] = m[CITY_HEIGHT - 1][xx] = 'wall'
    for zz in range(CITY_HEIGHT):
        m[zz][0] = m[zz][CITY_WIDTH - 1] = 'wall'
    # paintTerminalEntrance
    for zz in range(0, 8):
        for xx in range(27, 49):
            m[zz][xx] = 'sidewalk' if (xx <= 28 or xx >= 47) else 'busway' if 35 <= xx <= 40 else 'road'
    return m


CODES = {'building': 'b', 'road': 'r', 'busway': 'B', 'sidewalk': 's', 'grass': 'g', 'plaza': 'p', 'wall': 'w', 'water': 'W',
         'floor': 'f', 'dirt': 'd', 'ruin': 'R', 'storeD69': 'S', 'colpatria': 'C', 'colpatriaAnnex': 'A', 'smallBuilding': 'M'}
NORTH_ROWS = 37   # estacion Portal del Centro: Z -74 a -2 del original (TransitTerminal.js)


def main():
    city = expand_city(expand_streets(layout_center()))
    assert len(city) == CITY_HEIGHT and all(len(row) == CITY_WIDTH for row in city)
    # estacion al norte: misma seccion que la entrada (paintTerminalEntrance) y muros fuera de ella
    north = []
    for _ in range(NORTH_ROWS):
        row = []
        for x in range(CITY_WIDTH):
            if 27 <= x <= 48:
                row.append('sidewalk' if (x <= 28 or x >= 47) else 'busway' if 35 <= x <= 40 else 'road')
            else:
                row.append('wall')
        north.append(row)
    grid = north + city
    rows = [''.join(CODES[v] for v in row) for row in grid]

    lane = lambda band, side: ((band[0] + band[1]) / 2 + side * (band[1] - band[0] + 1) / 4) * TILE
    data = {
        'width': CITY_WIDTH, 'height': len(grid), 'northRows': NORTH_ROWS, 'tile': TILE,
        'codes': {v: k for k, v in CODES.items()},
        'rows': rows,
        'carreras': [b[:2] for b in CITY_CARRERAS], 'carrerasNames': [b[2] for b in CITY_CARRERAS],
        'calles': [b[:2] for b in CITY_CALLES], 'callesNames': [b[2] for b in CITY_CALLES],
        'caracasLanes': CARACAS['lanes'], 'caracasBusway': CARACAS['busway'],
        'extensionParks': EXTENSION_PARKS,
        # posiciones del original (metros, marco del original) de Bogota.js
        'reserved': {
            'spawn': [112.98, 97.84], 'bed': [112.98, 94.64],
            'sell': [WX(14.5), WZ(34)], 'dealer': [WX(14.5), WZ(21.5)], 'craft': [WX(16.5), WZ(24.5)], 'shop': [WX(34), WZ(29)],
        },
        'vendors': [
            {'name': 'Chaza de don Efraín', 'x': WX(21), 'z': WZ(18.5), 'rot': math.pi / 2, 'kind': 'chaza', 'items': ['cigarrillo', 'cafe', 'pan', 'agua']},
            {'name': 'Fritanga La Mona', 'x': WX(28), 'z': WZ(29), 'rot': 0, 'kind': 'fritanga', 'items': ['lataComida', 'pan', 'refresco']},
            {'name': 'Dulces y minutos', 'x': WX(39), 'z': WZ(36), 'rot': -math.pi / 2, 'kind': 'chaza', 'items': ['cigarrillo', 'refresco', 'agua']},
            {'name': 'Chaza del callejón', 'x': WX(18.5), 'z': WZ(21.5), 'rot': 0, 'kind': 'chaza', 'items': ['cigarrillo', 'pan', 'agua']},
            {'name': 'Chaza de la Carrera 8', 'x': (CITY_CARRERAS[2][0] - 1.5) * TILE, 'z': 55 * TILE, 'rot': math.pi / 2, 'kind': 'chaza', 'items': ['agua', 'pan', 'cafe']},
            {'name': 'Fritanga del Sur', 'x': 52 * TILE, 'z': (CITY_CALLES[2][0] - 1.5) * TILE, 'rot': 0, 'kind': 'fritanga', 'items': ['lataComida', 'pan', 'refresco']},
        ],
        # parques con palomas (Bogota.js: build) y carteles de parques y lago
        'pigeonParks': [[WX(29.5), WX(37.5), WZ(33.5), WZ(44.5)],
                        *[[(p['x0'] + .5) * TILE, (p['x1'] - .5) * TILE, (p['z0'] + .5) * TILE, (p['z1'] - .5) * TILE] for p in EXTENSION_PARKS]],
        'parkSigns': [{'x': WX(29), 'z': WZ(33), 'lines': ['Parque', 'Tercer Milenio']},
                      *[{'x': p['x0'] * TILE, 'z': ((p['z0'] + p['z1']) // 2) * TILE, 'lines': [p['name'], 'Bogotá']} for p in EXTENSION_PARKS],
                      {'x': 150, 'z': 226, 'lines': ['Parque del Lago', 'Lago · Puente TransMilenio']}],
        'busRoute': {'centerX': (CARACAS['busway'][0] * TILE + CARACAS['busway'][1] * TILE) / 2, 'endZ': -62, 'stationZ': -30},
        'streetLanes': {'carreras': [[lane(b, -1), lane(b, 1)] for b in CITY_CARRERAS]},
    }
    out = pathlib.Path(__file__).resolve().parent.parent / 'cpp' / 'assets' / 'data' / 'city-map.json'
    out.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding='utf-8')
    counts = {}
    for row in rows:
        for ch in row: counts[ch] = counts.get(ch, 0) + 1
    print('escrito', out, len(rows), 'filas', {data['codes'][k]: v for k, v in sorted(counts.items())})


if __name__ == '__main__':
    main()
