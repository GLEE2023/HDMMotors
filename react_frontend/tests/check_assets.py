"""Non-hardware checks for the bundled CAD asset."""
from pathlib import Path
import trimesh

root=Path(__file__).resolve().parents[1]
model=root/'public/models/glee-mechanism.glb'
assert model.exists() and model.stat().st_size>1000
scene=trimesh.load(model,force='scene')
meshes=[v for v in scene.geometry.values() if isinstance(v,trimesh.Trimesh)]
assert len(meshes)>=10, f'Insufficient retained mechanism components: {len(meshes)}'
assert sum(len(m.faces) for m in meshes)>2000
assert any('motor' in name for name in scene.geometry)
assert any('upper' in name for name in scene.geometry)
assert not any('Wall' in name or 'Plate' in name or 'Garage' in name for name in scene.geometry)
logo=root/'public/branding/glee-logo.webp'
assert logo.exists() and logo.stat().st_size>1000
print(f'OK: {len(meshes)} CAD components, {sum(len(m.faces) for m in meshes)} triangles, logo present')

# Inspect the actual GLB attributes. Loading with trimesh computes normals on
# demand, so checking mesh.vertex_normals alone would miss broken GLB exports.
import struct, json
binary=model.read_bytes()
json_length, chunk_type=struct.unpack_from('<II',binary,12)
assert chunk_type==0x4E4F534A, 'Missing GLB JSON chunk'
gltf=json.loads(binary[20:20+json_length])
assert all('NORMAL' in primitive['attributes']
           for mesh in gltf['meshes'] for primitive in mesh['primitives']), \
       'Each mechanism mesh must include NORMAL attributes for lighting'
print('OK: all CAD meshes contain explicit vertex normals')
