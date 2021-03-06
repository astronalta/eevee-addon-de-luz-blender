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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_draw.c
 *  \ingroup wm
 *
 * Handle OpenGL buffers for windowing, also paint cursor.
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_image.h"

#include "GHOST_C-api.h"

#include "ED_node.h"
#include "ED_view3d.h"
#include "ED_screen.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_immediate.h"
#include "GPU_viewport.h"

#include "RE_engine.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_draw.h"
#include "wm_window.h"
#include "wm_event_system.h"

#ifdef WITH_OPENSUBDIV
#  include "BKE_subsurf.h"
#endif

/* swap */
#define WIN_NONE_OK     0
#define WIN_BACK_OK     1
#define WIN_FRONT_OK    2
#define WIN_BOTH_OK     3

/* ******************* drawing, overlays *************** */


static void wm_paintcursor_draw(bContext *C, ARegion *ar)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	
	if (wm->paintcursors.first) {
		wmWindow *win = CTX_wm_window(C);
		bScreen *screen = WM_window_get_active_screen(win);
		wmPaintCursor *pc;

		if (ar->swinid && screen->subwinactive == ar->swinid) {
			for (pc = wm->paintcursors.first; pc; pc = pc->next) {
				if (pc->poll == NULL || pc->poll(C)) {
					ARegion *ar_other = CTX_wm_region(C);
					if (ELEM(win->grabcursor, GHOST_kGrabWrap, GHOST_kGrabHide)) {
						int x = 0, y = 0;
						wm_get_cursor_position(win, &x, &y);
						pc->draw(C,
						         x - ar_other->winrct.xmin,
						         y - ar_other->winrct.ymin,
						         pc->customdata);
					}
					else {
						pc->draw(C,
						         win->eventstate->x - ar_other->winrct.xmin,
						         win->eventstate->y - ar_other->winrct.ymin,
						         pc->customdata);
					}
				}
			}
		}
	}
}

/* ********************* drawing, swap ****************** */

static void wm_area_mark_invalid_backbuf(ScrArea *sa)
{
	if (sa->spacetype == SPACE_VIEW3D)
		((View3D *)sa->spacedata.first)->flag |= V3D_INVALID_BACKBUF;
}

static bool wm_area_test_invalid_backbuf(ScrArea *sa)
{
	if (sa->spacetype == SPACE_VIEW3D)
		return (((View3D *)sa->spacedata.first)->flag & V3D_INVALID_BACKBUF) != 0;
	else
		return true;
}

static void wm_region_test_render_do_draw(const Scene *scene, ScrArea *sa, ARegion *ar)
{
	/* tag region for redraw from render engine preview running inside of it */
	if (sa->spacetype == SPACE_VIEW3D) {
		RegionView3D *rv3d = ar->regiondata;
		RenderEngine *engine = (rv3d) ? rv3d->render_engine : NULL;
		GPUViewport *viewport = (rv3d) ? rv3d->viewport : NULL;

		if (engine && (engine->flag & RE_ENGINE_DO_DRAW)) {
			View3D *v3d = sa->spacedata.first;
			rcti border_rect;

			/* do partial redraw when possible */
			if (ED_view3d_calc_render_border(scene, v3d, ar, &border_rect))
				ED_region_tag_redraw_partial(ar, &border_rect);
			else
				ED_region_tag_redraw(ar);

			engine->flag &= ~RE_ENGINE_DO_DRAW;
		}
		else if (viewport && GPU_viewport_do_update(viewport)) {
			ED_region_tag_redraw(ar);
		}
	}
}

/********************** draw all **************************/
/* - reference method, draw all each time                 */

static void wm_method_draw_full(bContext *C, wmWindow *win)
{
	bScreen *screen = WM_window_get_active_screen(win);
	ScrArea *sa;
	ARegion *ar;

	/* draw area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				ar->do_draw = false;
				wm_paintcursor_draw(C, ar);
				CTX_wm_region_set(C, NULL);
			}
		}
		
		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	ED_screen_draw(win);
	screen->do_draw = false;

	/* draw overlapping regions */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	if (screen->do_draw_gesture)
		wm_gesture_draw(win);
}

