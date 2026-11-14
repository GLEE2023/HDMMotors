"""Generate a render-only wall-free GLB from the reference STEP assembly.

Requires cadquery/OCP and trimesh. Use the newest CAD and correctly separated
joint axes before treating animation as kinematically accurate.

Usage: python tools/convert_step.py path/to/FullAssembly.stp
"""
from __future__ import annotations
import sys
from pathlib import Path
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString
from OCP.XCAFDoc import XCAFDoc_DocumentTool
from OCP.TDF import TDF_LabelSequence
from OCP.TDataStd import TDataStd_Name
from OCP.TopExp import TopExp_Explorer
from OCP.TopAbs import TopAbs_SOLID
from OCP.TopoDS import TopoDS
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.StlAPI import StlAPI_Writer
import trimesh

path=Path(sys.argv[1] if len(sys.argv)>1 else 'Full+Assembly.stp')
reader=STEPCAFControl_Reader()
reader.SetNameMode(True)
if reader.ReadFile(str(path.resolve())).name != 'IFSelect_RetDone':
    raise RuntimeError(f'Unable to read {path}')
doc=TDocStd_Document(TCollection_ExtendedString('mechanism'))
assert reader.Transfer(doc)
shapes=XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
roots=TDF_LabelSequence();shapes.GetFreeShapes(roots)
parts=TDF_LabelSequence();shapes.GetComponents_s(roots.Value(1),parts)

scene=trimesh.Scene()
selected=0
for i in range(1,parts.Length()+1):
    component=parts.Value(i)
    name_attribute=TDataStd_Name()
    component.FindAttribute(TDataStd_Name.GetID_s(),name_attribute)
    name=name_attribute.Get().ToExtString()
    # This reference model stores all enclosure walls as separate labels.
    # Use an allowlist rather than hiding by geometry thickness to avoid
    # accidentally removing motor mounts when a newer assembly changes.
    if not (name.startswith('ActiveMotor') or name.startswith('ASSEMBLY V1')):
        continue
    prefix='motor' if name.startswith('ActiveMotor') else 'upper'
    explorer=TopExp_Explorer(shapes.GetShape_s(component),TopAbs_SOLID)
    index=0
    while explorer.More():
        solid=TopoDS.Solid_s(explorer.Current())
        BRepMesh_IncrementalMesh(solid,1.5,False,0.35,True).Perform()
        # STL is a temporary interchange representation. Export one solid
        # at a time to retain selectable/editable groups in the GLB.
        tmp=Path('/tmp')/f'_glee_{prefix}_{index}.stl'
        StlAPI_Writer().Write(solid,str(tmp))
        mesh=trimesh.load(tmp,force='mesh',process=True)
        tmp.unlink(missing_ok=True)
        if not mesh.is_empty and len(mesh.vertices)>9:
            mesh.visual.vertex_colors=[111,134,164,255] if prefix=='motor' else [217,227,240,255]
            name=f'{prefix}_{index}'
            scene.add_geometry(mesh,node_name=name,geom_name=name)
            selected+=1
        explorer.Next();index+=1
out=Path(__file__).resolve().parents[1]/'public/models/glee-mechanism.glb'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_bytes(scene.export(file_type='glb'))
print(f'Exported {selected} mechanism solids without enclosure walls to {out}')
