/*
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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/glutil.c
 *  \ingroup edscr
 */


#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "GPU_basic_shader.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "UI_interface.h"

/* ******************************************** */

void setlinestyle(int nr)
{
	if (nr == 0) {
		glDisable(GL_LINE_STIPPLE);
	}
	else {
		
		glEnable(GL_LINE_STIPPLE);
		if (U.pixelsize > 1.0f)
			glLineStipple(nr, 0xCCCC);
		else
			glLineStipple(nr, 0xAAAA);
	}
}

/* Invert line handling */
	
#define GL_TOGGLE(mode, onoff)  (((onoff) ? glEnable : glDisable)(mode))

void set_inverted_drawing(int enable) 
{
	glLogicOp(enable ? GL_INVERT : GL_COPY);
	GL_TOGGLE(GL_COLOR_LOGIC_OP, enable);
	GL_TOGGLE(GL_DITHER, !enable);
}

float glaGetOneFloat(int param)
{
	GLfloat v;
	glGetFloatv(param, &v);
	return v;
}

int glaGetOneInt(int param)
{
	GLint v;
	glGetIntegerv(param, &v);
	return v;
}

void glaRasterPosSafe2f(float x, float y, float known_good_x, float known_good_y)
{
	GLubyte dummy = 0;

	/* As long as known good coordinates are correct
	 * this is guaranteed to generate an ok raster
	 * position (ignoring potential (real) overflow
	 * issues).
	 */
	glRasterPos2f(known_good_x, known_good_y);

	/* Now shift the raster position to where we wanted
	 * it in the first place using the glBitmap trick.
	 */
	glBitmap(0, 0, 0, 0, x - known_good_x, y - known_good_y, &dummy);
}

