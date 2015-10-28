/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2011 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

#ifndef _GRAPHIC_H_
#define _GRAPHIC_H_

#include "mcfw/interfaces/ti_vdis.h"

typedef enum {
    GRPX_FORMAT_RGB565 = 0,
    GRPX_FORMAT_RGB888 = 1,
    GRPX_FORMAT_MAX

}grpx_plane_type;


int grpx_fb_init(grpx_plane_type planeType);
void grpx_fb_exit(void);
int grpx_fb_draw(VDIS_DEV devId);
Int32 grpx_fb_scale(VDIS_DEV devId, UInt32 startX, UInt32 startY, UInt32 outWidth, UInt32 outHeight);
Int32 grpx_fb_demo();

int grpx_link_init(grpx_plane_type planeType);
void grpx_link_exit(void);
int grpx_link_draw(VDIS_DEV devId);
Int32 grpx_link_scale(VDIS_DEV devId, UInt32 startX, UInt32 startY, UInt32 outWidth, UInt32 outHeight);
Int32 grpx_link_demo();

#if 0

#define grpx_init   grpx_fb_init
#define grpx_exit   grpx_fb_exit
#define grpx_draw   grpx_fb_draw
#define grpx_scale  grpx_fb_scale
#define grpx_demo   grpx_fb_demo

#else

#define grpx_init   grpx_link_init
#define grpx_exit   grpx_link_exit
#define grpx_draw   grpx_link_draw
#define grpx_scale  grpx_link_scale
#define grpx_demo   grpx_link_demo


#endif

#endif /*   _GRAPHIC_H_ */

