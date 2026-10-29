import unreal as u
source='/Game/DEM/Tests/L_MX301_BinaryMaterials'
target='/Game/DEM/Tests/L_CoupledPowder_GPU50k'
if u.EditorAssetLibrary.does_asset_exist(target):raise RuntimeError('Refusing to overwrite existing level')
assert u.EditorAssetLibrary.duplicate_asset(source,target)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level(target)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
found=0
for a in actors.get_all_level_actors():
 if a.get_class().get_name()=='PowderGranularMixer':
  a.set_editor_property('enable_coupled_grid_air',True)
  a.set_editor_property('enable_prescribed_air',False)
  a.set_editor_property('coupled_simple_box',True)
  a.set_editor_property('test_mixer_asset',True)
  a.set_editor_property('coupled_pressure_drop_pa',0.0)
  a.set_editor_property('target_rate',0.1)
  a.set_editor_property('rpm',0.0)
  a.set_editor_property('automatic_sequence',False)
  a.set_actor_label('GPU_50000_TwoWayAir_SimpleBox_NotCalibrated')
  found+=1
assert found==1
assert levels.save_current_level()
u.log('COUPLED_GPU_LEVEL_CREATED')
