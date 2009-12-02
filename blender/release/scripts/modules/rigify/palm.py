# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
from rigify import get_bone_data, copy_bone_simple
from rna_prop_ui import rna_idprop_ui_get, rna_idprop_ui_prop_get


def metarig_template():
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.object
    arm = obj.data
    bone = arm.edit_bones.new('hand')
    bone.head[:] = 0.0082, -1.2492, 0.0000
    bone.tail[:] = 0.0423, -0.4150, 0.0000
    bone.roll = 0.0000
    bone.connected = False
    bone = arm.edit_bones.new('palm.03')
    bone.head[:] = 0.0000, 0.0000, -0.0000
    bone.tail[:] = 0.0506, 1.2781, -0.1299
    bone.roll = -3.1396
    bone.connected = False
    bone.parent = arm.edit_bones['hand']
    bone = arm.edit_bones.new('palm.04')
    bone.head[:] = 0.5000, -0.0000, 0.0000
    bone.tail[:] = 0.6433, 1.2444, -0.1299
    bone.roll = -3.1357
    bone.connected = False
    bone.parent = arm.edit_bones['hand']
    bone = arm.edit_bones.new('palm.05')
    bone.head[:] = 1.0000, 0.0000, 0.0000
    bone.tail[:] = 1.3961, 1.0084, -0.1299
    bone.roll = -3.1190
    bone.connected = False
    bone.parent = arm.edit_bones['hand']
    bone = arm.edit_bones.new('palm.02')
    bone.head[:] = -0.5000, 0.0000, -0.0000
    bone.tail[:] = -0.5674, 1.2022, -0.1299
    bone.roll = 3.1386
    bone.connected = False
    bone.parent = arm.edit_bones['hand']
    bone = arm.edit_bones.new('palm.01')
    bone.head[:] = -1.0000, 0.0000, -0.0000
    bone.tail[:] = -1.3286, 1.0590, -0.1299
    bone.roll = 3.1239
    bone.connected = False
    bone.parent = arm.edit_bones['hand']
    bone = arm.edit_bones.new('palm.06')
    bone.head[:] = 1.3536, -0.2941, 0.0000
    bone.tail[:] = 2.1109, 0.4807, -0.1299
    bone.roll = -3.0929
    bone.connected = False
    bone.parent = arm.edit_bones['hand']

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['hand']
    pbone['type'] = 'palm'


def main(obj, orig_bone_name):
    arm, palm_pbone, palm_ebone = get_bone_data(obj, orig_bone_name)
    children = [ebone.name for ebone in palm_ebone.children]
    children.sort() # simply assume the pinky has the lowest name
    
    # Make a copy of the pinky
    pinky_ebone = arm.edit_bones[children[0]]
    control_ebone = copy_bone_simple(arm, pinky_ebone.name, "palm_control", parent=True)
    control_name = control_ebone.name 
    
    offset = (arm.edit_bones[children[0]].head - arm.edit_bones[children[1]].head)
    
    control_ebone.head += offset
    control_ebone.tail += offset
    
    bpy.ops.object.mode_set(mode='OBJECT')
    
    
    arm, control_pbone, control_ebone = get_bone_data(obj, control_name)
    arm, pinky_pbone, pinky_ebone = get_bone_data(obj, children[0])
    
    control_pbone.rotation_mode = 'YZX'
    control_pbone.lock_rotation = False, True, True
    
    driver_fcurves = pinky_pbone.driver_add("rotation_euler")
    
    
    controller_path = control_pbone.path_to_id()
    
    # add custom prop
    control_pbone["spread"] = 0.0
    prop = rna_idprop_ui_prop_get(control_pbone, "spread", create=True)
    prop["soft_min"] = -1.0
    prop["soft_max"] = 1.0
    
    
    # *****
    driver = driver_fcurves[0].driver
    driver.type = 'AVERAGE'
    
    tar = driver.targets.new()
    tar.name = "x"
    tar.id_type = 'OBJECT'
    tar.id = obj
    tar.rna_path = controller_path + ".rotation_euler[0]"
    
    
    # *****
    driver = driver_fcurves[1].driver
    driver.expression = "-x/4.0"
    
    tar = driver.targets.new()
    tar.name = "x"
    tar.id_type = 'OBJECT'
    tar.id = obj
    tar.rna_path = controller_path + ".rotation_euler[0]"
    
    
    # *****
    driver = driver_fcurves[2].driver
    driver.expression = "(1.0-cos(x))-s"
    tar = driver.targets.new()
    tar.name = "x"
    tar.id_type = 'OBJECT'
    tar.id = obj
    tar.rna_path = controller_path + ".rotation_euler[0]"
    
    tar = driver.targets.new()
    tar.name = "s"
    tar.id_type = 'OBJECT'
    tar.id = obj
    tar.rna_path = controller_path + '["spread"]'


    for i, child_name in enumerate(children):
        child_pbone = obj.pose.bones[child_name]
        child_pbone.rotation_mode = 'YZX'
        
        if child_name != children[-1] and child_name != children[0]:
            
            # this is somewhat arbitrary but seems to look good
            inf = i / (len(children)+1)
            inf = 1.0 - inf
            inf = ((inf * inf) + inf) / 2.0
            
            # used for X/Y constraint
            inf_minor = inf * inf
            
            con = child_pbone.constraints.new('COPY_ROTATION')
            con.name = "Copy Z Rot"
            con.target = obj
            con.subtarget = children[0] # also pinky_pbone
            con.owner_space = con.target_space = 'LOCAL'
            con.use_x, con.use_y, con.use_z = False, False, True
            con.influence = inf

            con = child_pbone.constraints.new('COPY_ROTATION')
            con.name = "Copy XY Rot"
            con.target = obj
            con.subtarget = children[0] # also pinky_pbone
            con.owner_space = con.target_space = 'LOCAL'
            con.use_x, con.use_y, con.use_z = True, True, False
            con.influence = inf_minor


    child_pbone = obj.pose.bones[children[-1]]
    child_pbone.rotation_mode = 'QUATERNION'

