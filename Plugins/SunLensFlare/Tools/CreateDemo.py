import unreal, pathlib, json, traceback
ROOT='/Game/Render/SunLensFlare'
lib=unreal.MaterialEditingLibrary
out=pathlib.Path(__file__).resolve().parent.parent/'Saved'
out.mkdir(parents=True,exist_ok=True)
report=dict(materials={},errors=[])
try:
    for part in ['Ghost','Halo','Glare']:
        m=unreal.load_asset(ROOT+'/M_MobileLensFlare_'+part)
        lib.recompile_material(m)
        report['materials'][part]=unreal.SunLensFlareImportLibrary.get_material_errors(m)
    map_path=ROOT+'/L_SunLensFlareDemo'
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.EditorLevelLibrary.load_level(map_path)
    else:
        unreal.EditorLevelLibrary.new_level(map_path)
        spawn=unreal.EditorLevelLibrary.spawn_actor_from_class
        sun=spawn(unreal.DirectionalLight,unreal.Vector(0,0,1000),unreal.Rotator(pitch=-41.56075,yaw=91.93006,roll=0))
        sun.set_actor_label('Sun_LensFlareDemo')
        sun.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        sun.light_component.set_editor_property('atmosphere_sun_light',True)
        sun.light_component.set_editor_property('intensity',100000.)
        sky=spawn(unreal.SkyAtmosphere,unreal.Vector(0,0,0))
        sky.set_actor_label('SkyAtmosphere')
        sky_light=spawn(unreal.SkyLight,unreal.Vector(0,0,0))
        sky_light.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        sky_light.light_component.set_editor_property('real_time_capture',True)
        flare=spawn(unreal.SunLensFlareActor,unreal.Vector(0,0,0))
        flare.set_actor_label('LensFlareActor')
        flare.set_editor_property('config',unreal.load_asset(ROOT+'/DA_MainMapLensFlare'))
        flare.set_editor_property('sun_light',sun)
        flare.refresh_materials()
        camera=spawn(unreal.CameraActor,unreal.Vector(0,0,200),unreal.Rotator(pitch=38.5057,yaw=-97.2463,roll=1.34735))
        camera.set_actor_label('Camera_MainMapAngle')
        camera.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
        camera.camera_component.set_editor_property('field_of_view',90.)
        camera.camera_component.set_editor_property('constrain_aspect_ratio',False)
        pp=spawn(unreal.PostProcessVolume,unreal.Vector(0,0,0))
        pp.set_actor_label('Exposure_NoNativeLensFlare')
        pp.set_editor_property('unbound',True)
        s=pp.get_editor_property('settings')
        for n,v in dict(override_auto_exposure_min_brightness=True,override_auto_exposure_max_brightness=True,auto_exposure_min_brightness=12.,auto_exposure_max_brightness=12.,override_lens_flare_intensity=True,lens_flare_intensity=0.,override_bloom_intensity=True,bloom_intensity=0.).items():s.set_editor_property(n,v)
        pp.set_editor_property('settings',s)
        ws=unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
        ws.set_editor_property('default_game_mode',unreal.GameModeBase)
    # Explicit keywords avoid Python Rotator's roll,pitch,yaw positional order.
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if isinstance(a,unreal.DirectionalLight):a.set_actor_rotation(unreal.Rotator(pitch=-41.56075,yaw=91.93006,roll=0),False)
        if isinstance(a,unreal.CameraActor):a.set_actor_rotation(unreal.Rotator(pitch=38.5057,yaw=-97.2463,roll=1.34735),False)
        if isinstance(a,unreal.SunLensFlareActor):
            a.set_editor_property('intensity',4096.)
            a.set_editor_property('color_fade_rate',1.5)
            a.set_editor_property('enabled',True)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,0,200),unreal.Rotator(pitch=38.5057,yaw=-97.2463,roll=1.34735))
    unreal.EditorLevelLibrary.save_current_level()
    report['map']=map_path
except Exception:report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1])
(out/'demo_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('DEMO_RESULT '+json.dumps(report,ensure_ascii=False))