/****************** draw overlap all **********************/
/* - redraw marked areas, and anything that overlaps it   */
/* - it also handles swap exchange optionally, assuming   */
/*   that on swap no clearing happens and we get back the */
/*   same buffer as we swapped to the front               */

/* mark area-regions to redraw if overlapped with rect */
static void wm_flush_regions_down(bScreen *screen, rcti *dirty)
{
	ScrArea *sa;
	ARegion *ar;

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (BLI_rcti_isect(dirty, &ar->winrct, NULL)) {
				ar->do_draw = RGN_DRAW;
				memset(&ar->drawrct, 0, sizeof(ar->drawrct));
				ar->swap = WIN_NONE_OK;
			}
		}
	}
}

/* mark menu-regions to redraw if overlapped with rect */
static void wm_flush_regions_up(bScreen *screen, rcti *dirty)
{
	ARegion *ar;
	
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (BLI_rcti_isect(dirty, &ar->winrct, NULL)) {
			ar->do_draw = RGN_DRAW;
			memset(&ar->drawrct, 0, sizeof(ar->drawrct));
			ar->swap = WIN_NONE_OK;
		}
	}
}

static void wm_method_draw_overlap_all(bContext *C, wmWindow *win, int exchange)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	bScreen *screen = WM_window_get_active_screen(win);
	ScrArea *sa;
	ARegion *ar;
	static rcti rect = {0, 0, 0, 0};

	/* after backbuffer selection draw, we need to redraw */
	for (sa = screen->areabase.first; sa; sa = sa->next)
		for (ar = sa->regionbase.first; ar; ar = ar->next)
			if (ar->swinid && !wm_area_test_invalid_backbuf(sa))
				ED_region_tag_redraw(ar);

	/* flush overlapping regions */
	if (screen->regionbase.first) {
		/* flush redraws of area regions up to overlapping regions */
		for (sa = screen->areabase.first; sa; sa = sa->next)
			for (ar = sa->regionbase.first; ar; ar = ar->next)
				if (ar->swinid && ar->do_draw)
					wm_flush_regions_up(screen, &ar->winrct);
		
		/* flush between overlapping regions */
		for (ar = screen->regionbase.last; ar; ar = ar->prev)
			if (ar->swinid && ar->do_draw)
				wm_flush_regions_up(screen, &ar->winrct);
		
		/* flush redraws of overlapping regions down to area regions */
		for (ar = screen->regionbase.last; ar; ar = ar->prev)
			if (ar->swinid && ar->do_draw)
				wm_flush_regions_down(screen, &ar->winrct);
	}

	/* flush drag item */
	if (rect.xmin != rect.xmax) {
		wm_flush_regions_down(screen, &rect);
		rect.xmin = rect.xmax = 0;
	}
	if (wm->drags.first) {
		/* doesnt draw, fills rect with boundbox */
		wm_drags_draw(C, win, &rect);
	}
	
	/* draw marked area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid) {
				if (ar->do_draw) {
					CTX_wm_region_set(C, ar);
					ED_region_do_draw(C, ar);
					ar->do_draw = false;
					wm_paintcursor_draw(C, ar);
					CTX_wm_region_set(C, NULL);

					if (exchange)
						ar->swap = WIN_FRONT_OK;
				}
				else if (exchange) {
					if (ar->swap == WIN_FRONT_OK) {
						CTX_wm_region_set(C, ar);
						ED_region_do_draw(C, ar);
						ar->do_draw = false;
						wm_paintcursor_draw(C, ar);
						CTX_wm_region_set(C, NULL);

						ar->swap = WIN_BOTH_OK;
					}
					else if (ar->swap == WIN_BACK_OK)
						ar->swap = WIN_FRONT_OK;
					else if (ar->swap == WIN_BOTH_OK)
						ar->swap = WIN_BOTH_OK;
				}
			}
		}

		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	/* after area regions so we can do area 'overlay' drawing */
	if (screen->do_draw) {
		ED_screen_draw(win);
		screen->do_draw = false;

		if (exchange)
			screen->swap = WIN_FRONT_OK;
	}
	else if (exchange) {
		if (screen->swap == WIN_FRONT_OK) {
			ED_screen_draw(win);
			screen->do_draw = false;
			screen->swap = WIN_BOTH_OK;
		}
		else if (screen->swap == WIN_BACK_OK)
			screen->swap = WIN_FRONT_OK;
		else if (screen->swap == WIN_BOTH_OK)
			screen->swap = WIN_BOTH_OK;
	}

	/* draw marked overlapping regions */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid && ar->do_draw) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	if (screen->do_draw_gesture)
		wm_gesture_draw(win);
	
	/* needs pixel coords in screen */
	if (wm->drags.first) {
		wm_drags_draw(C, win, NULL);
	}
}

