import unreal, json, os, re, pathlib, traceback
ROOT='/Game/Render/SunLensFlare'
BASE=pathlib.Path(__file__).resolve().parent.parent
SRC=BASE/'Reference'
REPORT=BASE/'Saved'/'ImportReport.json'
REPORT.parent.mkdir(parents=True,exist_ok=True)
report=dict(imported=[],materials={},errors=[])
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
imp=unreal.SunLensFlareImportLibrary
reference=json.loads((SRC/'reference.json').read_text(encoding='utf8'))
extra=json.loads((SRC/'extra.json').read_text(encoding='utf8'))
mapping={}

def read_t3d(name):
    raw=(SRC/(name+'.t3d')).read_bytes()
    return raw.decode('utf16' if raw.startswith(b'\xff\xfe') else 'utf8')

def parse(name):
    text=read_t3d(name)
    classes={}
    blocks={}
    root=[]
    depth=0
    current=None
    for line in text.splitlines():
        s=line.strip()
        if s.startswith('Begin Object'):
            depth+=1
            if depth==2:
                n=re.search(r'Name="([^"]+)"',s).group(1)
                c=re.search(r'Class=/Script/\w+\.(\w+)',s)
                if c:classes[n]=c.group(1)
                current=n
                blocks.setdefault(n,[])
            continue
        if s=='End Object':
            if depth==2:current=None
            depth-=1
            continue
        if depth==1:root.append(s)
        elif depth==2 and current:blocks[current].append(s)
    return classes,blocks,root

def create(name,cls,factory):
    p=ROOT+'/'+name
    existing=unreal.load_asset(p)
    if existing:return existing
    obj=tools.create_asset(name,ROOT,cls,factory)
    if not obj:raise RuntimeError('Cannot create '+p)
    report['imported'].append(p)
    return obj

def rewrite(text,asset,nodes):
    # Replace local object references with fully qualified paths of the new objects.
    def local(m):
        original=m.group(2)
        n=original.split(':')[-1]
        if n in nodes:
            node=nodes[n]
            return node.get_class().get_name()+"'\""+node.get_path_name()+"\"'"
        for old,new in mapping.items():
            if original==old:return m.group(1)+"'\""+new+"\"'"
        return m.group(0)
    return re.sub(r"(\w+)'\"([^\"]+)\"'",local,text)

def expression(asset,cls,name=None):
    if isinstance(asset,unreal.Material):return lib.create_material_expression(asset,cls)
    return lib.create_material_expression_in_function(asset,cls)

