import bpy,bmesh,json,pathlib,argparse,sys
from mathutils import Vector
parser=argparse.ArgumentParser(description='Create separately weighted Daniya stockings from the original Blender character.')
parser.add_argument('--output',required=True,help='Destination folder for the new blend and FBX files')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
root=pathlib.Path(args.output).resolve()
root.mkdir(parents=True,exist_ok=True)
if bpy.data.objects.get('Daniya_Stockings'):raise RuntimeError('Open the original character blend before generating stockings')
if pathlib.Path(bpy.data.filepath).resolve()==root/'Daniya_WithStockings.blend':raise RuntimeError('Do not overwrite the open source blend')
body=max((o for o in bpy.data.objects if o.type=='MESH'),key=lambda o:len(o.data.vertices))
arm=next(m.object for m in body.modifiers if m.type=='ARMATURE')
skin=next(i for i,m in enumerate(body.data.materials) if m and m.name=='肌肤')
leg_groups={g.index for g in body.vertex_groups if g.name in {'足.L','足.R','ひざ.L','ひざ.R','足D.L','足D.R','ひざD.L','ひざD.R','足首D.L','足首D.R','足先EX.L','足先EX.R','足首.L','足首.R'}}
def leg_vertex(v):
    return sum(g.weight for g in v.groups if g.group in leg_groups)>.45 and .08< v.co.z <.89
keep={p.index for p in body.data.polygons if p.material_index==skin and all(leg_vertex(body.data.vertices[v]) for v in p.vertices)}
if len(keep)<500:raise RuntimeError('Unexpected leg selection '+str(len(keep)))
stock=body.copy();stock.data=body.data.copy();stock.name='Daniya_Stockings';bpy.context.collection.objects.link(stock)
stock.data.shape_keys and stock.shape_key_clear()
bm=bmesh.new();bm.from_mesh(stock.data);bm.faces.ensure_lookup_table()
bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.index not in keep],context='FACES')
bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_faces],context='VERTS')
bm.normal_update()
for v in bm.verts:v.co+=v.normal*.0012
# Subdivide without smoothing the surface away from the skin; custom data includes bone weights and UVs.
bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=1,use_grid_fill=True)
bm.normal_update();bm.to_mesh(stock.data);bm.free();stock.data.update()
stock.data.materials.clear();mat=bpy.data.materials.new('M_Daniya_Stockings');mat.diffuse_color=(.08,.055,.075,.5);stock.data.materials.append(mat)
for p in stock.data.polygons:p.material_index=0;p.use_smooth=True
for m in list(stock.modifiers):
    if m.type!='ARMATURE':stock.modifiers.remove(m)
for o in bpy.context.view_layer.objects:o.select_set(False)
stock.select_set(True);arm.select_set(True);bpy.context.view_layer.objects.active=stock
fbx=root/'Daniya_Stockings.fbx'
bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,use_armature_deform_only=False,bake_anim=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',use_mesh_modifiers=True)
blend=root/'Daniya_WithStockings.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(blend))
print('STOCKING_CREATED',len(stock.data.vertices),len(stock.data.polygons),str(fbx),str(blend))
stats={'mesh':stock.name,'vertices':len(stock.data.vertices),'faces':len(stock.data.polygons),'selected_source_faces':len(keep),'fbx':str(fbx),'blend':str(blend),'offset_metres':.0012}
(root/'stocking_mesh_info.json').write_text(json.dumps(stats,ensure_ascii=False,indent=2),encoding='utf-8')
