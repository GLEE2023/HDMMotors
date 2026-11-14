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
