/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_key_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "object_intern.h"

/*************************** Clear Transformation ****************************/

static int object_location_clear_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Location");
	
	/* clear location of selected objects if not in weight-paint mode */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if (!(ob->mode & OB_MODE_WEIGHT_PAINT)) {
			/* clear location if not locked */
			if ((ob->protectflag & OB_LOCK_LOCX)==0)
				ob->loc[0]= ob->dloc[0]= 0.0f;
			if ((ob->protectflag & OB_LOCK_LOCY)==0)
				ob->loc[1]= ob->dloc[1]= 0.0f;
			if ((ob->protectflag & OB_LOCK_LOCZ)==0)
				ob->loc[2]= ob->dloc[2]= 0.0f;
				
			/* auto keyframing */
			if (autokeyframe_cfra_can_key(scene, &ob->id)) {
				ListBase dsources = {NULL, NULL};
				
				/* now insert the keyframe(s) using the Keying Set
				 *	1) add datasource override for the PoseChannel
				 *	2) insert keyframes
				 *	3) free the extra info 
				 */
				ANIM_relative_keyingset_add_source(&dsources, &ob->id, NULL, NULL); 
				ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
				BLI_freelistN(&dsources);
			}
		}
		
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;
	
	/* this is needed so children are also updated */
	DAG_ids_flush_update(0);

	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_location_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Location";
	ot->description = "Clear the object's location";
	ot->idname= "OBJECT_OT_location_clear";
	
	/* api callbacks */
	ot->exec= object_location_clear_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_rotation_clear_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Rotation");
	
	/* clear rotation of selected objects if not in weight-paint mode */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(!(ob->mode & OB_MODE_WEIGHT_PAINT)) {
			/* clear rotations that aren't locked */
			if (ob->protectflag & (OB_LOCK_ROTX|OB_LOCK_ROTY|OB_LOCK_ROTZ|OB_LOCK_ROTW)) {
				if (ob->protectflag & OB_LOCK_ROT4D) {
					/* perform clamping on a component by component basis */
					if (ob->rotmode == ROT_MODE_AXISANGLE) {
						if ((ob->protectflag & OB_LOCK_ROTW) == 0)
							ob->rotAngle= 0.0f;
						if ((ob->protectflag & OB_LOCK_ROTX) == 0)
							ob->rotAxis[0]= 0.0f;
						if ((ob->protectflag & OB_LOCK_ROTY) == 0)
							ob->rotAxis[1]= 0.0f;
						if ((ob->protectflag & OB_LOCK_ROTZ) == 0)
							ob->rotAxis[2]= 0.0f;
							
						/* check validity of axis - axis should never be 0,0,0 (if so, then we make it rotate about y) */
						if (IS_EQ(ob->rotAxis[0], ob->rotAxis[1]) && IS_EQ(ob->rotAxis[1], ob->rotAxis[2]))
							ob->rotAxis[1] = 1.0f;
					}
					else if (ob->rotmode == ROT_MODE_QUAT) {
						if ((ob->protectflag & OB_LOCK_ROTW) == 0)
							ob->quat[0]= 1.0f;
						if ((ob->protectflag & OB_LOCK_ROTX) == 0)
							ob->quat[1]= 0.0f;
						if ((ob->protectflag & OB_LOCK_ROTY) == 0)
							ob->quat[2]= 0.0f;
						if ((ob->protectflag & OB_LOCK_ROTZ) == 0)
							ob->quat[3]= 0.0f;
					}
					else {
						/* the flag may have been set for the other modes, so just ignore the extra flag... */
						if ((ob->protectflag & OB_LOCK_ROTX) == 0)
							ob->rot[0]= 0.0f;
						if ((ob->protectflag & OB_LOCK_ROTY) == 0)
							ob->rot[1]= 0.0f;
						if ((ob->protectflag & OB_LOCK_ROTZ) == 0)
							ob->rot[2]= 0.0f;
					}
				}
				else {
					/* perform clamping using euler form (3-components) */
					float eul[3], oldeul[3], quat1[4] = {0};
					
					if (ob->rotmode == ROT_MODE_QUAT) {
						QUATCOPY(quat1, ob->quat);
						quat_to_eul( oldeul,ob->quat);
					}
					else if (ob->rotmode == ROT_MODE_AXISANGLE) {
						axis_angle_to_eulO( oldeul, EULER_ORDER_DEFAULT,ob->rotAxis, ob->rotAngle);
					}
					else {
						VECCOPY(oldeul, ob->rot);
					}
					
					eul[0]= eul[1]= eul[2]= 0.0f;
					
					if (ob->protectflag & OB_LOCK_ROTX)
						eul[0]= oldeul[0];
					if (ob->protectflag & OB_LOCK_ROTY)
						eul[1]= oldeul[1];
					if (ob->protectflag & OB_LOCK_ROTZ)
						eul[2]= oldeul[2];
					
					if (ob->rotmode == ROT_MODE_QUAT) {
						eul_to_quat( ob->quat,eul);
						/* quaternions flip w sign to accumulate rotations correctly */
						if ((quat1[0]<0.0f && ob->quat[0]>0.0f) || (quat1[0]>0.0f && ob->quat[0]<0.0f)) {
							mul_qt_fl(ob->quat, -1.0f);
						}
					}
					else if (ob->rotmode == ROT_MODE_AXISANGLE) {
						eulO_to_axis_angle( ob->rotAxis, &ob->rotAngle,eul, EULER_ORDER_DEFAULT);
					}
					else {
						copy_v3_v3(ob->rot, eul);
					}
				}
			}						 // Duplicated in source/blender/editors/armature/editarmature.c
			else { 
				if (ob->rotmode == ROT_MODE_QUAT) {
					ob->quat[1]=ob->quat[2]=ob->quat[3]= 0.0f; 
					ob->quat[0]= 1.0f;
				}
				else if (ob->rotmode == ROT_MODE_AXISANGLE) {
					/* by default, make rotation of 0 radians around y-axis (roll) */
					ob->rotAxis[0]=ob->rotAxis[2]=ob->rotAngle= 0.0f;
					ob->rotAxis[1]= 1.0f;
				}
				else {
					ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0f;
				}
			}
			
			/* auto keyframing */
			if (autokeyframe_cfra_can_key(scene, &ob->id)) {
				ListBase dsources = {NULL, NULL};
				
				/* now insert the keyframe(s) using the Keying Set
				 *	1) add datasource override for the PoseChannel
				 *	2) insert keyframes
				 *	3) free the extra info 
				 */
				ANIM_relative_keyingset_add_source(&dsources, &ob->id, NULL, NULL); 
				ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
				BLI_freelistN(&dsources);
			}
		}
		
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;
	
	/* this is needed so children are also updated */
	DAG_ids_flush_update(0);

	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_rotation_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Rotation";
	ot->description = "Clear the object's rotation";
	ot->idname= "OBJECT_OT_rotation_clear";
	
	/* api callbacks */
	ot->exec= object_rotation_clear_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_scale_clear_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Scaling");
	
	/* clear scales of selected objects if not in weight-paint mode */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(!(ob->mode & OB_MODE_WEIGHT_PAINT)) {
			/* clear scale factors which are not locked */
			if ((ob->protectflag & OB_LOCK_SCALEX)==0) {
				ob->dsize[0]= 0.0f;
				ob->size[0]= 1.0f;
			}
			if ((ob->protectflag & OB_LOCK_SCALEY)==0) {
				ob->dsize[1]= 0.0f;
				ob->size[1]= 1.0f;
			}
			if ((ob->protectflag & OB_LOCK_SCALEZ)==0) {
				ob->dsize[2]= 0.0f;
				ob->size[2]= 1.0f;
			}
			
			/* auto keyframing */
			if (autokeyframe_cfra_can_key(scene, &ob->id)) {
				ListBase dsources = {NULL, NULL};
				
				/* now insert the keyframe(s) using the Keying Set
				 *	1) add datasource override for the PoseChannel
				 *	2) insert keyframes
				 *	3) free the extra info 
				 */
				ANIM_relative_keyingset_add_source(&dsources, &ob->id, NULL, NULL); 
				ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
				BLI_freelistN(&dsources);
			}
		}
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;
	
	/* this is needed so children are also updated */
	DAG_ids_flush_update(0);

	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_scale_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Scale";
	ot->description = "Clear the object's scale";
	ot->idname= "OBJECT_OT_scale_clear";
	
	/* api callbacks */
	ot->exec= object_scale_clear_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_origin_clear_exec(bContext *C, wmOperator *op)
{
	float *v1, *v3, mat[3][3];
	int armature_clear= 0;

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(ob->parent) {
			v1= ob->loc;
			v3= ob->parentinv[3];
			
			copy_m3_m4(mat, ob->parentinv);
			negate_v3_v3(v3, v1);
			mul_m3_v3(mat, v3);
		}
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;

	if(armature_clear==0) /* in this case flush was done */
		DAG_ids_flush_update(0);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_origin_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Origin";
	ot->description = "Clear the object's origin";
	ot->idname= "OBJECT_OT_origin_clear";
	
	/* api callbacks */
	ot->exec= object_origin_clear_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/*************************** Apply Transformation ****************************/

/* use this when the loc/size/rot of the parent has changed but the children
 * should stay in the same place, e.g. for apply-size-rot or object center */
static void ignore_parent_tx(Main *bmain, Scene *scene, Object *ob ) 
{
	Object workob;
	Object *ob_child;
	
	/* a change was made, adjust the children to compensate */
	for(ob_child=bmain->object.first; ob_child; ob_child=ob_child->id.next) {
		if(ob_child->parent == ob) {
			object_apply_mat4(ob_child, ob_child->obmat);
			what_does_parent(scene, ob_child, &workob);
			invert_m4_m4(ob_child->parentinv, workob.obmat);
		}
	}
}

static int apply_objects_internal(bContext *C, ReportList *reports, int apply_loc, int apply_scale, int apply_rot)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	bArmature *arm;
	Mesh *me;
	Curve *cu;
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	MVert *mvert;
	float rsmat[3][3], tmat[3][3], obmat[3][3], iobmat[3][3], mat[4][4], scale;
	int a, change = 0;
	
	/* first check if we can execute */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {

		if(ob->type==OB_MESH) {
			me= ob->data;
			
			if(ID_REAL_USERS(me) > 1) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a multi user mesh, doing nothing.");
				return OPERATOR_CANCELLED;
			}
		}
		else if(ob->type==OB_ARMATURE) {
			arm= ob->data;
			
			if(ID_REAL_USERS(arm) > 1) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a multi user armature, doing nothing.");
				return OPERATOR_CANCELLED;
			}
		}
		else if(ELEM(ob->type, OB_CURVE, OB_SURF)) {
			cu= ob->data;
			
			if(ID_REAL_USERS(cu) > 1) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a multi user curve, doing nothing.");
				return OPERATOR_CANCELLED;
			}
			if(!(cu->flag & CU_3D) && (apply_rot || apply_loc)) {
				BKE_report(reports, RPT_ERROR, "Neither rotation nor location could be applied to a 2d curve, doing nothing.");
				return OPERATOR_CANCELLED;
			}
			if(cu->key) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a curve with vertex keys, doing nothing.");
				return OPERATOR_CANCELLED;
			}
		}
	}
	CTX_DATA_END;
	
	/* now execute */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {

		/* calculate rotation/scale matrix */
		if(apply_scale && apply_rot)
			object_to_mat3(ob, rsmat);
		else if(apply_scale)
			object_scale_to_mat3(ob, rsmat);
		else if(apply_rot)
			object_rot_to_mat3(ob, rsmat);
		else
			unit_m3(rsmat);

		copy_m4_m3(mat, rsmat);

		/* calculate translation */
		if(apply_loc) {
			copy_v3_v3(mat[3], ob->loc);

			if(!(apply_scale && apply_rot)) {
				/* correct for scale and rotation that is still applied */
				object_to_mat3(ob, obmat);
				invert_m3_m3(iobmat, obmat);
				mul_m3_m3m3(tmat, rsmat, iobmat);
				mul_m3_v3(tmat, mat[3]);
			}
		}

		/* apply to object data */
		if(ob->type==OB_MESH) {
			me= ob->data;
			
			/* adjust data */
			mvert= me->mvert;
			for(a=0; a<me->totvert; a++, mvert++)
				mul_m4_v3(mat, mvert->co);
			
			if(me->key) {
				KeyBlock *kb;
				
				for(kb=me->key->block.first; kb; kb=kb->next) {
					float *fp= kb->data;
					
					for(a=0; a<kb->totelem; a++, fp+=3)
						mul_m4_v3(mat, fp);
				}
			}
			
			/* update normals */
			mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
		}
		else if (ob->type==OB_ARMATURE) {
			ED_armature_apply_transform(ob, mat);
		}
		else if(ELEM(ob->type, OB_CURVE, OB_SURF)) {
			cu= ob->data;

			scale = mat3_to_scale(rsmat);

			for(nu=cu->nurb.first; nu; nu=nu->next) {
				if(nu->type == CU_BEZIER) {
					a= nu->pntsu;
					for(bezt= nu->bezt; a--; bezt++) {
						mul_m4_v3(mat, bezt->vec[0]);
						mul_m4_v3(mat, bezt->vec[1]);
						mul_m4_v3(mat, bezt->vec[2]);
						bezt->radius *= scale;
					}
				}
				else {
					a= nu->pntsu*nu->pntsv;
					for(bp= nu->bp; a--; bp++)
						mul_m4_v3(mat, bp->vec);
				}
			}
		}
		else
			continue;

		if(apply_loc)
			ob->loc[0]= ob->loc[1]= ob->loc[2]= 0.0f;
		if(apply_scale)
			ob->size[0]= ob->size[1]= ob->size[2]= 1.0f;
		if(apply_rot) {
			ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0f;
			ob->quat[1]= ob->quat[2]= ob->quat[3]= 0.0f;
			ob->rotAxis[0]= ob->rotAxis[2]= 0.0f;
			ob->rotAngle= 0.0f;
			
			ob->quat[0]= ob->rotAxis[1]= 1.0f;
		}

		where_is_object(scene, ob);
		ignore_parent_tx(bmain, scene, ob);

		DAG_id_flush_update(&ob->id, OB_RECALC_OB|OB_RECALC_DATA);

		change = 1;
	}
	CTX_DATA_END;

	if(!change)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	return OPERATOR_FINISHED;
}

