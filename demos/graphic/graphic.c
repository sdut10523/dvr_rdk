/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2011 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

/*----------------------------------------------------------------------------
 Defines referenced header files
-----------------------------------------------------------------------------*/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <linux/fb.h>
#include <linux/ti81xxfb.h>

#define GRPX_SC_MARGIN_OFFSET   (3)

#define RGB_KEY_24BIT_GRAY   0x00808080
#define RGB_KEY_16BIT_GRAY   0x8410


#include "img_ti_logo.h"
#include "ti_media_std.h"
#include "graphic.h"

#include "demos/mcfw_api_demos/mcfw_demo/demo.h"
#include "demos/mcfw_api_demos/mcfw_demo/demo_swms.h"



/*----------------------------------------------------------------------------
 Definitions and macro
-----------------------------------------------------------------------------*/
#define FBDEV_NAME_0            "/dev/fb0"
#define FBDEV_NAME_2            "/dev/fb2"

#ifdef TI_814X_BUILD
#define NUM_GRPX_DISPLAYS   1
#else
#define NUM_GRPX_DISPLAYS   2
#endif

#ifdef TI_814X_BUILD
#define GRPX_PLANE_WIDTH    (1280)
#define GRPX_PLANE_HEIGHT   ( 720)
#else
#define GRPX_PLANE_WIDTH    (1280)
#define GRPX_PLANE_HEIGHT   ( 720)
#endif
#define GRPX_STARTX_0        (50u)
#define GRPX_STARTY_0        (20u)
#define GRPX_STARTX_1        (GRPX_PLANE_WIDTH-210)
#define GRPX_STARTY_1        (GRPX_STARTY_0)
#define GRPX_STARTX_2        (GRPX_STARTX_0)
#define GRPX_STARTY_2        (GRPX_PLANE_HEIGHT-150)
#define GRPX_STARTX_3        (GRPX_STARTX_1)
#define GRPX_STARTY_3        (GRPX_STARTY_2)




typedef struct {
    int fd;
    int fd2;
    unsigned char *buf[NUM_GRPX_DISPLAYS];
    unsigned int curWidth;
    unsigned int curHeight;
    unsigned int iconWidth;
    unsigned int iconHeight;
    unsigned int bytes_per_pixel;
    int buf_size;
    grpx_plane_type planeType;

} app_grpx_t;

/*----------------------------------------------------------------------------
 Declares variables
-----------------------------------------------------------------------------*/
static app_grpx_t grpx_obj;

/*----------------------------------------------------------------------------
 Declares a function prototype
-----------------------------------------------------------------------------*/
#define eprintf(str,args...) printf(" [FBDEV] ERROR: " str, ## args);
#define dprintf(str,args...) printf(" [FBDEV] " str, ## args);


/*----------------------------------------------------------------------------
 local function
-----------------------------------------------------------------------------*/

static int disp_getregparams(int display_fd)
{
    int ret;

    struct ti81xxfb_region_params regp;

    memset(&regp, 0, sizeof(regp));

    ret = ioctl(display_fd, TIFB_GET_PARAMS, &regp);
    if (ret < 0) {
        eprintf("TIFB_GET_PARAMS\n");
        return ret;
    }

    dprintf("\n");
    dprintf("Reg Params Info\n");
    dprintf("---------------\n");
    dprintf("region %d, postion %d x %d, prioirty %d\n",
        regp.ridx,
        regp.pos_x,
        regp.pos_y,
        regp.priority);
    dprintf("first %d, last %d\n",
        regp.firstregion,
        regp.lastregion);
    dprintf("sc en %d, sten en %d\n",
        regp.scalaren,
        regp.stencilingen);
    dprintf("tran en %d, type %d, key %d\n",
        regp.transen,
        regp.transtype,
        regp.transcolor);
    dprintf("blend %d, alpha %d\n"
        ,regp.blendtype,
        regp.blendalpha);
    dprintf("bb en %d, alpha %d\n",
        regp.bben,
        regp.bbalpha);
    dprintf("\n");
    return 0;
}