static int get_cached_work_texture(int *r_w, int *r_h)
{
	static GLint texid = -1;
	static int tex_w = 256;
	static int tex_h = 256;

	if (texid == -1) {
		glGenTextures(1, (GLuint *)&texid);

		glBindTexture(GL_TEXTURE_2D, texid);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	*r_w = tex_w;
	*r_h = tex_h;
	return texid;
}

static void immDrawPixelsTexSetupAttributes(IMMDrawPixelsTexState *state)
{
	Gwn_VertFormat *vert_format = immVertexFormat();
	state->pos = GWN_vertformat_attr_add(vert_format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	state->texco = GWN_vertformat_attr_add(vert_format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
}

/* To be used before calling immDrawPixelsTex
 * Default shader is GPU_SHADER_2D_IMAGE_COLOR
 * You can still set uniforms with :
 * GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "name"), 0);
 * */
IMMDrawPixelsTexState immDrawPixelsTexSetup(int builtin)
{
	IMMDrawPixelsTexState state;
	immDrawPixelsTexSetupAttributes(&state);

	state.shader = GPU_shader_get_builtin_shader(builtin);

	/* Shader will be unbind by immUnbindProgram in immDrawPixelsTexScaled_clipping */
	immBindBuiltinProgram(builtin);
	immUniform1i("image", 0);
	state.do_shader_unbind = true;

	return state;
}

/* Use the currently bound shader.
 *
 * Use immDrawPixelsTexSetup to bind the shader you
 * want before calling immDrawPixelsTex.
 *
 * If using a special shader double check it uses the same
 * attributes "pos" "texCoord" and uniform "image".
 *
 * If color is NULL then use white by default
 *
 * Be also aware that this function unbinds the shader when
 * it's finished.
 * */
void immDrawPixelsTexScaled_clipping(IMMDrawPixelsTexState *state,
                                     float x, float y, int img_w, int img_h,
                                     int format, int type, int zoomfilter, void *rect,
                                     float scaleX, float scaleY,
                                     float clip_min_x, float clip_min_y,
                                     float clip_max_x, float clip_max_y,
                                     float xzoom, float yzoom, float color[4])
{
	unsigned char *uc_rect = (unsigned char *) rect;
	const float *f_rect = (float *)rect;
	int subpart_x, subpart_y, tex_w, tex_h;
	int seamless, offset_x, offset_y, nsubparts_x, nsubparts_y;
	int texid = get_cached_work_texture(&tex_w, &tex_h);
	int components;
	const bool use_clipping = ((clip_min_x < clip_max_x) && (clip_min_y < clip_max_y));
	float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	GLint unpack_row_length;
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row_length);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, img_w);
	glBindTexture(GL_TEXTURE_2D, texid);

	/* don't want nasty border artifacts */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, zoomfilter);

	/* setup seamless 2=on, 0=off */
	seamless = ((tex_w < img_w || tex_h < img_h) && tex_w > 2 && tex_h > 2) ? 2 : 0;

	offset_x = tex_w - seamless;
	offset_y = tex_h - seamless;

	nsubparts_x = (img_w + (offset_x - 1)) / (offset_x);
	nsubparts_y = (img_h + (offset_y - 1)) / (offset_y);

	if (format == GL_RGBA)
		components = 4;
	else if (format == GL_RGB)
		components = 3;
	else if (format == GL_RED)
		components = 1;
	else {
		BLI_assert(!"Incompatible format passed to glaDrawPixelsTexScaled");
		return;
	}

	if (type == GL_FLOAT) {
		/* need to set internal format to higher range float */

		/* NOTE: this could fail on some drivers, like mesa,
		 *       but currently this code is only used by color
		 *       management stuff which already checks on whether
		 *       it's possible to use GL_RGBA16F_ARB
		 */

		/* TODO viewport : remove extension */
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, tex_w, tex_h, 0, format, GL_FLOAT, NULL);
	}
	else {
		/* switch to 8bit RGBA for byte buffer */
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, format, GL_UNSIGNED_BYTE, NULL);
	}

	unsigned int pos = state->pos, texco = state->texco;

	/* optional */
	/* NOTE: Shader could be null for GLSL OCIO drawing, it is fine, since
	 * it does not need color.
	 */
	if (state->shader != NULL && GPU_shader_get_uniform(state->shader, "color") != -1) {
		immUniformColor4fv((color) ? color : white);
	}

	for (subpart_y = 0; subpart_y < nsubparts_y; subpart_y++) {
		for (subpart_x = 0; subpart_x < nsubparts_x; subpart_x++) {
			int remainder_x = img_w - subpart_x * offset_x;
			int remainder_y = img_h - subpart_y * offset_y;
			int subpart_w = (remainder_x < tex_w) ? remainder_x : tex_w;
			int subpart_h = (remainder_y < tex_h) ? remainder_y : tex_h;
			int offset_left = (seamless && subpart_x != 0) ? 1 : 0;
			int offset_bot = (seamless && subpart_y != 0) ? 1 : 0;
			int offset_right = (seamless && remainder_x > tex_w) ? 1 : 0;
			int offset_top = (seamless && remainder_y > tex_h) ? 1 : 0;
			float rast_x = x + subpart_x * offset_x * xzoom;
			float rast_y = y + subpart_y * offset_y * yzoom;
			/* check if we already got these because we always get 2 more when doing seamless */
			if (subpart_w <= seamless || subpart_h <= seamless)
				continue;

			if (use_clipping) {
				if (rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX < clip_min_x ||
				    rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY < clip_min_y)
				{
					continue;
				}
				if (rast_x + (float)offset_left * xzoom > clip_max_x ||
				    rast_y + (float)offset_bot * yzoom > clip_max_y)
				{
					continue;
				}
			}

			if (type == GL_FLOAT) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, format, GL_FLOAT, &f_rect[((size_t)subpart_y) * offset_y * img_w * components + subpart_x * offset_x * components]);

				/* add an extra border of pixels so linear looks ok at edges of full image */
				if (subpart_w < tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, format, GL_FLOAT, &f_rect[((size_t)subpart_y) * offset_y * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
				if (subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, format, GL_FLOAT, &f_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + subpart_x * offset_x * components]);
				if (subpart_w < tex_w && subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, format, GL_FLOAT, &f_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
			}
			else {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, format, GL_UNSIGNED_BYTE, &uc_rect[((size_t)subpart_y) * offset_y * img_w * components + subpart_x * offset_x * components]);

				if (subpart_w < tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, format, GL_UNSIGNED_BYTE, &uc_rect[((size_t)subpart_y) * offset_y * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
				if (subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, format, GL_UNSIGNED_BYTE, &uc_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + subpart_x * offset_x * components]);
				if (subpart_w < tex_w && subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, format, GL_UNSIGNED_BYTE, &uc_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
			}

			immBegin(GWN_PRIM_TRI_FAN, 4);
			immAttrib2f(texco, (float)(0 + offset_left) / tex_w, (float)(0 + offset_bot) / tex_h);
			immVertex2f(pos, rast_x + (float)offset_left * xzoom, rast_y + (float)offset_bot * yzoom);

			immAttrib2f(texco, (float)(subpart_w - offset_right) / tex_w, (float)(0 + offset_bot) / tex_h);
			immVertex2f(pos, rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX, rast_y + (float)offset_bot * yzoom);

			immAttrib2f(texco, (float)(subpart_w - offset_right) / tex_w, (float)(subpart_h - offset_top) / tex_h);
			immVertex2f(pos, rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX, rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);

			immAttrib2f(texco, (float)(0 + offset_left) / tex_w, (float)(subpart_h - offset_top) / tex_h);
			immVertex2f(pos, rast_x + (float)offset_left * xzoom, rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);
			immEnd();
		}
	}

	if (state->do_shader_unbind) {
		immUnbindProgram();
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_length);
}