/****************** draw triple buffer ********************/
/* - area regions are written into a texture, without any */
/*   of the overlapping menus, brushes, gestures. these   */
/*   are redrawn each time.                               */

static void wm_draw_triple_free(wmDrawTriple *triple)
{
	if (triple) {
		glDeleteTextures(1, &triple->bind);
		MEM_freeN(triple);
	}
}

static void wm_draw_triple_fail(bContext *C, wmWindow *win)
{
	wm_draw_window_clear(win);

	win->drawfail = true;
	wm_method_draw_overlap_all(C, win, 0);
}

static bool wm_triple_gen_textures(wmWindow *win, wmDrawTriple *triple)
{
	/* compute texture sizes */
	triple->x = WM_window_pixels_x(win);
	triple->y = WM_window_pixels_y(win);

#if USE_TEXTURE_RECTANGLE
	/* GL_TEXTURE_RECTANGLE is part of GL 3.1 so we can use it soon without runtime checks */
	triple->target = GL_TEXTURE_RECTANGLE;
#else
	triple->target = GL_TEXTURE_2D;
#endif

	/* generate texture names */
	glGenTextures(1, &triple->bind);

	/* proxy texture is only guaranteed to test for the cases that
	 * there is only one texture in use, which may not be the case */
	const GLint maxsize = GPU_max_texture_size();

	if (triple->x > maxsize || triple->y > maxsize) {
		printf("WM: failed to allocate texture for triple buffer drawing "
		       "(texture too large for graphics card).\n");
		return false;
	}

	/* setup actual texture */
	glBindTexture(triple->target, triple->bind);

	/* no mipmaps */
#if USE_TEXTURE_RECTANGLE
	/* already has no mipmaps */
#else
	glTexParameteri(triple->target, GL_TEXTURE_MAX_LEVEL, 0);
	/* GL_TEXTURE_BASE_LEVEL = 0 by default */
#endif

	glTexParameteri(triple->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(triple->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(triple->target, 0, GL_RGB8, triple->x, triple->y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glBindTexture(triple->target, 0);

	return true;
}

void wm_triple_draw_textures(wmWindow *win, wmDrawTriple *triple, float alpha)
{
	const int sizex = WM_window_pixels_x(win);
	const int sizey = WM_window_pixels_y(win);

	/* wmOrtho for the screen has this same offset */
	float ratiox = sizex;
	float ratioy = sizey;
	float halfx = GLA_PIXEL_OFS;
	float halfy = GLA_PIXEL_OFS;

#if USE_TEXTURE_RECTANGLE
	/* texture rectangle has unnormalized coordinates */
#else
	ratiox /= triple->x;
	ratioy /= triple->y;
	halfx /= triple->x;
	halfy /= triple->y;
#endif

	Gwn_VertFormat *format = immVertexFormat();
	unsigned int texcoord = GWN_vertformat_attr_add(format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	const int activeTex = 7; /* arbitrary */
	glActiveTexture(GL_TEXTURE0 + activeTex);
	glBindTexture(triple->target, triple->bind);

#if USE_TEXTURE_RECTANGLE
	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_RECT_MODULATE_ALPHA);
#else
	immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_MODULATE_ALPHA);
	/* TODO: make pure 2D version
	 * and a 2D_IMAGE (replace, not modulate) version for when alpha = 1.0
	 */
#endif
	immUniform1f("alpha", alpha);
	immUniform1i("image", activeTex);

	immBegin(GWN_PRIM_TRI_FAN, 4);

	immAttrib2f(texcoord, halfx, halfy);
	immVertex2f(pos, 0.0f, 0.0f);

	immAttrib2f(texcoord, ratiox + halfx, halfy);
	immVertex2f(pos, sizex, 0.0f);

	immAttrib2f(texcoord, ratiox + halfx, ratioy + halfy);
	immVertex2f(pos, sizex, sizey);

	immAttrib2f(texcoord, halfx, ratioy + halfy);
	immVertex2f(pos, 0.0f, sizey);

	immEnd();
	immUnbindProgram();

	glBindTexture(triple->target, 0);
	if (activeTex != 0)
		glActiveTexture(GL_TEXTURE0);
}

static void wm_triple_copy_textures(wmWindow *win, wmDrawTriple *triple)
{
	const int sizex = WM_window_pixels_x(win);
	const int sizey = WM_window_pixels_y(win);

	glBindTexture(triple->target, triple->bind);
	/* what is GL_READ_BUFFER right now? */
	glCopyTexSubImage2D(triple->target, 0, 0, 0, 0, 0, sizex, sizey);
	glBindTexture(triple->target, 0);
}

static void wm_draw_region_blend(wmWindow *win, ARegion *ar, wmDrawTriple *triple)
{
	float fac = ED_region_blend_factor(ar);
	
	/* region blend always is 1, except when blend timer is running */
	if (fac < 1.0f) {
		bScreen *screen = WM_window_get_active_screen(win);

		wmSubWindowScissorSet(win, screen->mainwin, &ar->winrct, true);

		glEnable(GL_BLEND);
		wm_triple_draw_textures(win, triple, 1.0f - fac);
		glDisable(GL_BLEND);
	}
}

static void wm_method_draw_triple(bContext *C, wmWindow *win)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmDrawData *dd, *dd_next, *drawdata = win->drawdata.first;
	bScreen *screen = WM_window_get_active_screen(win);
	ScrArea *sa;
	ARegion *ar;
	bool copytex = false;

	if (drawdata && drawdata->triple) {
#if 0 /* why do we need to clear before overwriting? */
		glClearColor(1, 1, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif

		wmSubWindowSet(win, screen->mainwin);

		wm_triple_draw_textures(win, drawdata->triple, 1.0f);
	}
	else {
		/* we run it when we start OR when we turn stereo on */
		if (drawdata == NULL) {
			drawdata = MEM_callocN(sizeof(wmDrawData), "wmDrawData");
			BLI_addhead(&win->drawdata, drawdata);
		}

		drawdata->triple = MEM_callocN(sizeof(wmDrawTriple), "wmDrawTriple");

		if (!wm_triple_gen_textures(win, drawdata->triple)) {
			wm_draw_triple_fail(C, win);
			return;
		}
	}

	/* it means stereo was just turned off */
	/* note: we are removing all drawdatas that are not the first */
	for (dd = drawdata->next; dd; dd = dd_next) {
		dd_next = dd->next;

		BLI_remlink(&win->drawdata, dd);
		wm_draw_triple_free(dd->triple);
		MEM_freeN(dd);
	}

	wmDrawTriple *triple = drawdata->triple;

	/* draw marked area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->do_draw) {
				if (ar->overlap == false) {
					CTX_wm_region_set(C, ar);
					ED_region_do_draw(C, ar);
					ar->do_draw = false;
					CTX_wm_region_set(C, NULL);
					copytex = true;
				}
			}
		}

		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	if (copytex) {
		wmSubWindowSet(win, screen->mainwin);

		wm_triple_copy_textures(win, triple);
	}

	if (wm->paintcursors.first) {
		for (sa = screen->areabase.first; sa; sa = sa->next) {
			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->swinid && ar->swinid == screen->subwinactive) {
					CTX_wm_area_set(C, sa);
					CTX_wm_region_set(C, ar);

					/* make region ready for draw, scissor, pixelspace */
					ED_region_set(C, ar);
					wm_paintcursor_draw(C, ar);

					CTX_wm_region_set(C, NULL);
					CTX_wm_area_set(C, NULL);
				}
			}
		}

		wmSubWindowSet(win, screen->mainwin);
	}

	/* draw overlapping area regions (always like popups) */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->overlap) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				ar->do_draw = false;
				CTX_wm_region_set(C, NULL);

				wm_draw_region_blend(win, ar, triple);
			}
		}

		CTX_wm_area_set(C, NULL);
	}

	/* after area regions so we can do area 'overlay' drawing */
	ED_screen_draw(win);
	WM_window_get_active_screen(win)->do_draw = false;

	/* draw floating regions (menus) */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	/* always draw, not only when screen tagged */
	if (win->gesture.first)
		wm_gesture_draw(win);

	/* needs pixel coords in screen */
	if (wm->drags.first) {
		wm_drags_draw(C, win, NULL);
	}
}

