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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/BlenderRoutines/KX_BlenderGL.cpp
 *  \ingroup blroutines
 */


#include "KX_BlenderGL.h"

/* 
 * This little block needed for linking to Blender... 
 */
#ifdef WIN32
#include <vector>
#include "BLI_winstuff.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "BL_Material.h" // MAXTEX

/* Data types encoding the game world: */
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_camera_types.h"
#include "DNA_world_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_image_types.h"
#include "DNA_view3d_types.h"
#include "DNA_material_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_bmfont.h"
#include "BKE_image.h"

#include "BLI_path_util.h"
#include "BLI_string.h"

extern "C" {
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"
#include "wm_cursors.h"
#include "wm_window.h"
#include "BLF_api.h"
}

/* end of blender block */
void BL_warp_pointer(wmWindow *win, int x,int y)
{
	WM_cursor_warp(win, x, y);
}

void BL_SwapBuffers(wmWindow *win)
{
	wm_window_swap_buffers(win);
}

void BL_MakeDrawable(wmWindowManager *wm, wmWindow *win)
{
	wm_window_make_drawable(wm, win);
}

void BL_SetSwapInterval(struct wmWindow *win, int interval)
{
	wm_window_set_swap_interval(win, interval);
}

int BL_GetSwapInterval(struct wmWindow *win)
{
	return wm_window_get_swap_interval(win);
}

static void DisableForText()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); /* needed for texture fonts otherwise they render as wireframe */

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);

	if (GLEW_ARB_multitexture) {
		for (int i=0; i<MAXTEX; i++) {
			glActiveTextureARB(GL_TEXTURE0_ARB+i);

			if (GLEW_ARB_texture_cube_map)
				glDisable(GL_TEXTURE_CUBE_MAP_ARB);

			glDisable(GL_TEXTURE_2D);
		}

		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else {
		if (GLEW_ARB_texture_cube_map)
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);

		glDisable(GL_TEXTURE_2D);
	}
}

