import unreal, pathlib, time, traceback
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
out=pathlib.Path(unreal.Paths.project_saved_dir())/'StockingPreview'
out.mkdir(parents=True,exist_ok=True)
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Wardrobe/Maps/L_WardrobeMain')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor=next(a for a in sub.get_all_level_actors() if isinstance(a,unreal.WardrobeShowcase) and 'DA_Chara00' in [str(t) for t in a.tags])
profiles=unreal.load_asset('/Game/Wardrobe/DA_WardrobeCharacters')
actor.set_editor_property('interaction_data',profiles)
actor.set_editor_property('interaction_profile_index',0)
actor.set_interaction_enabled(True)
actor.stop_default_sequence()
component=actor.get_editor_property('stockings')
if not component.refresh_surface(): raise RuntimeError('Stocking initialization failed')
transform=actor.get_actor_transform()
point=unreal.MathLibrary.transform_location(transform,unreal.Vector(0,0,48))
forward=unreal.MathLibrary.transform_direction(transform,unreal.Vector(0,1,0))
location=point+forward*105
rotation=unreal.MathLibrary.find_look_at_rotation(location,point)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(location,rotation)
unreal.EditorLevelLibrary.editor_set_game_view(True)
preview_camera=sub.spawn_actor_from_class(unreal.CameraActor,location,rotation)
preview_camera.get_component_by_class(unreal.CameraComponent).set_editor_property('field_of_view',48)
preview_camera.get_component_by_class(unreal.CameraComponent).set_editor_property('post_process_blend_weight',0)
# All original lights, toon parameters, materials and world postprocess remain in use.
unreal.AutomationLibrary.finish_loading_before_screenshot()
surface=profiles.get_editor_property('characters')[0].get_editor_property('stockings').get_editor_property('surface')
vertices=surface.get_editor_property('vertices')
triangles=surface.get_editor_property('triangles')
started=time.monotonic()
state={'phase':0,'wait':0.0,'task':None,'busy':False}
def grab():
    for i in range(0,len(triangles),3):
        v=[vertices[triangles[i+j]].get_editor_property('position') for j in range(3)]
        p=(v[0]+v[1]+v[2])/3
        if p.z<32 or p.z>46: continue
        n=unreal.MathLibrary.normal(unreal.MathLibrary.cross_vector_vector(v[1]-v[0],v[2]-v[0]))
        if n.y<.45: continue
        origin=unreal.MathLibrary.transform_location(transform,p+n*80)
        ray=unreal.MathLibrary.transform_direction(transform,n*-1)
        if component.begin_pull(origin,ray):
            tangent=unreal.MathLibrary.transform_direction(transform,unreal.Vector(1 if p.x>0 else -1,0,0))
            component.update_pull(origin+tangent*12,ray)
            unreal.log('STOCKING_VIEWPORT_GRAB '+str(p))
            return
    raise RuntimeError('Unable to grab calf')
def screenshot(name):
    path=out/name
    task=unreal.AutomationLibrary.take_high_res_screenshot(1200,1200,str(path),camera=preview_camera,delay=0.5)
    if not task.is_valid_task(): raise RuntimeError('No main viewport for screenshot')
    unreal.log('STOCKING_VIEWPORT_CAPTURE '+name)
    state['task']=task
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        if time.monotonic()-started>180: raise RuntimeError('Viewport capture timed out')
        component.simulate(min(dt,1/30))
        state['wait']+=dt
        task=state['task']
        if task and not task.is_task_done(): return
        if state['phase']==0 and state['wait']>8:
            state['phase']=1; screenshot('丝袜_自然状态.png')
        elif state['phase']==1:
            grab(); state['wait']=0; state['task']=None; state['phase']=2
        elif state['phase']==2 and state['wait']>3:
            unreal.log('STOCKING_VIEWPORT_PULL '+str(component.get_pull_distance())+' strain='+str(component.get_maximum_stretch()))
            state['phase']=3; screenshot('丝袜_拉拽状态.png')
        elif state['phase']==3:
            component.release_pull(); state['wait']=0; state['task']=None; state['phase']=4
        elif state['phase']==4 and state['wait']>4:
            state['phase']=5; screenshot('丝袜_回弹状态.png')
        elif state['phase']==5:
            unreal.log('STOCKING_MAIN_VIEWPORT_COMPLETE'); finish()
    except Exception:
        unreal.log_error(traceback.format_exc()); finish()
    finally:
        state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)