static void wm_method_draw_triple_multiview(bContext *C, wmWindow *win, eStereoViews sview)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmDrawData *drawdata;
	wmDrawTriple *triple_data, *triple_all;
	bScreen *screen = WM_window_get_active_screen(win);
	ScrArea *sa;
	ARegion *ar;
	int copytex = false;
	int id;

	/* we store the triple_data in sequence to triple_all */
	for (id = 0; id < 2; id++) {
		drawdata = BLI_findlink(&win->drawdata, (sview * 2) + id);

		if (drawdata && drawdata->triple) {
			if (id == 0) {
#if 0 /* why do we need to clear before overwriting? */
				glClearColor(0, 0, 0, 0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif

				wmSubWindowSet(win, screen->mainwin);

				wm_triple_draw_textures(win, drawdata->triple, 1.0f);
			}
		}
		else {
			/* we run it when we start OR when we turn stereo on */
			if (drawdata == NULL) {
				drawdata = MEM_callocN(sizeof(wmDrawData), "wmDrawData");
				BLI_addtail(&win->drawdata, drawdata);
			}

			drawdata->triple = MEM_callocN(sizeof(wmDrawTriple), "wmDrawTriple");

			if (!wm_triple_gen_textures(win, drawdata->triple)) {
				wm_draw_triple_fail(C, win);
				return;
			}
		}
	}

	triple_data = ((wmDrawData *) BLI_findlink(&win->drawdata, sview * 2))->triple;
	triple_all  = ((wmDrawData *) BLI_findlink(&win->drawdata, (sview * 2) + 1))->triple;

	/* draw marked area regions */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		switch (sa->spacetype) {
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				sima->iuser.multiview_eye = sview;
				break;
			}
			case SPACE_VIEW3D:
			{
				View3D *v3d = sa->spacedata.first;
				if (v3d->camera && v3d->camera->type == OB_CAMERA) {
					Camera *cam = v3d->camera->data;
					CameraBGImage *bgpic = cam->bg_images.first;
					v3d->multiview_eye = sview;
					if (bgpic) bgpic->iuser.multiview_eye = sview;
				}
				break;
			}
			case SPACE_NODE:
			{
				SpaceNode *snode = sa->spacedata.first;
				if ((snode->flag & SNODE_BACKDRAW) && ED_node_is_compositor(snode)) {
					Image *ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
					ima->eye = sview;
				}
				break;
			}
			case SPACE_SEQ:
			{
				SpaceSeq *sseq = sa->spacedata.first;
				sseq->multiview_eye = sview;
				break;
			}
		}

		/* draw marked area regions */
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->do_draw) {

				if (ar->overlap == false) {
					CTX_wm_region_set(C, ar);
					ED_region_do_draw(C, ar);

					if (sview == STEREO_RIGHT_ID)
						ar->do_draw = false;

					CTX_wm_region_set(C, NULL);
					copytex = true;
				}
			}
		}

		wm_area_mark_invalid_backbuf(sa);
		CTX_wm_area_set(C, NULL);
	}

	if (copytex) {
		wmSubWindowSet(win, screen->mainwin);

		wm_triple_copy_textures(win, triple_data);
	}

	if (wm->paintcursors.first) {
		for (sa = screen->areabase.first; sa; sa = sa->next) {
			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->swinid && ar->swinid == screen->subwinactive) {
					CTX_wm_area_set(C, sa);
					CTX_wm_region_set(C, ar);

					/* make region ready for draw, scissor, pixelspace */
					ED_region_set(C, ar);
					wm_paintcursor_draw(C, ar);

					CTX_wm_region_set(C, NULL);
					CTX_wm_area_set(C, NULL);
				}
			}
		}

		wmSubWindowSet(win, screen->mainwin);
	}

	/* draw overlapping area regions (always like popups) */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		CTX_wm_area_set(C, sa);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid && ar->overlap) {
				CTX_wm_region_set(C, ar);
				ED_region_do_draw(C, ar);
				if (sview == STEREO_RIGHT_ID)
					ar->do_draw = false;
				CTX_wm_region_set(C, NULL);

				wm_draw_region_blend(win, ar, triple_data);
			}
		}

		CTX_wm_area_set(C, NULL);
	}

	/* after area regions so we can do area 'overlay' drawing */
	ED_screen_draw(win);
	if (sview == STEREO_RIGHT_ID)
		screen->do_draw = false;

	/* draw floating regions (menus) */
	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->swinid) {
			CTX_wm_menu_set(C, ar);
			ED_region_do_draw(C, ar);
			if (sview == STEREO_RIGHT_ID)
				ar->do_draw = false;
			CTX_wm_menu_set(C, NULL);
		}
	}

	/* always draw, not only when screen tagged */
	if (win->gesture.first)
		wm_gesture_draw(win);

	/* needs pixel coords in screen */
	if (wm->drags.first) {
		wm_drags_draw(C, win, NULL);
	}

	/* copy the ui + overlays */
	wmSubWindowSet(win, screen->mainwin);
	wm_triple_copy_textures(win, triple_all);
}

