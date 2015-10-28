/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2009 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

/**
    \ingroup MCFW_API
    \defgroup MCFW_VDIS_API McFW Video Display (VDIS) API

    @{
*/

/**
    \file ti_vdis_timings".h
    \brief Venc timing params
*/

#ifndef __TI_VDIS_TIMINGS_H__
#define __TI_VDIS_TIMINGS_H__

#define VDIS_TIMINGS_1080P_60 "echo 148500,1920/88/148/44,1080/4/36/5,1  > /sys/devices/platform/vpss/display%d/timings"
#define VDIS_TIMINGS_720P_60  "echo 74250,1280/110/220/40,720/5/20/5,1   > /sys/devices/platform/vpss/display%d/timings"
#define VDIS_TIMINGS_XGA_60   "echo 65000,1024/24/160/136,768/3/29/6,1   > /sys/devices/platform/vpss/display%d/timings"
#define VDIS_TIMINGS_SXGA_60  "echo 108000,1280/48/248/112,1024/1/38/3,1 > /sys/devices/platform/vpss/display%d/timings"

#define VDIS_MODE_NTSC        "echo ntsc                                 > /sys/devices/platform/vpss/display%d/mode"
#define VDIS_MODE_PAL         "echo pal                                  > /sys/devices/platform/vpss/display%d/mode"

#define VDIS_TIMINGS_SETVENC  "echo %d > /sys/devices/platform/vpss/display%d/enabled"

#define VDIS_CLKSRC_SETVENC  "echo %s > /sys/devices/platform/vpss/display%d/clksrc"

#define VDIS_TIMINGS_TIEDVENCS "echo %d > /sys/devices/platform/vpss/system/tiedvencs"

#define VDIS_NODE_GRPX          "echo %s > /sys/devices/platform/vpss/graphics%d/enabled"

#define VDIS_SET_GRPX          "echo %d > /sys/devices/platform/vpss/graphics%d/enabled"

#define VDIS_CMD_ARG1(buff, timing, x)      sprintf(buff, timing, x); system(buff);

#define VDIS_CMD_ARG2(buff, timing, x, y)   sprintf(buff, timing, x, y); system(buff);

#endif  /* __TI_VDIS_TIMINGS_H__ */

/* @} */
