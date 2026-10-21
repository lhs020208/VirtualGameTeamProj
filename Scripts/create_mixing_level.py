"""Create the task-21 mixing fixture and two explicit group display materials."""
import unreal as u
path='/Game/L_MixingTest'
assert not u.EditorAssetLibrary.does_asset_exist(path), 'Refusing to overwrite an existing level'
materials=[]
for name,color in [('M_MixA',u.LinearColor(.03,.25,1,1)),('M_MixB',u.LinearColor(1,.19,.015,1))]:
    asset='/Game/DEM/'+name
    assert not u.EditorAssetLibrary.does_asset_exist(asset), 'Refusing to overwrite material'
    material=u.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/DEM',u.Material,u.MaterialFactoryNew())
    material.set_editor_property('used_with_instanced_static_meshes',True)
    node=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant',color)
    u.MaterialEditingLibrary.connect_material_property(node,'',u.MaterialProperty.MP_BASE_COLOR)
    u.MaterialEditingLibrary.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material)
    materials.append(material)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.new_level(path)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderClumpTest')
game=u.load_class(None,'/Script/VirtualGameTeamProj.PowderClumpGameMode')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',game)
actor=actors.spawn_actor_from_class(cls,u.Vector(0,0,0))
actor.set_actor_label('MixingExperiment_A16_B16_30RPM')
actor.set_editor_property('mixing_rig',True)
actor.set_editor_property('group_a_material',materials[0])
actor.set_editor_property('group_b_material',materials[1])
actor.reset_simulation()
assert not actor.get_editor_property('error'),str(actor.get_editor_property('error'))
assert actor.get_editor_property('mixing_index_valid')
assert abs(actor.get_editor_property('mixing_index'))<1e-12
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,200),u.Rotator(pitch=-50,yaw=-30,roll=0))
light.light_component.set_editor_property('intensity',3.0)
actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,200))
actors.spawn_actor_from_class(u.SkyAtmosphere,u.Vector(0,0,0))
assert levels.save_current_level()
u.log('TASK21_LEVEL_CREATED /Game/L_MixingTest')
