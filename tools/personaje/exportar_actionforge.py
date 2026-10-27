"""Empaqueta el personaje predeterminado en GLB sin Blender (requiere Pillow)."""
import copy
import io
import json
from pathlib import Path
import struct
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
FOLDER = ROOT / 'public/models/personaje/hombre-adulto'
OUT = ROOT / 'work/exportaciones/personaje-vagabundo.glb'
catalog = json.loads((FOLDER.parent / 'catalogo.json').read_text(encoding='utf-8'))
body = next(b for b in catalog['cuerpos'] if b['id'] == 'hombre-adulto')
parts = [body['piezas'][p] for p in ['chaqueta', 'desordenado', 'descuidada', 'tenis-rotos']]
hidden = set(v for p in parts for v in p['oculta'])
result = dict(asset={'version': '2.0', 'generator': 'Vagabundo character export'},
              scene=0, scenes=[{'nodes': []}], **{k: [] for k in
              ['nodes', 'meshes', 'skins', 'accessors', 'bufferViews', 'materials', 'textures', 'images', 'samplers']})
binary = bytearray()
bone_nodes = {}


def append_data(data):
    binary.extend(b'\0' * (-len(binary) % 4))
    start = len(binary)
    binary.extend(data)
    return start


def values(g, buffers, accessor_id):
    a = g['accessors'][accessor_id]
    view = g['bufferViews'][a['bufferView']]
    code, size = {5121: ('B', 1), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}[a['componentType']]
    start = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    step = view.get('byteStride', size)
    return [struct.unpack_from('<' + code, buffers[view['buffer']], start + i * step)[0]
            for i in range(a['count'])]


for file_index, filename in enumerate(['cuerpo.gltf'] + [p['file'] for p in parts]):
    g = json.loads((FOLDER / filename).read_text(encoding='utf-8'))
    buffers = [(FOLDER / b['uri']).read_bytes() for b in g['buffers']]
    offsets = {key: len(result[key]) for key in result if isinstance(result[key], list)}
    starts = [append_data(b) for b in buffers]
    for v in g.get('bufferViews', []):
        result['bufferViews'].append({**v, 'buffer': 0,
            'byteOffset': starts[v['buffer']] + v.get('byteOffset', 0)})
    for a in g.get('accessors', []):
        if 'sparse' in a:
            raise ValueError('No se admite un accesor sparse en esta exportación')
        result['accessors'].append({**a, 'bufferView': a['bufferView'] + offsets['bufferViews']})
    result['samplers'].extend(g.get('samplers', []))
    for image in g.get('images', []):
        with Image.open(FOLDER / image['uri']) as source:
            output = io.BytesIO()
            source.save(output, format='PNG')
        data = output.getvalue()
        view_id = len(result['bufferViews'])
        result['bufferViews'].append({'buffer': 0, 'byteOffset': append_data(data), 'byteLength': len(data)})
        result['images'].append({'name': image.get('name', 'texture'), 'mimeType': 'image/png', 'bufferView': view_id})
    for texture in g.get('textures', []):
        source = texture.get('source', texture.get('extensions', {}).get('EXT_texture_webp', {}).get('source'))
        result['textures'].append({'source': source + offsets['images'],
                                   'sampler': texture.get('sampler', 0) + offsets['samplers']})

    def material_refs(obj):
        if isinstance(obj, dict):
            for key, value in obj.items():
                if key.endswith('Texture') and isinstance(value, dict) and 'index' in value:
                    value['index'] += offsets['textures']
                else:
                    material_refs(value)
        elif isinstance(obj, list):
            for value in obj:
                material_refs(value)

    for material in g.get('materials', []):
        material_refs(material)
        result['materials'].append(material)
    for mesh in g['meshes']:
        mesh = copy.deepcopy(mesh)
        for primitive in mesh['primitives']:
            # Igual que en el juego: retirar cuerpo cubierto por ropa y zapatos.
            if file_index == 0 and 'body' in mesh.get('name', '').lower():
                vid_id = primitive['attributes'].get('_VID', primitive['attributes'].get('_vid'))
                if vid_id is not None:
                    vertices = values(g, buffers, vid_id)
                    indices = values(g, buffers, primitive['indices'])
                    kept = [v for i in range(0, len(indices), 3)
                            if not any(round(vertices[v]) in hidden for v in indices[i:i+3])
                            for v in indices[i:i+3]]
                    data = struct.pack('<' + 'I' * len(kept), *kept)
                    view = len(result['bufferViews'])
                    result['bufferViews'].append({'buffer': 0, 'byteOffset': append_data(data), 'byteLength': len(data), 'target': 34963})
                    new_index = len(result['accessors'])
                    result['accessors'].append({'bufferView': view, 'componentType': 5125, 'count': len(kept), 'type': 'SCALAR', 'min': [min(kept)], 'max': [max(kept)]})
                    primitive['indices'] = new_index - offsets['accessors']
            primitive['attributes'] = {k: v + offsets['accessors'] for k, v in primitive['attributes'].items()}
            if 'indices' in primitive:
                primitive['indices'] += offsets['accessors']
            if 'material' in primitive:
                primitive['material'] += offsets['materials']
            for target in primitive.get('targets', []):
                for key in target:
                    target[key] += offsets['accessors']
        result['meshes'].append(mesh)
    node_map = {}
    joints = {j for skin in g['skins'] for j in skin['joints']}
    for i, node in enumerate(g['nodes']):
        if file_index and i in joints:
            node_map[i] = bone_nodes[node['name']]
        elif not file_index or 'mesh' in node:
            node_map[i] = len(result['nodes'])
            result['nodes'].append(copy.deepcopy(node))
    for i, node in enumerate(g['nodes']):
        if file_index and 'mesh' not in node:
            continue
        out = result['nodes'][node_map[i]]
        if 'children' in out:
            out['children'] = [node_map[n] for n in out['children']]
        if 'mesh' in out:
            out['mesh'] += offsets['meshes']
            out['skin'] += offsets['skins']
            if file_index:
                result['nodes'][body_root]['children'].append(node_map[i])
    for skin in g['skins']:
        skin['joints'] = [node_map[j] for j in skin['joints']]
        skin['inverseBindMatrices'] += offsets['accessors']
        if 'skeleton' in skin:
            skin['skeleton'] = node_map[skin['skeleton']]
        result['skins'].append(skin)
    if not file_index:
        bone_nodes = {g['nodes'][j]['name']: node_map[j] for j in joints}
        result['scenes'][0]['nodes'] = [node_map[n] for n in g['scenes'][g.get('scene', 0)]['nodes']]
        body_root = result['scenes'][0]['nodes'][0]

result['buffers'] = [{'byteLength': len(binary)}]
metadata = json.dumps(result, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
metadata += b' ' * (-len(metadata) % 4)
binary.extend(b'\0' * (-len(binary) % 4))
total = 12 + 8 + len(metadata) + 8 + len(binary)
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes(struct.pack('<III', 0x46546C67, 2, total) + struct.pack('<II', len(metadata), 0x4E4F534A)
                + metadata + struct.pack('<II', len(binary), 0x004E4942) + binary)
assert all(j < len(result['nodes']) for s in result['skins'] for j in s['joints'])
assert len(bone_nodes) == 52
assert all('uri' not in image for image in result['images'])
print(f'{OUT}\n{total / 1024 / 1024:.1f} MB; {len(bone_nodes)} huesos; {len(result["meshes"])} mallas; {len(result["images"])} texturas PNG')