void immDrawPixelsTexScaled(IMMDrawPixelsTexState *state,
                            float x, float y, int img_w, int img_h,
                            int format, int type, int zoomfilter, void *rect,
                            float scaleX, float scaleY, float xzoom, float yzoom, float color[4])
{
	immDrawPixelsTexScaled_clipping(state, x, y, img_w, img_h, format, type, zoomfilter, rect,
	                                scaleX, scaleY, 0.0f, 0.0f, 0.0f, 0.0f, xzoom, yzoom, color);
}

void immDrawPixelsTex(IMMDrawPixelsTexState *state,
                      float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect,
                      float xzoom, float yzoom, float color[4])
{
	immDrawPixelsTexScaled_clipping(state, x, y, img_w, img_h, format, type, zoomfilter, rect, 1.0f, 1.0f,
	                                0.0f, 0.0f, 0.0f, 0.0f, xzoom, yzoom, color);
}

void immDrawPixelsTex_clipping(IMMDrawPixelsTexState *state,
                               float x, float y, int img_w, int img_h,
                               int format, int type, int zoomfilter, void *rect,
                               float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y,
                               float xzoom, float yzoom, float color[4])
{
	immDrawPixelsTexScaled_clipping(state, x, y, img_w, img_h, format, type, zoomfilter, rect, 1.0f, 1.0f,
	                                clip_min_x, clip_min_y, clip_max_x, clip_max_y, xzoom, yzoom, color);
}

/* 2D Drawing Assistance */

void glaDefine2DArea(rcti *screen_rect)
{
	const int sc_w = BLI_rcti_size_x(screen_rect) + 1;
	const int sc_h = BLI_rcti_size_y(screen_rect) + 1;

	glViewport(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);
	glScissor(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);

	/* The GLA_PIXEL_OFS magic number is to shift the matrix so that
	 * both raster and vertex integer coordinates fall at pixel
	 * centers properly. For a longer discussion see the OpenGL
	 * Programming Guide, Appendix H, Correctness Tips.
	 */

	gpuOrtho2D(GLA_PIXEL_OFS, sc_w + GLA_PIXEL_OFS, GLA_PIXEL_OFS, sc_h + GLA_PIXEL_OFS);
	gpuLoadIdentity();
}

/* TODO(merwin): put the following 2D code to use, or build new 2D code inspired & informd by it */

#if 0 /* UNUSED */

struct gla2DDrawInfo {
	int orig_vp[4], orig_sc[4];
	float orig_projmat[16], orig_viewmat[16];

	rcti screen_rect;
	rctf world_rect;

	float wo_to_sc[2];
};

void gla2DGetMap(gla2DDrawInfo *di, rctf *rect) 
{
	*rect = di->world_rect;
}