/****************** main update call **********************/

/* quick test to prevent changing window drawable */
static bool wm_draw_update_test_window(wmWindow *win)
{
	const Scene *scene = WM_window_get_active_scene(win);
	const bScreen *screen = WM_window_get_active_screen(win);
	ScrArea *sa;
	ARegion *ar;
	bool do_draw = false;

	for (ar = screen->regionbase.first; ar; ar = ar->next) {
		if (ar->do_draw_overlay) {
			wm_tag_redraw_overlay(win, ar);
			ar->do_draw_overlay = false;
		}
		if (ar->swinid && ar->do_draw)
			do_draw = true;
	}

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			wm_region_test_render_do_draw(scene, sa, ar);

			if (ar->swinid && ar->do_draw)
				do_draw = true;
		}
	}

	if (do_draw)
		return true;
	
	if (screen->do_refresh)
		return true;
	if (screen->do_draw)
		return true;
	if (screen->do_draw_gesture)
		return true;
	if (screen->do_draw_paintcursor)
		return true;
	if (screen->do_draw_drag)
		return true;
	
	return false;
}

static int wm_automatic_draw_method(wmWindow *win)
{
	/* We assume all supported GPUs now support triple buffer well. */
	if (win->drawmethod == USER_DRAW_AUTOMATIC) {
		return USER_DRAW_TRIPLE;
	}
	else {
		return win->drawmethod;
	}
}

