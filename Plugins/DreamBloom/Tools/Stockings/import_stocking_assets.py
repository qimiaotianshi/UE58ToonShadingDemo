import unreal, pathlib, json
ROOT='/Game/Wardrobe/Stockings'
SRC=str(pathlib.Path(unreal.Paths.project_content_dir())/'Model/Daniya/Red/Stockings/Source/Daniya_Stockings.fbx')
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
mesh=unreal.load_asset('/Game/Model/Daniya/Red/Stockings/SK_Daniya_Stockings')
if not mesh:
    options=unreal.FbxImportUI()
    options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_as_skeletal',True)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('import_animations',False)
    options.set_editor_property('import_materials',False)
    options.set_editor_property('import_textures',False)
    options.set_editor_property('create_physics_asset',False)
    skeletal=options.get_editor_property('skeletal_mesh_import_data')
    skeletal.set_editor_property('import_morph_targets',False)
    skeletal.set_editor_property('use_t0_as_ref_pose',False)
    skeletal.set_editor_property('convert_scene',True)
    skeletal.set_editor_property('convert_scene_unit',True)
    skeletal.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task=unreal.AssetImportTask()
    for name,value in {'filename':SRC,'destination_path':'/Game/Model/Daniya/Red/Stockings','destination_name':'SK_Daniya_Stockings','automated':True,'replace_existing':False,'save':True,'options':options,'factory':unreal.FbxFactory()}.items():
        task.set_editor_property(name,value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log('STOCKING_IMPORTED '+str(task.imported_object_paths))
    mesh=unreal.load_asset('/Game/Model/Daniya/Red/Stockings/SK_Daniya_Stockings')
if not mesh:raise RuntimeError('Independent skeletal mesh import failed')
body=unreal.load_asset('/Game/Model/Daniya/Red/DaniyaToUe')
unreal.log('STOCKING_SOURCE_REF '+unreal.WardrobeStockingEditorTools.describe_reference(mesh))
unreal.log('STOCKING_BODY_REF '+unreal.WardrobeStockingEditorTools.describe_reference(body))
surface=unreal.WardrobeStockingEditorTools.bake_surface(mesh,body,ROOT+'/DA_DaniyaStockingSurface')
if not surface:raise RuntimeError('Reference alignment or surface baking failed')
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
material=unreal.load_asset(ROOT+'/M_StockingPull')
if not material:
    material=asset_tools.create_asset('M_StockingPull',ROOT,unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('two_sided',True)
    material.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    def expr(cls,x,y):return unreal.MaterialEditingLibrary.create_material_expression(material,cls,x,y)
    custom=expr(unreal.MaterialExpressionCustom,0,0)
    custom.set_editor_property('description','Textile density, stretch transparency and edge pearl')
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    names=['UV','Strain','Pull','N','V','Tint','Opacity','Strength','Edge','Pearl']
    custom_inputs=[]
    for n in names:
        item=unreal.CustomInput();item.set_editor_property('input_name',n);custom_inputs.append(item)
    custom.set_editor_property('inputs',custom_inputs)
    custom.set_editor_property('code','''
float grazing=pow(saturate(1.0-abs(dot(normalize(N),normalize(V)))),1.5);
float stretch=saturate(Strain);
float alpha=lerp(saturate(Opacity),saturate(Edge),grazing)*(1.0-saturate(stretch*Strength));
float coverage=0.18*(1.0-smoothstep(0.0,0.25,stretch));
alpha=1.0-(1.0-alpha)*(1.0-coverage);
float2 p=UV*650.0;
float2 cell=floor(p);
float hash=frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453);
float2 q=frac(p)-0.5;
float aa=max(length(ddx(p)),length(ddy(p)))*0.35+0.015;
float grain=(1.0-smoothstep(max(0.0,0.13-aa),0.13+aa,length(q)))*step(0.7,hash);
grain*=1.0-smoothstep(1.5,3.0,max(length(ddx(p)),length(ddy(p))));
float sheen=saturate(Pearl)*(grazing*0.1+grain*grazing*0.55)*(1.0+saturate(Pull)*0.3);
return float4(saturate(Tint+sheen.xxx),saturate(alpha));
''')
    sources={}
    sources['UV']=(expr(unreal.MaterialExpressionTextureCoordinate,-700,0),'')
    vertex=expr(unreal.MaterialExpressionVertexColor,-700,160)
    vertex.set_editor_property('desc','R: strain (0-1), G: pull influence (0-1). Supplied by C++.')
    sources['Strain']=(vertex,'R');sources['Pull']=(vertex,'G')
    sources['N']=(expr(unreal.MaterialExpressionVertexNormalWS,-700,320),'')
    sources['V']=(expr(unreal.MaterialExpressionCameraVectorWS,-700,420),'')
    tint=expr(unreal.MaterialExpressionVectorParameter,-700,540);tint.set_editor_property('parameter_name','StockingTint');tint.set_editor_property('default_value',unreal.LinearColor(.004,.003,.005,1));sources['Tint']=(tint,'RGB')
    for i,(input_name,param,value) in enumerate([('Opacity','BaseOpacity',.97),('Strength','StretchOpacityStrength',1.5),('Edge','EdgeOpacity',.985),('Pearl','PearlStrength',.15)]):
        e=expr(unreal.MaterialExpressionScalarParameter,-400,650+i*100);e.set_editor_property('parameter_name',param);e.set_editor_property('default_value',value);sources[input_name]=(e,'')
    for name,(node,output) in sources.items():
        if not unreal.MaterialEditingLibrary.connect_material_expressions(node,output,custom,name):raise RuntimeError('Material input connection failed: '+name)
    rgb=expr(unreal.MaterialExpressionComponentMask,260,0);rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True)
    alpha=expr(unreal.MaterialExpressionComponentMask,260,140)
    for channel in ('r','g','b'):alpha.set_editor_property(channel,False)
    alpha.set_editor_property('a',True)
    unreal.MaterialEditingLibrary.connect_material_expressions(custom,'',rgb,'')
    unreal.MaterialEditingLibrary.connect_material_expressions(custom,'',alpha,'')
    unreal.MaterialEditingLibrary.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,.38),(unreal.MaterialProperty.MP_SPECULAR,.65),(unreal.MaterialProperty.MP_METALLIC,0)]:
        c=expr(unreal.MaterialExpressionConstant,260,320);c.set_editor_property('r',value);unreal.MaterialEditingLibrary.connect_material_property(c,'',prop)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
