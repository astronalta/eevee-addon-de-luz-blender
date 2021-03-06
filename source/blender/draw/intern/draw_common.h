/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_common.h
 *  \ingroup draw
 */

#ifndef __DRAW_COMMON_H__
#define __DRAW_COMMON_H__

struct DRWPass;
struct DRWShadingGroup;
struct Gwn_Batch;
struct Object;
struct ViewLayer;

/* Used as ubo but colors can be directly referenced as well */
/* Keep in sync with: common_globals_lib.glsl (globalsBlock) */
typedef struct GlobalsUboStorage {
	/* UBOs data needs to be 16 byte aligned (size of vec4) */
	float colorWire[4];
	float colorWireEdit[4];
	float colorActive[4];
	float colorSelect[4];
	float colorTransform[4];
	float colorGroupActive[4];
	float colorGroupSelect[4];
	float colorGroup[4];
	float colorLibrarySelect[4];
	float colorLibrary[4];
	float colorLamp[4];
	float colorSpeaker[4];
	float colorCamera[4];
	float colorEmpty[4];
	float colorVertex[4];
	float colorVertexSelect[4];
	float colorEditMeshActive[4];
	float colorEdgeSelect[4];
	float colorEdgeSeam[4];
	float colorEdgeSharp[4];
	float colorEdgeCrease[4];
	float colorEdgeBWeight[4];
	float colorEdgeFaceSelect[4];
	float colorFace[4];
	float colorFaceSelect[4];
	float colorNormal[4];
	float colorVNormal[4];
	float colorLNormal[4];
	float colorFaceDot[4];

	float colorDeselect[4];
	float colorOutline[4];
	float colorLampNoAlpha[4];

	float colorBackground[4];

	float colorGrid[4];
	float colorGridEmphasise[4];
	float colorGridAxisX[4];
	float colorGridAxisY[4];
	float colorGridAxisZ[4];

	/* Pack individual float at the end of the buffer to avoid alignement errors */
	float sizeLampCenter, sizeLampCircle, sizeLampCircleShadow;
	float sizeVertex, sizeEdge, sizeEdgeFix, sizeFaceDot;
	float gridDistance, gridResolution, gridSubdivisions, gridScale;
} GlobalsUboStorage;
/* Keep in sync with globalsBlock in shaders */

void DRW_globals_update(void);

struct DRWShadingGroup *shgroup_dynlines_uniform_color(struct DRWPass *pass, float color[4]);
struct DRWShadingGroup *shgroup_dynpoints_uniform_color(struct DRWPass *pass, float color[4], float *size);
struct DRWShadingGroup *shgroup_groundlines_uniform_color(struct DRWPass *pass, float color[4]);
struct DRWShadingGroup *shgroup_groundpoints_uniform_color(struct DRWPass *pass, float color[4]);
struct DRWShadingGroup *shgroup_instance_screenspace(struct DRWPass *pass, struct Gwn_Batch *geom, float *size);
struct DRWShadingGroup *shgroup_instance_objspace_solid(struct DRWPass *pass, struct Gwn_Batch *geom, float (*obmat)[4]);
struct DRWShadingGroup *shgroup_instance_objspace_wire(struct DRWPass *pass, struct Gwn_Batch *geom, float (*obmat)[4]);
struct DRWShadingGroup *shgroup_instance_screen_aligned(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_instance_axis_names(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_instance_image_plane(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_instance_scaled(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_instance(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_camera_instance(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_distance_lines_instance(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_spot_instance(struct DRWPass *pass, struct Gwn_Batch *geom);
struct DRWShadingGroup *shgroup_instance_bone_envelope_wire(struct DRWPass *pass, struct Gwn_Batch *geom, float (*obmat)[4]);
struct DRWShadingGroup *shgroup_instance_bone_envelope_solid(struct DRWPass *pass, struct Gwn_Batch *geom, float (*obmat)[4]);
struct DRWShadingGroup *shgroup_instance_mball_helpers(struct DRWPass *pass, struct Gwn_Batch *geom);

int DRW_object_wire_theme_get(struct Object *ob, struct ViewLayer *view_layer, float **r_color);
float *DRW_color_background_blend_get(int theme_id);

/* draw_armature.c */
void DRW_shgroup_armature_object(
        struct Object *ob, struct ViewLayer *view_layer,
        struct DRWPass *pass_bone_solid, struct DRWPass *pass_bone_wire, struct DRWPass *pass_bone_envelope,
        struct DRWShadingGroup *shgrp_relationship_lines);

void DRW_shgroup_armature_pose(
        struct Object *ob,
        struct DRWPass *pass_bone_solid, struct DRWPass *pass_bone_wire, struct DRWPass *pass_bone_envelope,
        struct DRWShadingGroup *shgrp_relationship_lines);

void DRW_shgroup_armature_edit(
        struct Object *ob,
        struct DRWPass *pass_bone_solid, struct DRWPass *pass_bone_wire, struct DRWPass *pass_bone_envelope,
        struct DRWShadingGroup *shgrp_relationship_lines);

/* pose_mode.c */
bool DRW_pose_mode_armature(
    struct Object *ob, struct Object *active_ob);

#endif /* __DRAW_COMMON_H__ */