void BL_draw_gamedebug_box(int xco, int yco, int width, int height, float percentage)
{
	/* This is a rather important line :( The gl-mode hasn't been left
	 * behind quite as neatly as we'd have wanted to. I don't know
	 * what cause it, though :/ .*/
	glDisable(GL_DEPTH_TEST);
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	
	glOrtho(0, width, 0, height, -100, 100);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	
	yco = height - yco;
	int barsize = 50;

	/* draw in black first*/
	glColor3ub(0, 0, 0);
	glBegin(GL_QUADS);
	glVertex2f(xco + 1 + 1 + barsize * percentage, yco - 1 + 10);
	glVertex2f(xco + 1, yco - 1 + 10);
	glVertex2f(xco + 1, yco - 1);
	glVertex2f(xco + 1 + 1 + barsize * percentage, yco - 1);
	glEnd();
	
	glColor3ub(255, 255, 255);
	glBegin(GL_QUADS);
	glVertex2f(xco + 1 + barsize * percentage, yco + 10);
	glVertex2f(xco, yco + 10);
	glVertex2f(xco, yco);
	glVertex2f(xco + 1 + barsize * percentage, yco);
	glEnd();
	
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

/* Print 3D text */
void BL_print_game_line(int fontid, const char *text, int size, int dpi, float *color, double *mat, float aspect)
{
	/* gl prepping */
	DisableForText();

	/* the actual drawing */
	glColor4fv(color);

	/* multiply the text matrix by the object matrix */
	BLF_enable(fontid, BLF_MATRIX|BLF_ASPECT);
	BLF_matrix(fontid, mat);

	/* aspect is the inverse scale that allows you to increase */
	/* your resolution without sizing the final text size      */
	/* the bigger the size, the smaller the aspect	           */
	BLF_aspect(fontid, aspect, aspect, aspect);

	BLF_size(fontid, size, dpi);
	BLF_position(fontid, 0, 0, 0);
	BLF_draw(fontid, (char *)text, 65535);

	BLF_disable(fontid, BLF_MATRIX|BLF_ASPECT);
}

void BL_print_gamedebug_line(const char *text, int xco, int yco, int width, int height)
{
	/* gl prepping */
	DisableForText();
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glOrtho(0, width, 0, height, -100, 100);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* the actual drawing */
	glColor3ub(255, 255, 255);
	BLF_size(blf_mono_font, 11, 72);
	BLF_position(blf_mono_font, (float)xco, (float)(height-yco), 0.0f);
	BLF_draw(blf_mono_font, (char *)text, 65535); /* XXX, use real len */

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void BL_print_gamedebug_line_padded(const char *text, int xco, int yco, int width, int height)
{
	/* This is a rather important line :( The gl-mode hasn't been left
	 * behind quite as neatly as we'd have wanted to. I don't know
	 * what cause it, though :/ .*/
	DisableForText();
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	
	glOrtho(0, width, 0, height, -100, 100);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* draw in black first*/
	glColor3ub(0, 0, 0);
	BLF_size(blf_mono_font, 11, 72);
	BLF_position(blf_mono_font, (float)xco+1, (float)(height-yco-1), 0.0f);
	BLF_draw(blf_mono_font, (char *)text, 65535);/* XXX, use real len */
	
	glColor3ub(255, 255, 255);
	BLF_position(blf_mono_font, (float)xco, (float)(height-yco), 0.0f);
	BLF_draw(blf_mono_font, (char *)text, 65535);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void BL_HideMouse(wmWindow *win)
{
	WM_cursor_set(win, CURSOR_NONE);
}


void BL_WaitMouse(wmWindow *win)
{
	WM_cursor_set(win, CURSOR_WAIT);
}


void BL_NormalMouse(wmWindow *win)
{
	WM_cursor_set(win, CURSOR_STD);
}
/* get shot from frontbuffer sort of a copy from screendump.c */
static unsigned int *screenshot(ScrArea *curarea, int *dumpsx, int *dumpsy)
{
	int x=0, y=0;
	unsigned int *dumprect= NULL;
	
	x= curarea->totrct.xmin;
	y= curarea->totrct.ymin;
	*dumpsx= curarea->totrct.xmax-x;
	*dumpsy= curarea->totrct.ymax-y;

	if (*dumpsx && *dumpsy) {
		
		dumprect= (unsigned int *)MEM_mallocN(sizeof(int) * (*dumpsx) * (*dumpsy), "dumprect");
		glReadBuffer(GL_FRONT);
		glReadPixels(x, y, *dumpsx, *dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, dumprect);
		glFinish();
		glReadBuffer(GL_BACK);
	}

	return dumprect;
}

/* based on screendump.c::screenshot_exec */
void BL_MakeScreenShot(bScreen *screen, ScrArea *curarea, const char *filename)
{
	unsigned int *dumprect;
	int dumpsx, dumpsy;
	
	dumprect = screenshot(curarea, &dumpsx, &dumpsy);

	if (dumprect) {
		/* initialize image file format data */
		Scene *scene = (screen)? screen->scene: NULL;
		ImageFormatData im_format;

		if (scene)
			im_format = scene->r.im_format;
		else
			BKE_imformat_defaults(&im_format);

		/* create file path */
		char path[FILE_MAX];
		BLI_strncpy(path, filename, sizeof(path));
		BLI_path_abs(path, G.main->name);
		BKE_add_image_extension_from_type(path, im_format.imtype);

		/* create and save imbuf */
		ImBuf *ibuf = IMB_allocImBuf(dumpsx, dumpsy, 24, 0);
		ibuf->rect = dumprect;

		BKE_imbuf_write_as(ibuf, path, &im_format, false);

		ibuf->rect = NULL;
		IMB_freeImBuf(ibuf);
		MEM_freeN(dumprect);
	}
}