def import_graph(name,obj):
    imp.clear_expressions(obj)
    classes,blocks,root=parse(name)
    nodes={}
    remove=set()
    if name=='MF_LensflareColorAndMask':
        remove={n for n,c in classes.items() if c in ['MaterialExpressionBranch','MaterialExpressionIfCondition','MaterialExpressionElseCondition']}
        remove.update(['MaterialExpressionMaterialFunctionCall_2','MaterialExpressionStaticSwitchParameter_0','MaterialExpressionStaticSwitchParameter_1'])
    for n,c in classes.items():
        if not c.startswith('MaterialExpression') or n in remove:continue
        if 'Kuro' in c:c='MaterialExpressionVectorParameter'
        cls=getattr(unreal,c,None)
        if not cls:raise RuntimeError('Missing expression class '+c)
        nodes[n]=expression(obj,cls)
        if c=='MaterialExpressionComponentMask':
            # UE4's default selects no channels; UE5 defaults to R+G.
            setprop(nodes[n],r=False,g=False,b=False,a=False)
    for n,node in nodes.items():
        lines=[]
        for s in blocks[n]:
            if s.startswith(('Material=','Function=')):continue
            if name=='MF_LensflareColorAndMask' and n=='MaterialExpressionMultiply_2' and s.startswith('A='):
                s='A=(Expression=MaterialExpressionMaterialFunctionCall\'"MF_LensflareColorAndMask:MaterialExpressionMaterialFunctionCall_0"\',OutputIndex=1)'
            if name=='MF_LensflareColorAndMask' and n=='MaterialExpressionMultiply_9' and s.startswith('A='):
                s='A=(Expression=MaterialExpressionVectorParameter\'"MF_LensflareColorAndMask:MaterialExpressionVectorParameter_0"\',Mask=1,MaskR=1,MaskG=1,MaskB=1)'
            lines.append(s)
        if 'Kuro' in classes[n]:
            lines.extend(['ParameterName="SunDirectionWS"','DefaultValue=(R=0,G=0,B=-1,A=0)'])
        imp.import_properties(node,rewrite('\n'.join(lines),obj,nodes))
    if isinstance(obj,unreal.Material):
        pins=[]
        flags=[]
        allowed_flags=('BlendMode','ShadingModel','ShadingModels','TwoSided','bDisableDepthTest','TranslucencyLightingMode','bUsedWith','bEnableSeparateTranslucency','bSupportRayTracing')
        for s in root:
            key=s.split('=')[0]
            if key in ['EmissiveColor','Opacity','OpacityMask','WorldPositionOffset','BaseColor','Roughness','Metallic','Specular','Normal']:pins.append(s)
            elif key.startswith(allowed_flags):flags.append(s)
        imp.import_properties(obj,'\n'.join(flags))
        imp.import_material_properties(obj,rewrite('\n'.join(pins),obj,nodes))
        imp.link_function_calls(obj)
        obj.set_editor_property('translucency_pass',unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
        lib.layout_material_expressions(obj)
        lib.recompile_material(obj)
    else:
        imp.link_function_calls(obj)
        lib.update_material_function(obj)
    return nodes

def setprop(obj,**props):
    for n,v in props.items():obj.set_editor_property(n,v)
    return obj

def connect(a,b,pin,output=''):
    if not lib.connect_material_expressions(a,output,b,pin):
        if not lib.connect_material_expressions(a,output,b,''):raise RuntimeError('Cannot connect '+pin)

def build_cull(obj):
    imp.clear_expressions(obj)
    _,blocks,_=parse('MF_MobileLensFlare_Cull')
    uv=setprop(expression(obj,unreal.MaterialExpressionFunctionInput),input_name='SunScreenUV',input_type=unreal.FunctionInputType.FUNCTION_INPUT_VECTOR2)
    imp.import_properties(uv,'\n'.join(s for s in blocks['MaterialExpressionFunctionInput_3'] if s.startswith(('Id=','InputName=','InputType=','SortPriority='))))
    tex=setprop(expression(obj,unreal.MaterialExpressionTextureObjectParameter),parameter_name='SunSceneSample',texture=unreal.load_asset(ROOT+'/T_SunSceneSampleFallback'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    direction=setprop(expression(obj,unreal.MaterialExpressionVectorParameter),parameter_name='SunDirectionWS',default_value=unreal.LinearColor(0,0,-1,0))
    transform=setprop(expression(obj,unreal.MaterialExpressionTransform),transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_CAMERA)
    connect(direction,transform,'Input')
    custom=setprop(expression(obj,unreal.MaterialExpressionCustom),description='Sun visibility, brightness and facing fade',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs=[('UV',uv),('SceneSample',tex),('DirectionCS',transform)]
    for name,default in [('SceneSampleLumThreshold',2),('SceneSampleLumThresholdRange',1),('UseSceneSample',1),('CullCosTheta',.5),('CullSmooth',.1),('LensflareDensity',1)]:
        inputs.append((name,setprop(expression(obj,unreal.MaterialExpressionScalarParameter),parameter_name=name,default_value=default)))
    imp.import_properties(custom,'\n'.join('Inputs(%d)=(InputName="%s")'%(i,n) for i,(n,_) in enumerate(inputs)))
    custom.set_editor_property('code','''float4 S = Texture2DSampleLevel(SceneSample, SceneSampleSampler, float2(0.5,0.5), 0);
float Edge = saturate(1.0 - max(max(UV.x-1.0,-UV.x),max(UV.y-1.0,-UV.y))/0.1);
float Facing = saturate(1.0 - (CullCosTheta + normalize(DirectionCS).z)/max(CullSmooth,0.001));
float Visibility = smoothstep(SceneSampleLumThreshold,SceneSampleLumThreshold+max(SceneSampleLumThresholdRange,0.001),length(S.rgb)) * S.a * S.a;
return Edge * Facing * LensflareDensity * lerp(1.0,Visibility,UseSceneSample);''')
    for n,node in inputs:connect(node,custom,n)
    zero=setprop(expression(obj,unreal.MaterialExpressionConstant3Vector),constant=unreal.LinearColor(0,0,0,0))
    for name,original,node in [('Mask','MaterialExpressionFunctionOutput_0',custom),('WPOThrow','MaterialExpressionFunctionOutput_1',zero)]:
        o=expression(obj,unreal.MaterialExpressionFunctionOutput)
        imp.import_properties(o,'\n'.join(s for s in blocks[original] if s.startswith(('Id=','SortPriority=','OutputName='))))
        connect(node,o,'A')
    imp.link_function_calls(obj)
    lib.update_material_function(obj)

def build_color_mask(obj):
    imp.clear_expressions(obj)
    _,blocks,_=parse('MF_LensflareColorAndMask')
    def scalar(name,default):return setprop(expression(obj,unreal.MaterialExpressionScalarParameter),parameter_name=name,default_value=default)
    def vector(name,default):return setprop(expression(obj,unreal.MaterialExpressionVectorParameter),parameter_name=name,default_value=default)
    def rgb(node):
        mask=setprop(expression(obj,unreal.MaterialExpressionComponentMask),r=True,g=True,b=True,a=False)
        connect(node,mask,'Input');return mask
    direction=rgb(vector('SunDirectionWS',unreal.LinearColor(0,0,-1,0)))
    cs=setprop(expression(obj,unreal.MaterialExpressionTransform),transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_CAMERA)
    connect(direction,cs,'Input')
    fov=setprop(expression(obj,unreal.MaterialExpressionViewProperty),property=unreal.MaterialExposedViewProperty.MEVP_TAN_HALF_FIELD_OF_VIEW)
    uv=setprop(expression(obj,unreal.MaterialExpressionCustom),description='Original sun direction projection',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2,code='float3 D=normalize(DirectionCS); return D.xy/(min(D.z,-0.000001)*TanHalfFOV)*float2(0.5,-0.5)+float2(0.5,0.5);')
    imp.import_properties(uv,'Inputs(0)=(InputName="DirectionCS")\nInputs(1)=(InputName="TanHalfFOV")')
    connect(cs,uv,'DirectionCS');connect(fov,uv,'TanHalfFOV')
    call=setprop(expression(obj,unreal.MaterialExpressionMaterialFunctionCall),material_function=unreal.load_asset(ROOT+'/MF_MobileLensFlare_Cull'))
    imp.link_function_calls(obj)
    connect(uv,call,'SunScreenUV')
    opacity=expression(obj,unreal.MaterialExpressionMultiply)
    connect(call,opacity,'A','Mask');connect(scalar('Opacity',0),opacity,'B')
    color=expression(obj,unreal.MaterialExpressionMultiply)
    connect(rgb(vector('ColorTint',unreal.LinearColor(1,1,1,1))),color,'A')
    connect(rgb(vector('BaseColorTint',unreal.LinearColor(1,1,1,1))),color,'B')
    zero=setprop(expression(obj,unreal.MaterialExpressionConstant3Vector),constant=unreal.LinearColor(0,0,0,0))
    nodes=[opacity,cs,color,zero,direction,uv]
    for i,node in enumerate(nodes):
        o=expression(obj,unreal.MaterialExpressionFunctionOutput)
        imp.import_properties(o,'\n'.join(s for s in blocks['MaterialExpressionFunctionOutput_'+str(i)] if s.startswith(('Id=','OutputName=','SortPriority='))))
        connect(node,o,'A')
    imp.link_function_calls(obj)
    lib.update_material_function(obj)

def import_file(name,extension,cls,options=None):
    task=unreal.AssetImportTask()
    task.filename=str(SRC/(name+extension));task.destination_path=ROOT;task.destination_name=name
    task.automated=True;task.replace_existing=False;task.save=True
    if options:task.options=options
    if not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/'+name):tools.import_asset_tasks([task])
    result=unreal.load_asset(ROOT+'/'+name)
    if not result:raise RuntimeError('Import failed '+name)
    report['imported'].append(ROOT+'/'+name)
    return result

try:
    # Import the exact three current meshes; FBX preserves packed UV channels and vertex colors.
    fbx=unreal.FbxImportUI()
    fbx.set_editor_property('import_mesh',True);fbx.set_editor_property('import_as_skeletal',False)
    fbx.set_editor_property('import_materials',False);fbx.set_editor_property('import_textures',False)
    fbx.set_editor_property('automated_import_should_detect_type',False)
    fbx.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
    fbx.static_mesh_import_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
    fbx.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs',False)
    fbx.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
    mesh_names=[c['mesh'].split('.')[-1] for c in reference['actors'][0]['components']]
    for name in mesh_names:import_file(name,'.fbx',unreal.StaticMesh,fbx)
    texture=import_file('T_DefaultColorWhite_D','.tga',unreal.Texture2D)
    texture.set_editor_property('srgb',True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    if not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/T_SunSceneSampleFallback'):
        unreal.EditorAssetLibrary.duplicate_asset(ROOT+'/T_DefaultColorWhite_D',ROOT+'/T_SunSceneSampleFallback')
    fallback=unreal.load_asset(ROOT+'/T_SunSceneSampleFallback')
    fallback.set_editor_property('srgb',False)
    unreal.EditorAssetLibrary.save_loaded_asset(fallback)
    for component in extra['components']:
        for path in component['textures'].values():
            if path and path.startswith('/Game/'):
                import_file(path.split('.')[-1],'.tga',unreal.Texture2D)
    for name in ['MPC_LensFlare_Seq','MPC_Sequence']:
        obj=create(name,unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
        _,_,root=parse(name)
        imp.import_properties(obj,'\n'.join(root))
        unreal.EditorAssetLibrary.save_loaded_asset(obj)
    for d in reference['dependencies']:
        old=d['path']+'.'+d['path'].split('/')[-1]
        mapping[old]=ROOT+'/'+d['path'].split('/')[-1]+'.'+d['path'].split('/')[-1]
    for name in ['MF_MobileLensFlare_Cull','MF_MobileLensFlare_Offset','MF_LensflareColorAndMask']:
        obj=create(name,unreal.MaterialFunction,unreal.MaterialFunctionFactoryNew())
        if name=='MF_MobileLensFlare_Cull':build_cull(obj)
        elif name=='MF_LensflareColorAndMask':build_color_mask(obj)
        else:import_graph(name,obj)
        unreal.EditorAssetLibrary.save_loaded_asset(obj)
    configs={}
    for comp in reference['actors'][0]['components']:
        source=comp['materials'][0]
        name=source['base'].split('.')[-1]
        m=create(name,unreal.Material,unreal.MaterialFactoryNew())
        import_graph(name,m)
        mi_name=name.replace('M_MobileLensFlare_','MI_MainMap_')
        mi=create(mi_name,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(mi,m)
        for n,v in source['params'].items():
            if isinstance(v,dict):lib.set_material_instance_vector_parameter_value(mi,n,unreal.LinearColor(v['r'],v['g'],v['b'],v['a']))
            else:lib.set_material_instance_scalar_parameter_value(mi,n,v)
        original=next(c for c in extra['components'] if c['name']==comp['name'])
        for n,path in original['textures'].items():
            if path:
                tex=unreal.load_asset(ROOT+'/'+path.split('.')[-1]) if path.startswith('/Game/') else unreal.load_asset(path)
                lib.set_material_instance_texture_parameter_value(mi,n,tex)
        _,_,parent_properties=parse(original['parent'].split('.')[-1])
        static_properties='\n'.join(s for s in parent_properties if s.startswith('StaticParameters='))
        for value,n in re.findall(r'Value=(True|False),ParameterInfo=\(Name="([^"]+)"\)',static_properties):
            imp.set_static_switch(mi,n,value=='True')
        if comp['name'].endswith('Ghost'):
            imp.import_properties(mi,'BasePropertyOverrides=(bOverride_TwoSided=True,TwoSided=True)')
        lib.update_material_instance(mi)
        configs[name.split('_')[-1]]=(unreal.load_asset(ROOT+'/'+comp['mesh'].split('.')[-1]),mi)
        unreal.EditorAssetLibrary.save_loaded_asset(m)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        report['materials'][name]=imp.get_material_errors(m)
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.SunLensFlareConfig)
    config=create('DA_MainMapLensFlare',unreal.SunLensFlareConfig,factory)
    for part,(mesh,mat) in configs.items():
        config.set_editor_property('static_mesh_'+part.lower(),mesh)
        config.set_editor_property('material_'+part.lower(),mat)
    unreal.EditorAssetLibrary.save_loaded_asset(config)
    # A reusable actor Blueprint exposes the settings without requiring C++ knowledge.
    bp_factory=unreal.BlueprintFactory();bp_factory.set_editor_property('parent_class',unreal.SunLensFlareActor)
    bp=create('BP_SunLensFlareActor',unreal.Blueprint,bp_factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    default=unreal.get_default_object(bp.generated_class())
    default.set_editor_property('config',config)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.EditorAssetLibrary.save_directory(ROOT,only_if_is_dirty=True,recursive=True)
    for n in ['M_MobileLensFlare_Ghost','M_MobileLensFlare_Glare','MF_MobileLensFlare_Offset','MF_LensflareColorAndMask']:
        task=unreal.AssetExportTask();task.object=unreal.load_asset(ROOT+'/'+n);task.filename=str(REPORT.parent/('target_'+n+'.t3d'));task.automated=True;task.prompt=False;task.replace_identical=True
        unreal.Exporter.run_asset_export_task(task)
except Exception:
    report['errors'].append(traceback.format_exc())
    unreal.log_error(report['errors'][-1])
finally:
    REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('SUN_LENS_FLARE_IMPORT_RESULT '+json.dumps(report,ensure_ascii=False))