bool WM_is_draw_triple(wmWindow *win)
{
	/* function can get called before this variable is set in drawing code below */
	if (win->drawmethod != U.wmdrawmethod)
		win->drawmethod = U.wmdrawmethod;
	return (USER_DRAW_TRIPLE == wm_automatic_draw_method(win));
}

void wm_tag_redraw_overlay(wmWindow *win, ARegion *ar)
{
	/* for draw triple gestures, paint cursors don't need region redraw */
	if (ar && win) {
		bScreen *screen = WM_window_get_active_screen(win);

		if (wm_automatic_draw_method(win) != USER_DRAW_TRIPLE)
			ED_region_tag_redraw(ar);
		screen->do_draw_paintcursor = true;
	}
}

void WM_paint_cursor_tag_redraw(wmWindow *win, ARegion *ar)
{
	bScreen *screen = WM_window_get_active_screen(win);
	screen->do_draw_paintcursor = true;
	wm_tag_redraw_overlay(win, ar);
}

void wm_draw_update(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;

#ifdef WITH_OPENSUBDIV
	BKE_subsurf_free_unused_buffers();
#endif

	GPU_free_unused_buffers();
	
	for (win = wm->windows.first; win; win = win->next) {
#ifdef WIN32
		GHOST_TWindowState state = GHOST_GetWindowState(win->ghostwin);

		if (state == GHOST_kWindowStateMinimized) {
			/* do not update minimized windows, gives issues on Intel (see T33223)
			 * and AMD (see T50856). it seems logical to skip update for invisible
			 * window anyway.
			 */
			continue;
		}
#endif
		if (win->drawmethod != U.wmdrawmethod) {
			wm_draw_window_clear(win);
			win->drawmethod = U.wmdrawmethod;
		}

		if (wm_draw_update_test_window(win)) {
			bScreen *screen = WM_window_get_active_screen(win);

			CTX_wm_window_set(C, win);
			
			/* sets context window+screen */
			wm_window_make_drawable(wm, win);

			/* notifiers for screen redraw */
			if (screen->do_refresh)
				ED_screen_refresh(wm, win);

			int drawmethod = wm_automatic_draw_method(win);

			if (win->drawfail)
				wm_method_draw_overlap_all(C, win, 0);
			else if (drawmethod == USER_DRAW_FULL)
				wm_method_draw_full(C, win);
			else if (drawmethod == USER_DRAW_OVERLAP)
				wm_method_draw_overlap_all(C, win, 0);
			else if (drawmethod == USER_DRAW_OVERLAP_FLIP)
				wm_method_draw_overlap_all(C, win, 1);
			else { /* USER_DRAW_TRIPLE */
				if ((WM_stereo3d_enabled(win, false)) == false) {
					wm_method_draw_triple(C, win);
				}
				else {
					wm_method_draw_triple_multiview(C, win, STEREO_LEFT_ID);
					wm_method_draw_triple_multiview(C, win, STEREO_RIGHT_ID);
					wm_method_draw_stereo3d(C, win);
				}
			}

			screen->do_draw_gesture = false;
			screen->do_draw_paintcursor = false;
			screen->do_draw_drag = false;
		
			wm_window_swap_buffers(win);

			CTX_wm_window_set(C, NULL);
		}
	}
}