static int visual_transform_apply_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	int change = 0;
	
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		where_is_object(scene, ob);
		object_apply_mat4(ob, ob->obmat);
		where_is_object(scene, ob);
		
		change = 1;
	}
	CTX_DATA_END;

	if(!change)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_visual_transform_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Apply Visual Transform";
	ot->description = "Apply the object's visual transformation to its data";
	ot->idname= "OBJECT_OT_visual_transform_apply";
	
	/* api callbacks */
	ot->exec= visual_transform_apply_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int location_apply_exec(bContext *C, wmOperator *op)
{
	return apply_objects_internal(C, op->reports, 1, 0, 0);
}

void OBJECT_OT_location_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Apply Location";
	ot->description = "Apply the object's location to its data";
	ot->idname= "OBJECT_OT_location_apply";
	
	/* api callbacks */
	ot->exec= location_apply_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int scale_apply_exec(bContext *C, wmOperator *op)
{
	return apply_objects_internal(C, op->reports, 0, 1, 0);
}

void OBJECT_OT_scale_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Apply Scale";
	ot->description = "Apply the object's scale to its data";
	ot->idname= "OBJECT_OT_scale_apply";
	
	/* api callbacks */
	ot->exec= scale_apply_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int rotation_apply_exec(bContext *C, wmOperator *op)
{
	return apply_objects_internal(C, op->reports, 0, 0, 1);
}