# The custom character lighting is a deferred Toon pass. A translucent density veil
# preserves the already-lit Toon skin underneath instead of introducing a second PBR lighting response.
material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('translucency_pass',unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
material.set_editor_property('disable_depth_test',False)
expressions=unreal.MaterialEditingLibrary.get_material_expressions(material)
rgb=next(e for e in expressions if isinstance(e,unreal.MaterialExpressionComponentMask) and e.get_editor_property('r') and e.get_editor_property('g') and e.get_editor_property('b'))
if not any(isinstance(e,unreal.MaterialExpressionEyeAdaptation) for e in expressions):
    eye=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionEyeAdaptation,260,460)
    stable_color=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionDivide,480,0)
    unreal.MaterialEditingLibrary.connect_material_expressions(rgb,'',stable_color,'A')
    unreal.MaterialEditingLibrary.connect_material_expressions(eye,'',stable_color,'B')
    unreal.MaterialEditingLibrary.connect_material_property(stable_color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
# Use the normal depth-tested pass so the original character costume and world occlude the textile.
expressions=unreal.MaterialEditingLibrary.get_material_expressions(material)
alpha=next(e for e in expressions if isinstance(e,unreal.MaterialExpressionComponentMask) and e.get_editor_property('a'))
unreal.MaterialEditingLibrary.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
for e in expressions:
    temporary=isinstance(e,(unreal.MaterialExpressionSceneDepth,unreal.MaterialExpressionPixelDepth,unreal.MaterialExpressionMultiply))
    temporary=temporary or (isinstance(e,unreal.MaterialExpressionCustom) and e.get_editor_property('description')=='Stocking costume depth gate')
    if temporary:unreal.MaterialEditingLibrary.delete_material_expression(material,e)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

# Appearance lives in an authored material instance, not character interaction settings.
for e in unreal.MaterialEditingLibrary.get_material_expressions(material):
    if isinstance(e,(unreal.MaterialExpressionScalarParameter,unreal.MaterialExpressionVectorParameter)) and str(e.get_editor_property('parameter_name')) in {'StockingTint','BaseOpacity','StretchOpacityStrength','EdgeOpacity','PearlStrength'}:
        e.set_editor_property('group','Stocking Appearance')
unreal.EditorAssetLibrary.save_loaded_asset(material)
appearance=unreal.load_asset(ROOT+'/MI_DaniyaStockings')
if not appearance:
    appearance=asset_tools.create_asset('MI_DaniyaStockings',ROOT,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    if not appearance:raise RuntimeError('Stocking material instance creation failed')
    unreal.MaterialEditingLibrary.set_material_instance_parent(appearance,material)
    unreal.MaterialEditingLibrary.update_material_instance(appearance)
    unreal.EditorAssetLibrary.save_loaded_asset(appearance)

da=unreal.load_asset('/Game/Wardrobe/DA_WardrobeCharacters')
characters=list(da.get_editor_property('characters'))
for c in characters:
    # This baked surface only supports Daniya; do not enable it on newly added characters.
    if str(c.get_editor_property('character_tag')) not in {'DA_Chara00','DA_Chara01'}:continue
    settings=c.get_editor_property('stockings')
    settings.set_editor_property('enabled',True)
    settings.set_editor_property('surface',surface)
    settings.set_editor_property('material',appearance)
    c.set_editor_property('stockings',settings)
da.set_editor_property('characters',characters)
unreal.EditorAssetLibrary.save_loaded_asset(da)
unreal.EditorAssetLibrary.save_directory('/Game/Model/Daniya/Red/Stockings',only_if_is_dirty=True,recursive=True)
unreal.log('STOCKING_ASSETS_READY profiles='+str(len(characters)))