void gla2DSetMap(gla2DDrawInfo *di, rctf *rect) 
{
	int sc_w, sc_h;
	float wo_w, wo_h;

	di->world_rect = *rect;
	
	sc_w = BLI_rcti_size_x(&di->screen_rect);
	sc_h = BLI_rcti_size_y(&di->screen_rect);
	wo_w = BLI_rcti_size_x(&di->world_rect);
	wo_h = BLI_rcti_size_y(&di->world_rect);
	
	di->wo_to_sc[0] = sc_w / wo_w;
	di->wo_to_sc[1] = sc_h / wo_h;
}

/** Save the current OpenGL state and initialize OpenGL for 2D
 * rendering. glaEnd2DDraw should be called on the returned structure
 * to free it and to return OpenGL to its previous state. The
 * scissor rectangle is set to match the viewport.
 *
 * See glaDefine2DArea for an explanation of why this function uses integers.
 *
 * \param screen_rect The screen rectangle to be used for 2D drawing.
 * \param world_rect The world rectangle that the 2D area represented
 * by \a screen_rect is supposed to represent. If NULL it is assumed the
 * world has a 1 to 1 mapping to the screen.
 */
gla2DDrawInfo *glaBegin2DDraw(rcti *screen_rect, rctf *world_rect) 
{
	gla2DDrawInfo *di = MEM_mallocN(sizeof(*di), "gla2DDrawInfo");
	int sc_w, sc_h;
	float wo_w, wo_h;

	glGetIntegerv(GL_VIEWPORT, (GLint *)di->orig_vp);
	glGetIntegerv(GL_SCISSOR_BOX, (GLint *)di->orig_sc);
	gpuGetProjectionMatrix(di->orig_projmat);
	gpuGetModelViewMatrix(di->orig_viewmat);

	di->screen_rect = *screen_rect;
	if (world_rect) {
		di->world_rect = *world_rect;
	}
	else {
		di->world_rect.xmin = di->screen_rect.xmin;
		di->world_rect.ymin = di->screen_rect.ymin;
		di->world_rect.xmax = di->screen_rect.xmax;
		di->world_rect.ymax = di->screen_rect.ymax;
	}

	sc_w = BLI_rcti_size_x(&di->screen_rect);
	sc_h = BLI_rcti_size_y(&di->screen_rect);
	wo_w = BLI_rcti_size_x(&di->world_rect);
	wo_h = BLI_rcti_size_y(&di->world_rect);

	di->wo_to_sc[0] = sc_w / wo_w;
	di->wo_to_sc[1] = sc_h / wo_h;

	glaDefine2DArea(&di->screen_rect);

	return di;
}

/**
 * Translate the (\a wo_x, \a wo_y) point from world coordinates into screen space.
 */
void gla2DDrawTranslatePt(gla2DDrawInfo *di, float wo_x, float wo_y, int *r_sc_x, int *r_sc_y)
{
	*r_sc_x = (wo_x - di->world_rect.xmin) * di->wo_to_sc[0];
	*r_sc_y = (wo_y - di->world_rect.ymin) * di->wo_to_sc[1];
}

/**
 * Translate the \a world point from world coordinates into screen space.
 */
void gla2DDrawTranslatePtv(gla2DDrawInfo *di, float world[2], int r_screen[2])
{
	screen_r[0] = (world[0] - di->world_rect.xmin) * di->wo_to_sc[0];
	screen_r[1] = (world[1] - di->world_rect.ymin) * di->wo_to_sc[1];
}

/**
 * Restores the previous OpenGL state and frees the auxiliary gla data.
 */