void OBJECT_OT_rotation_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Apply Rotation";
	ot->description = "Apply the object's rotation to its data";
	ot->idname= "OBJECT_OT_rotation_apply";
	
	/* api callbacks */
	ot->exec= rotation_apply_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ Texture Space Transform ****************************/

void texspace_edit(Scene *scene, View3D *v3d)
{
	Base *base;
	int nr=0;
	
	/* first test if from visible and selected objects
	 * texspacedraw is set:
	 */
	
	if(scene->obedit) return; // XXX get from context
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			break;
		}
	}

	if(base==0) {
		return;
	}
	
	nr= 0; // XXX pupmenu("Texture Space %t|Grab/Move%x1|Size%x2");
	if(nr<1) return;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			base->object->dtx |= OB_TEXSPACE;
		}
	}
	

	if(nr==1) {
// XXX		initTransform(TFM_TRANSLATION, CTX_TEXTURE);
// XXX		Transform();
	}
	else if(nr==2) {
// XXX		initTransform(TFM_RESIZE, CTX_TEXTURE);
// XXX		Transform();
	}
	else if(nr==3) {
// XXX		initTransform(TFM_ROTATION, CTX_TEXTURE);
// XXX		Transform();
	}
}

/********************* Set Object Center ************************/

static EnumPropertyItem prop_set_center_types[] = {
	{0, "GEOMETRY_ORIGIN", 0, "Geometry to Origin", "Move object geometry to object origin"},
	{1, "ORIGIN_GEOMETRY", 0, "Origin to Geometry", "Move object origin to center of object geometry"},
	{2, "ORIGIN_CURSOR", 0, "Origin to 3D Cursor", "Move object origin to position of the 3d cursor"},
	{0, NULL, 0, NULL, NULL}
};