void wm_draw_data_free(wmWindow *win)
{
	wmDrawData *dd;

	for (dd = win->drawdata.first; dd; dd = dd->next) {
		wm_draw_triple_free(dd->triple);
	}
	BLI_freelistN(&win->drawdata);
}

void wm_draw_window_clear(wmWindow *win)
{
	bScreen *screen = WM_window_get_active_screen(win);
	ScrArea *sa;
	ARegion *ar;

	wm_draw_data_free(win);

	/* clear screen swap flags */
	if (screen) {
		for (sa = screen->areabase.first; sa; sa = sa->next)
			for (ar = sa->regionbase.first; ar; ar = ar->next)
				ar->swap = WIN_NONE_OK;
		
		screen->swap = WIN_NONE_OK;
	}
}

void wm_draw_region_clear(wmWindow *win, ARegion *ar)
{
	bScreen *screen = WM_window_get_active_screen(win);
	int drawmethod = wm_automatic_draw_method(win);

	if (ELEM(drawmethod, USER_DRAW_OVERLAP, USER_DRAW_OVERLAP_FLIP))
		wm_flush_regions_down(screen, &ar->winrct);

	screen->do_draw = true;
}

void WM_redraw_windows(bContext *C)
{
	wmWindow *win_prev = CTX_wm_window(C);
	ScrArea *area_prev = CTX_wm_area(C);
	ARegion *ar_prev = CTX_wm_region(C);

	wm_draw_update(C);

	CTX_wm_window_set(C, win_prev);
	CTX_wm_area_set(C, area_prev);
	CTX_wm_region_set(C, ar_prev);
}