void glaEnd2DDraw(gla2DDrawInfo *di)
{
	glViewport(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	glScissor(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	gpuLoadProjectionMatrix(di->orig_projmat);
	gpuLoadMatrix(di->orig_viewmat);

	MEM_freeN(di);
}

#endif /* UNUSED */


/* *************** glPolygonOffset hack ************* */

/**
 * \note \a viewdist is only for ortho at the moment.
 */
void bglPolygonOffset(float viewdist, float dist)
{
	static float winmat[16], offset = 0.0f;
	
	if (dist != 0.0f) {
		float offs;
		
		// glEnable(GL_POLYGON_OFFSET_FILL);
		// glPolygonOffset(-1.0, -1.0);

		/* hack below is to mimic polygon offset */
		gpuGetProjectionMatrix(winmat);
		
		/* dist is from camera to center point */
		
		if (winmat[15] > 0.5f) {
#if 1
			offs = 0.00001f * dist * viewdist;  // ortho tweaking
#else
			static float depth_fac = 0.0f;
			if (depth_fac == 0.0f) {
				int depthbits;
				glGetIntegerv(GL_DEPTH_BITS, &depthbits);
				depth_fac = 1.0f / (float)((1 << depthbits) - 1);
			}
			offs = (-1.0 / winmat[10]) * dist * depth_fac;

			UNUSED_VARS(viewdist);
#endif
		}
		else {
			/* This adjustment effectively results in reducing the Z value by 0.25%.
			 *
			 * winmat[14] actually evaluates to `-2 * far * near / (far - near)`,
			 * is very close to -0.2 with default clip range, and is used as the coefficient multiplied by `w / z`,
			 * thus controlling the z dependent part of the depth value.
			 */
			offs = winmat[14] * -0.0025f * dist;
		}
		
		winmat[14] -= offs;
		offset += offs;
	}
	else {
		winmat[14] += offset;
		offset = 0.0;
	}

	gpuLoadProjectionMatrix(winmat);
}

/* **** Color management helper functions for GLSL display/transform ***** */

/* Draw given image buffer on a screen using GLSL for display transform */
void glaDrawImBuf_glsl_clipping(ImBuf *ibuf, float x, float y, int zoomfilter,
                                ColorManagedViewSettings *view_settings,
                                ColorManagedDisplaySettings *display_settings,
                                float clip_min_x, float clip_min_y,
                                float clip_max_x, float clip_max_y,
                                float zoom_x, float zoom_y)
{
	bool force_fallback = false;
	bool need_fallback = true;

	/* Early out */
	if (ibuf->rect == NULL && ibuf->rect_float == NULL)
		return;

	/* Single channel images could not be transformed using GLSL yet */
	force_fallback |= ibuf->channels == 1;

	/* If user decided not to use GLSL, fallback to glaDrawPixelsAuto */
	force_fallback |= (U.image_draw_method != IMAGE_DRAW_METHOD_GLSL);

	/* Try to draw buffer using GLSL display transform */
	if (force_fallback == false) {
		int ok;

		IMMDrawPixelsTexState state = {0};
		/* We want GLSL state to be fully handled by OCIO. */
		state.do_shader_unbind = false;
		immDrawPixelsTexSetupAttributes(&state);

		if (ibuf->rect_float) {
			if (ibuf->float_colorspace) {
				ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings, display_settings,
				                                                    ibuf->float_colorspace,
				                                                    ibuf->dither, true);
			}
			else {
				ok = IMB_colormanagement_setup_glsl_draw(view_settings, display_settings,
				                                         ibuf->dither, true);
			}
		}
		else {
			ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings, display_settings,
			                                                    ibuf->rect_colorspace,
			                                                    ibuf->dither, false);
		}

		if (ok) {
			if (ibuf->rect_float) {
				int format = 0;

				if (ibuf->channels == 3)
					format = GL_RGB;
				else if (ibuf->channels == 4)
					format = GL_RGBA;
				else
					BLI_assert(!"Incompatible number of channels for GLSL display");

				if (format != 0) {
					immDrawPixelsTex_clipping(&state,
					                          x, y, ibuf->x, ibuf->y, format, GL_FLOAT,
					                          zoomfilter, ibuf->rect_float,
					                          clip_min_x, clip_min_y, clip_max_x, clip_max_y,
					                          zoom_x, zoom_y, NULL);
				}
			}
			else if (ibuf->rect) {
				/* ibuf->rect is always RGBA */
				immDrawPixelsTex_clipping(&state,
				                          x, y, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE,
				                          zoomfilter, ibuf->rect,
				                          clip_min_x, clip_min_y, clip_max_x, clip_max_y,
				                          zoom_x, zoom_y, NULL);
			}

			IMB_colormanagement_finish_glsl_draw();

			need_fallback = false;
		}
	}

	/* In case GLSL failed or not usable, fallback to glaDrawPixelsAuto */
	if (need_fallback) {
		unsigned char *display_buffer;
		void *cache_handle;

		display_buffer = IMB_display_buffer_acquire(ibuf, view_settings, display_settings, &cache_handle);

		if (display_buffer) {
			IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
			immDrawPixelsTex_clipping(&state,
			                          x, y, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE,
			                          zoomfilter, display_buffer,
			                          clip_min_x, clip_min_y, clip_max_x, clip_max_y,
			                          zoom_x, zoom_y, NULL);
		}

		IMB_display_buffer_release(cache_handle);
	}
}