/* 0 == do center, 1 == center new, 2 == center cursor */
static int object_origin_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Object *obedit= CTX_data_edit_object(C);
	Object *tob;
	Mesh *me, *tme;
	Curve *cu;
/*	BezTriple *bezt;
	BPoint *bp; */
	Nurb *nu, *nu1;
	EditVert *eve;
	float cent[3], centn[3], min[3], max[3], omat[3][3];
	int a, total= 0;
	int centermode = RNA_enum_get(op->ptr, "type");
	
	/* keep track of what is changed */
	int tot_change=0, tot_lib_error=0, tot_multiuser_arm_error=0;
	MVert *mvert;

	if(scene->id.lib || v3d==NULL){
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed on Lib data");
		 return OPERATOR_CANCELLED;
	}
	if (obedit && centermode > 0) {
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in EditMode");
		return OPERATOR_CANCELLED;
	}	
	cent[0]= cent[1]= cent[2]= 0.0;	
	
	if(obedit) {

		INIT_MINMAX(min, max);
	
		if(obedit->type==OB_MESH) {
			Mesh *me= obedit->data;
			EditMesh *em = BKE_mesh_get_editmesh(me);

			for(eve= em->verts.first; eve; eve= eve->next) {
				if(v3d->around==V3D_CENTROID) {
					total++;
					VECADD(cent, cent, eve->co);
				}
				else {
					DO_MINMAX(eve->co, min, max);
				}
			}
			
			if(v3d->around==V3D_CENTROID) {
				mul_v3_fl(cent, 1.0f/(float)total);
			}
			else {
				cent[0]= (min[0]+max[0])/2.0f;
				cent[1]= (min[1]+max[1])/2.0f;
				cent[2]= (min[2]+max[2])/2.0f;
			}
			
			for(eve= em->verts.first; eve; eve= eve->next) {
				sub_v3_v3(eve->co, cent);			
			}
			
			recalc_editnormals(em);
			tot_change++;
			DAG_id_flush_update(&obedit->id, OB_RECALC_DATA);
			BKE_mesh_end_editmesh(me, em);
		}
	}
	
	/* reset flags */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			ob->flag &= ~OB_DONE;
	}
	CTX_DATA_END;

	for (tob= G.main->object.first; tob; tob= tob->id.next) {
		if(tob->data)
			((ID *)tob->data)->flag &= ~LIB_DOIT;
	}
	
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if((ob->flag & OB_DONE)==0) {
			ob->flag |= OB_DONE;
				
			if(obedit==NULL && (me=get_mesh(ob)) ) {
				if (me->id.lib) {
					tot_lib_error++;
				} else {
					if(centermode==2) {
						VECCOPY(cent, give_cursor(scene, v3d));
						invert_m4_m4(ob->imat, ob->obmat);
						mul_m4_v3(ob->imat, cent);
					} else {
						INIT_MINMAX(min, max);
						mvert= me->mvert;
						for(a=0; a<me->totvert; a++, mvert++) {
							DO_MINMAX(mvert->co, min, max);
						}
					
						cent[0]= (min[0]+max[0])/2.0f;
						cent[1]= (min[1]+max[1])/2.0f;
						cent[2]= (min[2]+max[2])/2.0f;
					}

					mvert= me->mvert;
					for(a=0; a<me->totvert; a++, mvert++) {
						sub_v3_v3(mvert->co, cent);
					}
					
					if (me->key) {
						KeyBlock *kb;
						for (kb=me->key->block.first; kb; kb=kb->next) {
							float *fp= kb->data;
							
							for (a=0; a<kb->totelem; a++, fp+=3) {
								sub_v3_v3(fp, cent);
							}
						}
					}

					tot_change++;
					me->id.flag |= LIB_DOIT;
						
					if(centermode) {
						copy_m3_m4(omat, ob->obmat);
						
						copy_v3_v3(centn, cent);
						mul_m3_v3(omat, centn);
						ob->loc[0]+= centn[0];
						ob->loc[1]+= centn[1];
						ob->loc[2]+= centn[2];
						
						where_is_object(scene, ob);
						ignore_parent_tx(bmain, scene, ob);
						
						/* other users? */
						CTX_DATA_BEGIN(C, Object*, ob_other, selected_editable_objects) {
							if((ob_other->flag & OB_DONE)==0) {
								tme= get_mesh(ob_other);
								
								if(tme==me) {
									
									ob_other->flag |= OB_DONE;
									ob_other->recalc= OB_RECALC_OB|OB_RECALC_DATA;

									copy_m3_m4(omat, ob_other->obmat);
									copy_v3_v3(centn, cent);
									mul_m3_v3(omat, centn);
									ob_other->loc[0]+= centn[0];
									ob_other->loc[1]+= centn[1];
									ob_other->loc[2]+= centn[2];
									
									where_is_object(scene, ob_other);
									ignore_parent_tx(bmain, scene, ob_other);
									
									if(!(tme->id.flag & LIB_DOIT)) {
										mvert= tme->mvert;
										for(a=0; a<tme->totvert; a++, mvert++) {
											sub_v3_v3(mvert->co, cent);
										}
										
										if (tme->key) {
											KeyBlock *kb;
											for (kb=tme->key->block.first; kb; kb=kb->next) {
												float *fp= kb->data;
												
												for (a=0; a<kb->totelem; a++, fp+=3) {
													sub_v3_v3(fp, cent);
												}
											}
										}

										tot_change++;
										tme->id.flag |= LIB_DOIT;
									}
								}
							}
						}
						CTX_DATA_END;
					}
				}
			}
			else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
				
				/* weak code here... (ton) */
				if(obedit==ob) {
					ListBase *editnurb= curve_get_editcurve(obedit);

					nu1= editnurb->first;
					cu= obedit->data;
				}
				else {
					cu= ob->data;
					nu1= cu->nurb.first;
				}
				
				if (cu->id.lib) {
					tot_lib_error++;
				} else {
					if(centermode==2) {
						copy_v3_v3(cent, give_cursor(scene, v3d));
						invert_m4_m4(ob->imat, ob->obmat);
						mul_m4_v3(ob->imat, cent);

						/* don't allow Z change if curve is 2D */
						if( !( cu->flag & CU_3D ) )
							cent[2] = 0.0;
					} 
					else {
						INIT_MINMAX(min, max);
						
						nu= nu1;
						while(nu) {
							minmaxNurb(nu, min, max);
							nu= nu->next;
						}
						
						cent[0]= (min[0]+max[0])/2.0f;
						cent[1]= (min[1]+max[1])/2.0f;
						cent[2]= (min[2]+max[2])/2.0f;
					}
					
					nu= nu1;
					while(nu) {
						if(nu->type == CU_BEZIER) {
							a= nu->pntsu;
							while (a--) {
								sub_v3_v3(nu->bezt[a].vec[0], cent);
								sub_v3_v3(nu->bezt[a].vec[1], cent);
								sub_v3_v3(nu->bezt[a].vec[2], cent);
							}
						}
						else {
							a= nu->pntsu*nu->pntsv;
							while (a--)
								sub_v3_v3(nu->bp[a].vec, cent);
						}
						nu= nu->next;
					}
			
					if(centermode && obedit==NULL) {
						copy_m3_m4(omat, ob->obmat);
						
						mul_m3_v3(omat, cent);
						ob->loc[0]+= cent[0];
						ob->loc[1]+= cent[1];
						ob->loc[2]+= cent[2];
						
						where_is_object(scene, ob);
						ignore_parent_tx(bmain, scene, ob);
					}
					
					tot_change++;
					cu->id.flag |= LIB_DOIT;

					if(obedit) {
						if (centermode==0) {
							DAG_id_flush_update(&obedit->id, OB_RECALC_DATA);
						}
						break;
					}
				}
			}
			else if(ob->type==OB_FONT) {
				/* get from bb */
				
				cu= ob->data;
				
				if(cu->bb==NULL) {
					/* do nothing*/
				} else if (cu->id.lib) {
					tot_lib_error++;
				} else {
					cu->xof= -0.5f*( cu->bb->vec[4][0] - cu->bb->vec[0][0]);
					cu->yof= -0.5f -0.5f*( cu->bb->vec[0][1] - cu->bb->vec[2][1]);	/* extra 0.5 is the height o above line */
					
					/* not really ok, do this better once! */
					cu->xof /= cu->fsize;
					cu->yof /= cu->fsize;

					tot_change++;
					cu->id.flag |= LIB_DOIT;
				}
			}
			else if(ob->type==OB_ARMATURE) {
				bArmature *arm = ob->data;
				
				if (arm->id.lib) {
					tot_lib_error++;
				} else if(ID_REAL_USERS(arm) > 1) {
					/*BKE_report(op->reports, RPT_ERROR, "Can't apply to a multi user armature");
					return;*/
					tot_multiuser_arm_error++;
				} else {
					/* Function to recenter armatures in editarmature.c 
					 * Bone + object locations are handled there.
					 */
					docenter_armature(scene, v3d, ob, centermode);

					tot_change++;
					cu->id.flag |= LIB_DOIT;
					
					where_is_object(scene, ob);
					ignore_parent_tx(bmain, scene, ob);
					
					if(obedit) 
						break;
				}
			}
		}
	}
	CTX_DATA_END;

	for (tob= G.main->object.first; tob; tob= tob->id.next) {
		if(tob->data && (((ID *)tob->data)->flag & LIB_DOIT)) {
			tob->recalc= OB_RECALC_OB|OB_RECALC_DATA;
		}
	}
	
	if (tot_change) {
		DAG_ids_flush_update(0);
		WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	}
	
	/* Warn if any errors occurred */
	if (tot_lib_error+tot_multiuser_arm_error) {
		BKE_reportf(op->reports, RPT_WARNING, "%i Object(s) Not Centered, %i Changed:",tot_lib_error+tot_multiuser_arm_error, tot_change);		
		if (tot_lib_error)
			BKE_reportf(op->reports, RPT_WARNING, "|%i linked library objects",tot_lib_error);
		if (tot_multiuser_arm_error)
			BKE_reportf(op->reports, RPT_WARNING, "|%i multiuser armature object(s)",tot_multiuser_arm_error);
	}
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_origin_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Origin";
	ot->description = "Set the object's origin, by either moving the data, or set to center of data, or use 3d cursor";
	ot->idname= "OBJECT_OT_origin_set";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_origin_set_exec;
	
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_set_center_types, 0, "Type", "");

}