static int disp_fbinfo(int fd)
{
    struct fb_fix_screeninfo fixinfo;
    struct fb_var_screeninfo varinfo, org_varinfo;
    int size;
    int ret;

    /* Get fix screen information. Fix screen information gives
     * fix information like panning step for horizontal and vertical
     * direction, line length, memory mapped start address and length etc.
     */
    ret = ioctl(fd, FBIOGET_FSCREENINFO, &fixinfo);
    if (ret < 0) {
        eprintf("FBIOGET_FSCREENINFO !!!\n");
        return -1;
    }

    {
        dprintf("\n");
        dprintf("Fix Screen Info\n");
        dprintf("---------------\n");
        dprintf("Line Length - %d\n", fixinfo.line_length);
        dprintf("Physical Address = %lx\n",fixinfo.smem_start);
        dprintf("Buffer Length = %d\n",fixinfo.smem_len);
        dprintf("\n");
    }

    /* Get variable screen information. Variable screen information
     * gives informtion like size of the image, bites per pixel,
     * virtual size of the image etc. */
    ret = ioctl(fd, FBIOGET_VSCREENINFO, &varinfo);
    if (ret < 0) {
        eprintf("FBIOGET_VSCREENINFO !!!\n");
        return -1;
    }

    {
        dprintf("\n");
        dprintf("Var Screen Info\n");
        dprintf("---------------\n");
        dprintf("Xres - %d\n", varinfo.xres);
        dprintf("Yres - %d\n", varinfo.yres);
        dprintf("Xres Virtual - %d\n", varinfo.xres_virtual);
        dprintf("Yres Virtual - %d\n", varinfo.yres_virtual);
        dprintf("Bits Per Pixel - %d\n", varinfo.bits_per_pixel);
        dprintf("Pixel Clk - %d\n", varinfo.pixclock);
        dprintf("Rotation - %d\n", varinfo.rotate);
        dprintf("\n");
    }

    disp_getregparams(fd);

    memcpy(&org_varinfo, &varinfo, sizeof(varinfo));

    /*
     * Set the resolution which read before again to prove the
     * FBIOPUT_VSCREENINFO ioctl.
     */

    ret = ioctl(fd, FBIOPUT_VSCREENINFO, &org_varinfo);
    if (ret < 0) {
        eprintf("FBIOPUT_VSCREENINFO !!!\n");
        return -1;
    }

    /* It is better to get fix screen information again. its because
     * changing variable screen info may also change fix screen info. */
    ret = ioctl(fd, FBIOGET_FSCREENINFO, &fixinfo);
    if (ret < 0) {
        eprintf("FBIOGET_FSCREENINFO !!!\n");
        return -1;
    }

    size = varinfo.xres*varinfo.yres*(varinfo.bits_per_pixel/8);
    dprintf("\n");
    dprintf("### BUF SIZE = %d Bytes !!! \n", size);
    dprintf("\n");

    return size;
}

static int draw_fill_color(unsigned char *buf_addr)
{
    unsigned int i, j;
    unsigned char *p;
    app_grpx_t *grpx = &grpx_obj;

    if(buf_addr==NULL)
        return -1;

    p = (unsigned char *)buf_addr;
    for(i=0; i<grpx->curHeight; i++) {
        for(j=0; j<grpx->curWidth; j++) {
            switch(grpx->planeType)
            {
                case GRPX_FORMAT_RGB565:
                    *p++  = 0x10;
                    *p++  = 0x84;
                break;
                case GRPX_FORMAT_RGB888:
                    *p++  = 0x80;
                    *p++  = 0x80;
                    *p++  = 0x80;
                    *p++  = 0x00;
                break;
                case GRPX_FORMAT_MAX:
                default:
                break;
            }
        }
    }

    return 0;
}