void glaDrawImBuf_glsl(ImBuf *ibuf, float x, float y, int zoomfilter,
                       ColorManagedViewSettings *view_settings,
                       ColorManagedDisplaySettings *display_settings,
                       float zoom_x, float zoom_y)
{
	glaDrawImBuf_glsl_clipping(ibuf, x, y, zoomfilter, view_settings, display_settings,
	                           0.0f, 0.0f, 0.0f, 0.0f, zoom_x, zoom_y);
}

void glaDrawImBuf_glsl_ctx_clipping(const bContext *C,
                                    ImBuf *ibuf,
                                    float x, float y,
                                    int zoomfilter,
                                    float clip_min_x, float clip_min_y,
                                    float clip_max_x, float clip_max_y,
                                    float zoom_x, float zoom_y)
{
	ColorManagedViewSettings *view_settings;
	ColorManagedDisplaySettings *display_settings;

	IMB_colormanagement_display_settings_from_ctx(C, &view_settings, &display_settings);

	glaDrawImBuf_glsl_clipping(ibuf, x, y, zoomfilter, view_settings, display_settings,
	                           clip_min_x, clip_min_y, clip_max_x, clip_max_y,
	                           zoom_x, zoom_y);
}

void glaDrawImBuf_glsl_ctx(const bContext *C, ImBuf *ibuf, float x, float y, int zoomfilter,
                           float zoom_x, float zoom_y)
{
	glaDrawImBuf_glsl_ctx_clipping(C, ibuf, x, y, zoomfilter, 0.0f, 0.0f, 0.0f, 0.0f, zoom_x, zoom_y);
}

/* don't move to GPU_immediate_util.h because this uses user-prefs
 * and isn't very low level */
void immDrawBorderCorners(unsigned int pos, const rcti *border, float zoomx, float zoomy)
{
	float delta_x = 4.0f * UI_DPI_FAC / zoomx;
	float delta_y = 4.0f * UI_DPI_FAC / zoomy;

	delta_x = min_ff(delta_x, border->xmax - border->xmin);
	delta_y = min_ff(delta_y, border->ymax - border->ymin);

	/* left bottom corner */
	immBegin(GWN_PRIM_LINE_STRIP, 3);
	immVertex2f(pos, border->xmin, border->ymin + delta_y);
	immVertex2f(pos, border->xmin, border->ymin);
	immVertex2f(pos, border->xmin + delta_x, border->ymin);
	immEnd();

	/* left top corner */
	immBegin(GWN_PRIM_LINE_STRIP, 3);
	immVertex2f(pos, border->xmin, border->ymax - delta_y);
	immVertex2f(pos, border->xmin, border->ymax);
	immVertex2f(pos, border->xmin + delta_x, border->ymax);
	immEnd();

	/* right bottom corner */
	immBegin(GWN_PRIM_LINE_STRIP, 3);
	immVertex2f(pos, border->xmax - delta_x, border->ymin);
	immVertex2f(pos, border->xmax, border->ymin);
	immVertex2f(pos, border->xmax, border->ymin + delta_y);
	immEnd();

	/* right top corner */
	immBegin(GWN_PRIM_LINE_STRIP, 3);
	immVertex2f(pos, border->xmax - delta_x, border->ymax);
	immVertex2f(pos, border->xmax, border->ymax);
	immVertex2f(pos, border->xmax, border->ymax - delta_y);
	immEnd();
}