static int draw_img(VDIS_DEV devId, unsigned char *buf_addr, unsigned char *img_addr, int sx, int sy, int wi, int ht)
{
    unsigned int i, j;
    unsigned char *p;
    app_grpx_t *grpx = &grpx_obj;
    unsigned char *key;


    if(buf_addr==NULL || img_addr==NULL)
        return -1;

    p = (unsigned char *)(buf_addr + ((sx * grpx->bytes_per_pixel)+( sy * grpx->bytes_per_pixel * grpx->curWidth)));

    for(j=0; j<ht; j++)
    {
        for(i=0; i<wi; i++)
        {
            if(grpx->planeType == GRPX_FORMAT_RGB565)
            {
                unsigned short key_rgb565 = RGB_KEY_16BIT_GRAY;
                key = (unsigned char *)&key_rgb565;

                if(img_addr[i] != key[i] || img_addr[i + 1] != key[i + 1] ) {
                    *p = *img_addr;
                    *(p + 1) = *(img_addr + 1);
                }
            }
            if(grpx->planeType == GRPX_FORMAT_RGB888)
            {
                unsigned int key_rgb888 = RGB_KEY_24BIT_GRAY;
                key = (unsigned char *)&key_rgb888;

                if((img_addr[i] != key[i]) || (img_addr[i + 1] != key[i + 1])
                   || (img_addr[i + 2] != key[i + 2]) || (img_addr[i + 3] != key[i + 3]) ) {

                    *p = *img_addr;
                    *(p + 1) = *(img_addr + 1);
                    *(p + 2) = *(img_addr + 2);
                    *(p + 3) = *(img_addr + 3);
                }
            }
            p        += grpx->bytes_per_pixel;
            img_addr += grpx->bytes_per_pixel;
        }
        p += ((grpx->curWidth-wi) * grpx->bytes_per_pixel);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 grpx_fb draw function
-----------------------------------------------------------------------------*/
int grpx_fb_draw(VDIS_DEV devId)
{
    int ret=0;
    unsigned char * img_ti_logo = NULL;
    app_grpx_t *grpx = &grpx_obj;

    dprintf("grpx_fb_draw ... \n");

    if(grpx->planeType >= GRPX_FORMAT_MAX)
    {
        return -1;
    }
    if(grpx->planeType == GRPX_FORMAT_RGB565)
    {
        img_ti_logo = (unsigned char *)img_ti_logo_RGB565;
        grpx->iconWidth  = 160;
        grpx->iconHeight = 64;

    }
    if(grpx->planeType == GRPX_FORMAT_RGB888)
    {
        img_ti_logo = (unsigned char *)img_ti_logo_RGB888;
        grpx->iconWidth  = 240;
        grpx->iconHeight = 60;
    }

    if (devId == VDIS_DEV_HDMI) {
        draw_img(   devId,
                    grpx->buf[0],
                    img_ti_logo,
                    GRPX_STARTX_0,
                    GRPX_STARTY_0,
                    grpx->iconWidth,
                    grpx->iconHeight
                );
        draw_img(   devId,
                    grpx->buf[0],
                    img_ti_logo,
                    GRPX_STARTX_1,
                    GRPX_STARTY_1,
                    grpx->iconWidth,
                    grpx->iconHeight
                );
        draw_img(   devId,
                    grpx->buf[0],
                    img_ti_logo,
                    GRPX_STARTX_2,
                    GRPX_STARTY_2,
                    grpx->iconWidth,
                    grpx->iconHeight
                );
        draw_img(   devId,
                    grpx->buf[0],
                    img_ti_logo,
                    GRPX_STARTX_3,
                    GRPX_STARTY_3,
                    grpx->iconWidth,
                    grpx->iconHeight
                );

    }
    if (devId == VDIS_DEV_SD) {
        draw_img(   devId,
                    grpx->buf[1],
                    img_ti_logo,
                    GRPX_STARTX_0,
                    GRPX_STARTY_0,
                    grpx->iconWidth,
                    grpx->iconHeight
                );
    }
    dprintf("grpx_fb_draw ... Done !!! \n");

    return ret;
}


Int32 grpx_fb_scale(VDIS_DEV devId, UInt32 startX, UInt32 startY, UInt32 outWidth, UInt32 outHeight)
{

    struct ti81xxfb_scparams scparams;
    Int32                    fd = 0, status = 0;
    app_grpx_t *grpx = &grpx_obj;
    struct ti81xxfb_region_params  regp;
    /* int dummy; */

    if (devId == VDIS_DEV_HDMI){
        fd = grpx->fd;
    }
    if (devId == VDIS_DEV_SD){
        fd = grpx->fd2;
    }

    /* TBD: FBIO_WAITFORVSYNC produces an error at run time
     */
    /*
    if (ioctl(fd, FBIO_WAITFORVSYNC, &dummy)) {
        eprintf("FBIO_WAITFORVSYNC !!!\n");
        return -1;
    }
    */

    /* Set Scalar Params for resolution conversion */
    scparams.inwidth = grpx->curWidth;
    scparams.inheight = grpx->curHeight;

    // this "-GRPX_SC_MARGIN_OFFSET" is needed since scaling can result in +2 extra pixels, so we compensate by doing -2 here
    scparams.outwidth = outWidth - GRPX_SC_MARGIN_OFFSET;
    scparams.outheight = outHeight - GRPX_SC_MARGIN_OFFSET;
    scparams.coeff = NULL;

    if ((status = ioctl(fd, TIFB_SET_SCINFO, &scparams)) < 0) {
        eprintf("TIFB_SET_SCINFO !!!\n");
    }



    if (ioctl(fd, TIFB_GET_PARAMS, &regp) < 0) {
        eprintf("TIFB_GET_PARAMS !!!\n");
    }

    regp.pos_x = startX;
    regp.pos_y = startY;
    regp.transen = TI81XXFB_FEATURE_ENABLE;
    regp.transcolor = RGB_KEY_24BIT_GRAY;
    regp.scalaren = TI81XXFB_FEATURE_ENABLE;

    if (ioctl(fd, TIFB_SET_PARAMS, &regp) < 0) {
        eprintf("TIFB_SET_PARAMS !!!\n");
    }

    return (status);

}

int grpx_fb_start2()
{
    int fd2 = 0;
    struct fb_var_screeninfo varinfo;

    app_grpx_t *grpx = &grpx_obj;
    int hdbuf_size;
    UInt32 outWidth, outHeight;

    dprintf("\n");
    dprintf("FB2: Starting !!!\n")

    fd2 = open(FBDEV_NAME_2, O_RDWR);
    if (fd2 <= 0) {
        eprintf("FB2: Could not open device [%s] !!! \n",FBDEV_NAME_2);
        return -1;
    }

    dprintf("FB2: Opened device [%s] (fd=%d) !!!\n",FBDEV_NAME_2,fd2);

    grpx->fd2 = fd2;

    if (ioctl(fd2, FBIOGET_VSCREENINFO, &varinfo) < 0) {
        eprintf("FB2: FBIOGET_VSCREENINFO !!!\n");
    }

    if (grpx->planeType == GRPX_FORMAT_RGB565)
    {

        varinfo.bits_per_pixel   =  16;
        varinfo.red.length       =  5;
        varinfo.green.length     =  6;
        varinfo.blue.length      =  5;

        varinfo.red.offset       =  11;
        varinfo.green.offset     =  5;
        varinfo.blue.offset      =  0;

        if(ioctl(fd2, FBIOPUT_VSCREENINFO, &varinfo) < 0) {
             eprintf("FB2: FBIOPUT_VSCREENINFO !!!\n");
        }
    }

    hdbuf_size = disp_fbinfo(fd2);
    if(hdbuf_size < 0)
    {
        eprintf("FB2: disp_fbinfo() !!!\n");
    }

    Demo_swMsGetOutSize(Vdis_getResolution(VDIS_DEV_SD), &outWidth,  &outHeight);

    grpx_fb_scale(VDIS_DEV_SD, 0, 0, outWidth, outHeight);

    if (ioctl(fd2, FBIOGET_VSCREENINFO, &varinfo) < 0) {
        eprintf("FB2: FBIOGET_VSCREENINFO !!!\n");
    }

    varinfo.xres           = grpx->curWidth;
    varinfo.yres           = grpx->curHeight;
    varinfo.xres_virtual   = grpx->curWidth;
    varinfo.yres_virtual   = grpx->curHeight;

    if(ioctl(fd2, FBIOPUT_VSCREENINFO, &varinfo) < 0) {
         eprintf("FB2: FBIOPUT_VSCREENINFO !!!\n");
    }

    disp_fbinfo(fd2);

    dprintf("FB2: Start DONE !!!\n")
    dprintf("\n");

    return 0;
}

int grpx_fb_stop2()
{
    int ret=0;

#ifdef TI_814X_BUILD
    app_grpx_t *grpx = &grpx_obj;

    dprintf("\n");
    dprintf("FB2: Stopping !!!\n")

    if(grpx->fd2 > 0) {
        ret = close(grpx->fd2);
    }

    dprintf("FB2: Stop DONE !!!\n")
    dprintf("\n");
#endif

    return ret;
}

/*----------------------------------------------------------------------------
 grpx_fb init/exit function
-----------------------------------------------------------------------------*/
int grpx_fb_start(grpx_plane_type planeType)
{
    int fd=0;
    struct fb_var_screeninfo varinfo;

    UInt32 outWidth, outHeight;

    app_grpx_t *grpx = &grpx_obj;
    int hdbuf_size, i;

    dprintf("\n");
    dprintf("FB0: Starting !!!\n")

    memset(grpx, 0, sizeof(app_grpx_t));

    if(planeType >= GRPX_FORMAT_MAX)
    {
        return -1;
    }
    else
    {
        grpx->planeType = planeType;
    }

    //# Open the display device
    fd = open(FBDEV_NAME_0, O_RDWR);
    if (fd <= 0) {
        eprintf("FB0: Could not open device [%s] !!! \n",FBDEV_NAME_0);
        return -1;
    }

    grpx->curWidth = GRPX_PLANE_WIDTH;
    grpx->curHeight = GRPX_PLANE_HEIGHT;

    dprintf("FB2: Opened device [%s] (fd=%d) !!!\n",FBDEV_NAME_0,fd);

    if (ioctl(fd, FBIOGET_VSCREENINFO, &varinfo) < 0) {
        eprintf("FB0: FBIOGET_VSCREENINFO !!!\n");
    }
    varinfo.xres           =  grpx->curWidth;
    varinfo.yres           =  grpx->curHeight;
    varinfo.xres_virtual   =  grpx->curWidth;
    varinfo.yres_virtual   =  grpx->curHeight;

    if (grpx->planeType == GRPX_FORMAT_RGB565)
    {
        varinfo.bits_per_pixel   =  16;
        varinfo.red.length       =  5;
        varinfo.green.length     =  6;
        varinfo.blue.length      =  5;

        varinfo.red.offset       =  11;
        varinfo.green.offset     =  5;
        varinfo.blue.offset      =  0;
    }

    if (ioctl(fd, FBIOPUT_VSCREENINFO, &varinfo) < 0) {
        eprintf("FB0: FBIOPUT_VSCREENINFO !!!\n");
    }

    grpx->bytes_per_pixel = (varinfo.bits_per_pixel / 8);

    hdbuf_size = disp_fbinfo(fd);
    if(hdbuf_size < 0)
    {
        eprintf("FB0: disp_fbinfo() !!!\n");
        return -1;
    }

    grpx->buf[0] = (unsigned char *)mmap (0, hdbuf_size*NUM_GRPX_DISPLAYS,
            (PROT_READ|PROT_WRITE), MAP_SHARED, fd, 0);
    if (grpx->buf[0] == MAP_FAILED) {
        eprintf("FB0: mmap() failed !!!\n");
        return -1;
    }
    grpx->buf_size = hdbuf_size;
    for(i=1; i<NUM_GRPX_DISPLAYS; i++) {
        grpx->buf[i] = grpx->buf[i-1] + (grpx->curWidth*grpx->curHeight*grpx->bytes_per_pixel);
    }

    grpx->fd = fd;

    Demo_swMsGetOutSize(Vdis_getResolution(VDIS_DEV_HDMI), &outWidth,  &outHeight);

    grpx_fb_scale(VDIS_DEV_HDMI, 0, 0, outWidth, outHeight);

    disp_fbinfo(fd);

    dprintf("FB0: Start DONE !!!\n");
    dprintf("\n");

    return 0;
}

int grpx_fb_init(grpx_plane_type planeType)
{
    app_grpx_t *grpx = &grpx_obj;

    // need to start and stop FBDev once for the RGB565 and SC to take effect
    grpx_fb_start(planeType);
#ifdef TI_814X_BUILD
    grpx_fb_start2();
#endif
    //# init fb - fill RGB_KEY
    draw_fill_color(grpx->buf[0]);
#ifdef TI_816X_BUILD
    draw_fill_color(grpx->buf[1]);
#endif

    return 0;
}

void grpx_fb_exit(void)
{
    int ret=0;
    app_grpx_t *grpx = &grpx_obj;

    dprintf("\n");
    dprintf("grpx_fb_exit ... \n");

    if(grpx->buf[0]) {
        ret = munmap(grpx->buf[0], (int)grpx->buf_size*NUM_GRPX_DISPLAYS);
    }
    if(ret)
        dprintf("FB0: munmap failed (%d) !!!\n", ret);

    if(grpx->fd > 0) {
#ifndef TI_814X_BUILD
//        ioctl(grpx->fd, TIFB_CLOSE, NULL);
#endif
        ret = close(grpx->fd);
    }
#ifdef TI_814X_BUILD
    grpx_fb_stop2();
#endif
    dprintf("grpx_fb_exit ... Done (%d) !!!\n", ret);
    dprintf("\n");

    return;
}


Int32 grpx_fb_demo()
{
    UInt32 devId;
    UInt32 outWidth, outHeight;
    UInt32 startX, startY;
    UInt32 offsetX, offsetY;
    UInt32 loopCount, i;
    UInt32 runCount;

    devId = VDIS_DEV_SD;

    runCount = 10000;

    loopCount = 100;
    offsetX = offsetY = 1;

    /* putting in a loop for test */
    while(runCount--)
    {
        /* putting in another loop to change size and position every few msecs */
        for(i=1; i<=loopCount; i++)
        {
            Demo_swMsGetOutSize(Vdis_getResolution(devId), &outWidth, &outHeight);

            startX = offsetX*i;
            startY = offsetY*i;

            outWidth  -= startX*2;
            outHeight -= startY*2;

            grpx_fb_scale(devId, startX, startY, outWidth, outHeight);
        }
        for(i=loopCount; i>=1; i--)
        {
            Demo_swMsGetOutSize(Vdis_getResolution(devId), &outWidth, &outHeight);

            startX = offsetX*i;
            startY = offsetY*i;

            outWidth  -= startX*2;
            outHeight -= startY*2;

            grpx_fb_scale(devId, startX, startY, outWidth, outHeight);
        }

        /* restore to original */
        Demo_swMsGetOutSize(Vdis_getResolution(devId), &outWidth, &outHeight);

        dprintf("[reset] %d x %d\n", outWidth, outHeight);
        grpx_fb_scale(devId, 0, 0, outWidth, outHeight);
    }

    return 0;
}
