
#include <demo.h>

char gDemo_displaySettingsMenu[] = {
    "\r\n ====================="
    "\r\n Display Settings Menu"
    "\r\n ====================="
    "\r\n"
    "\r\n 1: Disable channel"
    "\r\n 2: Enable  channel"
    "\r\n 3: Switch Layout"
    "\r\n 4: Switch Channels"
    "\r\n 5: Change resolution"
    "\r\n 6: Switch Queue(ONLY FOR SD Display)"

    "\r\n 7: Switch Channel(ONLY FOR Enc HD Usecase)"
    "\r\n 8: Switch SDTV channel (ONLY for progressive demo)"
    "\r\n 9: 2x digital zoom in top left"
    "\r\n a: 2x digital zoom in center"
    "\r\n"
    "\r\n p: Previous Menu"
    "\r\n"
    "\r\n Enter Choice: "
};

char gDemo_displayLayoutMenu[] = {
    "\r\n ====================="
    "\r\n Select Display Layout"
    "\r\n ====================="
    "\r\n"
    "\r\n 1: 1x1 CH"
    "\r\n 2: 2x2 CH"
    "\r\n 3: 3x3 CH"
    "\r\n 4: 4x4 CH"
    "\r\n 5: 2x2 CH + 4CH"
    "\r\n 6: 1CH + 5CH"
    "\r\n 7: 1CH + 7CH"
    "\r\n 8: 1CH + 2CH PIP "
};

char gDemo_displayLayoutMenuDecDemoOnly[] = {
    "\r\n 9: 4x5 CH"
#ifdef TI_816X_BUILD
    "\r\n a: 5x5 CH"
    "\r\n b: 5x6 CH"
    "\r\n c: 6x6 CH"
#endif
};

char gDemo_displayLayoutMenuEnd[] = {
    "\r\n"
    "\r\n Enter Choice: "
};

char gDemo_displayMenu[] = {
    "\r\n ====================="
    "\r\n Select Display"
    "\r\n ====================="
    "\r\n"
    "\r\n 1: ON-Chip HDMI"
    "\r\n 2: VGA / HDCOMP "
    "\r\n 3: OFF-Chip HDMI"
    "\r\n 4: SD "
    "\r\n"
    "\r\n Enter Choice: "

};

char gDemo_ResolutionMenu[] = {
    "\r\n ====================="
    "\r\n Select Display"
    "\r\n ====================="
    "\r\n"
    "\r\n 1: 1080P60"
    "\r\n 2: 720P60"
    "\r\n 3: XGA"
    "\r\n 4: SXGA"
    "\r\n 5: NTSC"
    "\r\n 6: PAL"
    "\r\n"
    "\r\n Enter Choice: "

};
char gDemo_displayHDDemoChanMenu[] = {
    "\r\n ********                 Channel Mapping                 *******"
    "\r\n Channel 0 - Physical Channel 0   Channel 1 - Physical Channel  4"
    "\r\n Channel 2 - Physical Channel 8   Channel 3 - Physical Channel 12"
    "\r\n"
};
/*
  * This API is to reset the BIG LIVE channel(s) in a old layout to 30 / 25 fps
  */
Void Demo_displayResetFps(VDIS_MOSAIC_S *vdMosaicParam, UInt32 layoutId)
    {
       /* Set outputFPS for 814x usecases generating 60fps for channels shown as bigger window in some layouts */
#ifdef TI_814X_BUILD
        Int32 currentFrameRate = Demo_swMsGetOutputFPS(vdMosaicParam);
        if (currentFrameRate == 50)
            currentFrameRate = 25;

        if (currentFrameRate == 60)
            currentFrameRate = 30;

        switch (layoutId)
        {
            /* Stream ID 0 refers to live channel for Vcap_setFrameRate */
            case DEMO_LAYOUT_MODE_4CH:
                printf ("4CH Layout, Resetting FPS of CH%d %d %d %d to %d/%dfps\n",
                        vdMosaicParam->chnMap[0],
                        vdMosaicParam->chnMap[1],
                        vdMosaicParam->chnMap[2],
                        vdMosaicParam->chnMap[3],
                        currentFrameRate*2, currentFrameRate
                    );
                Vcap_setFrameRate(vdMosaicParam->chnMap[0], 0, currentFrameRate*2, currentFrameRate);
                Vcap_setFrameRate(vdMosaicParam->chnMap[1], 0, currentFrameRate*2, currentFrameRate);
                Vcap_setFrameRate(vdMosaicParam->chnMap[2], 0, currentFrameRate*2, currentFrameRate);
                Vcap_setFrameRate(vdMosaicParam->chnMap[3], 0, currentFrameRate*2, currentFrameRate);
                break;

            case DEMO_LAYOUT_MODE_1CH:
                printf ("1CH Layout, Resetting FPS of CH%d to %d/%dfps\n",
                        vdMosaicParam->chnMap[0],
                        currentFrameRate*2, currentFrameRate
                    );

                Vcap_setFrameRate(vdMosaicParam->chnMap[0], 0, currentFrameRate*2, currentFrameRate);
                break;
        }
#else
    Int32 i=0;
    for (i=0; i<Vcap_getNumChannels(); i++)
        Vcap_setFrameRate(i, 0, 60, 0);
#endif
    }


/*
  * This API is to set have the LIVE channel(s) shown in bigger window <in some layouts> to be rendered
  *  at 60 / 50 fps for some 814x usecase
  */
Void Demo_displayChangeFpsForLayout (VDIS_MOSAIC_S *vdMosaicParam, UInt32 layoutId)
{
   /* Set outputFPS for 814x usecases generating 60fps for channels shown as bigger window in some layouts */
#ifdef TI_814X_BUILD
    Int32 i, currentFrameRate = Demo_swMsGetOutputFPS(vdMosaicParam);
    if (currentFrameRate == 50)
        currentFrameRate = 25;

    if (currentFrameRate == 60)
        currentFrameRate = 30;

    switch (layoutId)
    {
        /* Stream ID 0 refers to live channel for Vcap_setFrameRate */
        case DEMO_LAYOUT_MODE_4CH:
            printf ("4CH Layout, Setting FPS of CH%d %d %d %d to %d/%dfps\n",
                    vdMosaicParam->chnMap[0],
                    vdMosaicParam->chnMap[1],
                    vdMosaicParam->chnMap[2],
                    vdMosaicParam->chnMap[3],
                    currentFrameRate*2, currentFrameRate*2
                );
            /* Reset all capture channels fps */
            for (i=0; i<Vcap_getNumChannels(); i++)
                Vcap_setFrameRate(i, 0, currentFrameRate*2, currentFrameRate);
            Vcap_setFrameRate(vdMosaicParam->chnMap[0], 0, currentFrameRate*2, currentFrameRate*2);
            Vcap_setFrameRate(vdMosaicParam->chnMap[1], 0, currentFrameRate*2, currentFrameRate*2);
            Vcap_setFrameRate(vdMosaicParam->chnMap[2], 0, currentFrameRate*2, currentFrameRate*2);
            Vcap_setFrameRate(vdMosaicParam->chnMap[3], 0, currentFrameRate*2, currentFrameRate*2);
            Demo_swMsSetOutputFPS(vdMosaicParam, currentFrameRate*2);
            break;

        case DEMO_LAYOUT_MODE_1CH:
            printf ("1CH Layout, Setting FPS of CH%d to %d/%dfps\n",
                    vdMosaicParam->chnMap[0],
                    currentFrameRate*2, currentFrameRate*2
                );

            /* Reset all capture channels fps */
            for (i=0; i<Vcap_getNumChannels(); i++)
                Vcap_setFrameRate(i, 0, currentFrameRate*2, currentFrameRate);
            Vcap_setFrameRate(vdMosaicParam->chnMap[0], 0, currentFrameRate*2, currentFrameRate*2);
            Demo_swMsSetOutputFPS(vdMosaicParam, currentFrameRate*2);
            break;

        default:
            printf ("NORMAL Layout, Setting FPS of all channels to %d/%dfps\n",
                    currentFrameRate*2, currentFrameRate
                );
            /* swMS fps would be have been modified during layout generation; change only Capture frame rate */
            for (i=0; i<Vcap_getNumChannels(); i++)
                Vcap_setFrameRate(i, 0, currentFrameRate*2, currentFrameRate);
    }
#else
    Int32 i;

    for (i=0; i<vdMosaicParam->numberOfWindows; i++)
    {
        VSYS_PARAMS_S sysContextInfo;
        Vsys_getContext(&sysContextInfo);
        if(sysContextInfo.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC)
        {
            if((vdMosaicParam->chnMap[i] < Vcap_getNumChannels()) && (vdMosaicParam->useLowCostScaling[i] == FALSE))
                Vcap_setFrameRate(vdMosaicParam->chnMap[i],0,60,30);
        }
        else
            if(vdMosaicParam->chnMap[i] < Vcap_getNumChannels())
                Vcap_setFrameRate(vdMosaicParam->chnMap[i],0,60,30);

    }
#endif
}


int Demo_displaySwitchChn(int devId, int startChId)
{
    UInt32 chMap[VDIS_MOSAIC_WIN_MAX];
    int i;

    for(i=0;i<VDIS_MOSAIC_WIN_MAX;i++)
    {
         if (i < gDemo_info.maxVdisChannels)
            chMap[i] = (i+startChId)%gDemo_info.maxVdisChannels;
         else
            chMap[i] = DEMO_SW_MS_INVALID_ID;

    }
    Vdis_setMosaicChn(devId, chMap);

    /* wait for the info prints to complete */
    OSA_waitMsecs(100);

    return 0;
}

int Demo_displaySwitchQueue(int devId, int queueId)
{
    Vdis_switchActiveQueue(devId,queueId);
    return 0;
}

int Demo_displaySwitchSDChan(int devId, int chId)
{
    Vdis_switchSDTVChId(devId, chId);
    return 0;
}
int Demo_displaySwitchChannel(int devId, int chId)
{
    Vdis_switchActiveChannel(devId,chId);
    return 0;
}

int Demo_displayChnEnable(int chId, Bool enable)
{
    if(chId >= gDemo_info.maxVdisChannels)
    {
        return -1;
    }

    if(enable)
    {
        Vdis_enableChn(VDIS_DEV_HDMI,chId);
        Vdis_enableChn(VDIS_DEV_HDCOMP,chId);
        Vdis_enableChn(VDIS_DEV_SD,chId);
    }
    else
    {
        Vdis_disableChn(VDIS_DEV_HDMI,chId);
        Vdis_disableChn(VDIS_DEV_HDCOMP,chId);
        Vdis_disableChn(VDIS_DEV_SD,chId);
    }

    /* wait for the info prints to complete */
    OSA_waitMsecs(100);

    return 0;
}

int Demo_displayGetLayoutId(int demoId)
{
    char ch;
    int layoutId = DEMO_LAYOUT_MODE_4CH_4CH;
    Bool done = FALSE;

    while(!done)
    {
        printf(gDemo_displayLayoutMenu);

        if (demoId == DEMO_VDEC_VDIS)
            printf(gDemo_displayLayoutMenuDecDemoOnly);

        printf(gDemo_displayLayoutMenuEnd);

        ch = Demo_getChar();

        done = TRUE;

        switch(ch)
        {
            case '1':
                layoutId = DEMO_LAYOUT_MODE_1CH;
                break;
            case '2':
                layoutId = DEMO_LAYOUT_MODE_4CH;
                break;
            case '3':
                layoutId = DEMO_LAYOUT_MODE_9CH;
                break;
            case '4':
                layoutId = DEMO_LAYOUT_MODE_16CH;
                break;
            case '5':
                layoutId = DEMO_LAYOUT_MODE_4CH_4CH;
                break;
            case '6':
                layoutId = DEMO_LAYOUT_MODE_6CH;
                break;
            case '7':
                layoutId = DEMO_LAYOUT_MODE_7CH_1CH;
                break;
            case '8':
                layoutId = DEMO_LAYOUT_MODE_2CH_PIP;
                break;
            case '9':
                layoutId = DEMO_LAYOUT_MODE_20CH_4X5;
                break;
#ifndef TI_814X_BUILD
            case 'a':
                layoutId = DEMO_LAYOUT_MODE_25CH_5X5;
                break;
            case 'b':
                layoutId = DEMO_LAYOUT_MODE_30CH_5X6;
                break;
            case 'c':
                layoutId = DEMO_LAYOUT_MODE_36CH_6X6;
                break;
#endif
            default:
                done = FALSE;
                break;
        }
    }

    return layoutId;
}

int Demo_displaySetResolution(UInt32 displayId, UInt32 resolution)
{
    VDIS_MOSAIC_S vdisMosaicParams;
#if USE_FBDEV
    UInt32 outWidth, outHeight;
    Char gBuff[100];
#endif

#if USE_FBDEV
#ifdef TI_814X_BUILD
    /* Disable graphics through sysfs entries */
    if (displayId == VDIS_DEV_HDMI || displayId == VDIS_DEV_DVO2 ) {
        VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 0, 0)
    }
#endif
#ifdef TI_816X_BUILD
    /* Disable graphics through sysfs entries */
    VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 0, 0)
    VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 0, 1)
#endif

    if (displayId == VDIS_DEV_SD ) {
        VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 0, 2)
    }
#endif

    Vdis_stopDrv(displayId);

    memset(&vdisMosaicParams, 0, sizeof(VDIS_MOSAIC_S));

    /* Start with default layout */
    Demo_swMsGenerateLayout(displayId, 0, gDemo_info.maxVdisChannels,
            DEMO_LAYOUT_MODE_7CH_1CH,
            &vdisMosaicParams, FALSE,
            gDemo_info.Type,
            resolution);
    Demo_displayResetFps(&vdisMosaicParams,DEMO_LAYOUT_MODE_7CH_1CH);
#ifdef TI_816X_BUILD
    Demo_displayChangeFpsForLayout(&vdisMosaicParams,DEMO_LAYOUT_MODE_7CH_1CH);
#endif
    Vdis_setMosaicParams(displayId, &vdisMosaicParams);

    Vdis_setResolution(displayId, resolution);
    Vdis_startDrv(displayId);
#if USE_FBDEV

#ifdef TI_814X_BUILD
    Demo_swMsGetOutSize(resolution, &outWidth, &outHeight);

    if((displayId == VDIS_DEV_HDMI) || (displayId == VDIS_DEV_DVO2))
        grpx_fb_scale(VDIS_DEV_HDMI, 0, 0, outWidth, outHeight);
    if((displayId == VDIS_DEV_SD))
        grpx_fb_scale(VDIS_DEV_SD, 0, 0, outWidth, outHeight);
    /* Enable graphics through sysfs entries */
    if (displayId == VDIS_DEV_HDMI || displayId == VDIS_DEV_DVO2 ) {
        VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 1, 0)
    }
#endif

#ifdef TI_816X_BUILD

    Demo_swMsGetOutSize(resolution, &outWidth, &outHeight);

    if(displayId==VDIS_DEV_HDMI)
        grpx_fb_scale(VDIS_DEV_HDMI, 0, 0, outWidth, outHeight);

    /* Enable graphics through sysfs entries */
    VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 1, 0)
    VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 1, 1)
#endif
    if (displayId == VDIS_DEV_SD ) {
        VDIS_CMD_ARG2(gBuff, VDIS_SET_GRPX, 1, 2)
    }

#endif

    return 0;
}

int Demo_displaySettings(int demoId)
{
    Bool done = FALSE;
    char ch;
    static VDIS_MOSAIC_S vdMosaicParam;
    UInt32 chId, startChId, displayId, resolution;
    static int layoutId = DEMO_LAYOUT_MODE_9CH; // Have this static to use layoutId in Demo_displayChangeFpsForLayout() when channel remap alone happens
    Bool validRes = FALSE;
    static Int32 queueNo; // Should be static to retain current queue


    VDIS_DEV devId;
    Bool forceLowCostScale = FALSE;

    if ((demoId == DEMO_VCAP_VENC_VDEC_VDIS_INTERLACED) ||
        (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH))
        forceLowCostScale = TRUE;

    if(gDemo_info.maxVdisChannels<=0)
    {
        printf(" \n");
        printf(" WARNING: Display NOT enabled, this menu is NOT valid !!!\n");
        return -1;
    }

    while(!done)
    {
        printf(gDemo_displaySettingsMenu);

        ch = Demo_getChar();

        switch(ch)
        {
            case '1':
                chId = Demo_getChId("DISPLAY", gDemo_info.maxVdisChannels);

                Demo_displayChnEnable(chId, FALSE);
                break;

            case '2':
                chId = Demo_getChId("DISPLAY", gDemo_info.maxVdisChannels);

                Demo_displayChnEnable(chId, TRUE);
                break;

            case '3':
                if (demoId != VSYS_USECASE_MULTICHN_HD_VCAP_VENC)
                {
                layoutId = Demo_displayGetLayoutId(demoId);

                devId = VDIS_DEV_HDMI;
                startChId = 0;

                Demo_swMsGenerateLayout(devId, startChId, gDemo_info.maxVdisChannels,
                    layoutId, &vdMosaicParam, forceLowCostScale, gDemo_info.Type, Vdis_getResolution(devId) );
#ifdef TI_816X_BUILD
                if (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE)
                {
                                Demo_displayResetFps(&vdMosaicParam,layoutId);
                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
                }
#endif

#ifdef TI_814X_BUILD
                Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
#endif
                Vdis_setMosaicParams(devId,&vdMosaicParam);

                devId = VDIS_DEV_HDCOMP;
                startChId = 0;

                /* if the number of channels being display are more than that can fit in 4x4 then
                    make the other channels appear on the second HD Display.
                    Otherwise show same channels on the other HD Display
                */
                if(gDemo_info.maxVdisChannels>VDIS_MOSAIC_WIN_MAX)
                    startChId = VDIS_MOSAIC_WIN_MAX;

                Demo_swMsGenerateLayout(devId, startChId, gDemo_info.maxVdisChannels,
                    layoutId, &vdMosaicParam, forceLowCostScale, gDemo_info.Type, Vdis_getResolution(devId));
#ifdef TI_816X_BUILD
                if (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE)
                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
#endif

                Vdis_setMosaicParams(devId,&vdMosaicParam);

                devId = VDIS_DEV_SD;
                startChId = 0;

                Demo_swMsGenerateLayout(devId, startChId, gDemo_info.maxVdisChannels,
                    layoutId, &vdMosaicParam, forceLowCostScale, gDemo_info.Type, Vdis_getResolution(devId));

#ifdef TI_814X_BUILD
                Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
#endif

#ifdef TI_816X_BUILD
                if (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE)
                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
#endif

                Vdis_setMosaicParams(devId,&vdMosaicParam);


                /* wait for the info prints to complete */
                OSA_waitMsecs(500);
                }
                else
                {
                    printf(" This is not supported in this usecase");
                    OSA_waitMsecs(100);
                }

                break;

            case '4':
                if (demoId != VSYS_USECASE_MULTICHN_HD_VCAP_VENC)
                {
#ifdef TI_814X_BUILD
                chId = Demo_getChId("DISPLAY (HDMI)", gDemo_info.maxVdisChannels);

                if (Vdis_getMosaicParams(VDIS_DEV_HDMI,&vdMosaicParam) >= 0)
                {
                    Demo_displayResetFps(&vdMosaicParam,layoutId);
                }
                Demo_displaySwitchChn(VDIS_DEV_HDMI, chId);

                if (Vdis_getMosaicParams(VDIS_DEV_HDMI,&vdMosaicParam) >= 0)
                {
                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
                }

                /* wait for the info prints to complete */
                OSA_waitMsecs(500);

                chId = Demo_getChId("DISPLAY (SDTV)", gDemo_info.maxVdisChannels);

                if (Vdis_getMosaicParams(VDIS_DEV_SD,&vdMosaicParam) >= 0)
                {
                    Demo_displayResetFps(&vdMosaicParam,layoutId);
                }

                Demo_displaySwitchChn(VDIS_DEV_SD, chId);

                if (Vdis_getMosaicParams(VDIS_DEV_SD,&vdMosaicParam) >= 0)
                {
                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
                }

                /* wait for the info prints to complete */
                OSA_waitMsecs(500);
#else
                chId = Demo_getChId("DISPLAY (HDMI/SDTV)", gDemo_info.maxVdisChannels);

                Demo_displaySwitchChn(VDIS_DEV_HDMI, chId);

                if (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE)
                {
                    Vdis_getMosaicParams(VDIS_DEV_HDMI,&vdMosaicParam);

                    Demo_displayResetFps(&vdMosaicParam,layoutId);

                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
                }
                /* wait for the info prints to complete */
                OSA_waitMsecs(500);

                /* SDTV if enabled follows HDMI */
                Demo_displaySwitchChn(VDIS_DEV_SD, chId);

                if (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE)
                {
                    Vdis_getMosaicParams(VDIS_DEV_HDMI,&vdMosaicParam);

                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
                }
                /* wait for the info prints to complete */
                OSA_waitMsecs(500);

                chId = Demo_getChId("DISPLAY (HDCOMP/DVO2)", gDemo_info.maxVdisChannels);

                Demo_displaySwitchChn(VDIS_DEV_HDCOMP, chId);

                if (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE)
                {
                    Vdis_getMosaicParams(VDIS_DEV_HDMI,&vdMosaicParam);


                    Demo_displayChangeFpsForLayout(&vdMosaicParam,layoutId);
                }
#endif
                }
                else
                {
                    printf(" This is not supported in this usecase");
                    OSA_waitMsecs(100);
                }
                /* wait for the info prints to complete */
                OSA_waitMsecs(500);

                break;
            case '5':
                if (demoId != VSYS_USECASE_MULTICHN_HD_VCAP_VENC)
                {
                printf(gDemo_displayMenu);
                displayId = Demo_getIntValue("Display Id", 1, 4, 1);
                displayId -= 1;
#ifdef TI_814X_BUILD
                if (displayId == VDIS_DEV_HDCOMP) {
                    printf("\nVenc Not supported !!\n");
                }
                else {
#endif
                if ((demoId == VSYS_USECASE_MULTICHN_VCAP_VENC) && (displayId == VDIS_DEV_HDCOMP || displayId == VDIS_DEV_DVO2) )
                {
                    printf(" This is not supported in this usecase");
                    OSA_waitMsecs(100);
                    break;
                }
                printf(gDemo_ResolutionMenu);
                resolution = Demo_getIntValue("Display Id", 1, 6, 1);
                switch(resolution) {
                    case 1:
                        if (displayId != VDIS_DEV_SD)
                        {
                            resolution = VSYS_STD_1080P_60;
                            validRes   = TRUE;
                        }
                        else
                        {
                            printf("\n Resolution Not supported !!\n");
                        }
                    break;
                    case 2:
                        if (displayId != VDIS_DEV_SD)
                        {
                            resolution = VSYS_STD_720P_60;
                            validRes   = TRUE;
                        }
                        else
                        {
                            printf("\n Resolution Not supported !!\n");
                        }
                    break;
                    case 3:
                        if (displayId != VDIS_DEV_SD)
                        {
                            resolution = VSYS_STD_XGA_60;
                            validRes   = TRUE;
                        }
                        else
                        {
                            printf("\n Resolution Not supported !!\n");
                        }
                    break;
                    case 4:
                        if (displayId != VDIS_DEV_SD)
                        {
                            resolution = VSYS_STD_SXGA_60;
                            validRes   = TRUE;
                        }
                        else
                        {
                            printf("\n Resolution Not supported !!\n");
                        }
                    break;
                    case 5:
                        if (displayId == VDIS_DEV_SD)
                        {
                            resolution = VSYS_STD_NTSC;
                            validRes   = TRUE;
                        }
                        else
                        {
                            printf("\n Resolution Not supported !!\n");
                        }
                    break;
                    case 6:
                        if (displayId == VDIS_DEV_SD)
                        {
                            resolution = VSYS_STD_PAL;
                            validRes   = TRUE;
                        }
                        else
                        {
                            printf("\n Resolution Not supported !!\n");
                        }
                    break;
                    default:
                        resolution = VSYS_STD_1080P_60;

                }

                if (validRes) {
                    Demo_displaySetResolution(displayId, resolution);
                }
                validRes = FALSE;
#ifdef TI_814X_BUILD
            }
#endif
            }
            else
            {
                printf(" This is not supported in this usecase");
                OSA_waitMsecs(100);
            }
            break;
            case '6':
                {
                    queueNo = Demo_getIntValue("DISPLAY ACTIVE QUEUE(0/1) (only for SD)",
                                            0,
                                            1,
                                            0);
                    if (queueNo == 0)
                    {
                        /* Queue 0 has only 1 channel. Reset to Ch 0 while switching to Queue 0*/
                        printf ("Resetting to Ch 0\n");
                        Demo_displaySwitchSDChan(VDIS_DEV_SD,0);
                    }
                    Demo_displaySwitchQueue(VDIS_DEV_SD, queueNo);
                }
                break;
            case '7':
               if(demoId == VSYS_USECASE_MULTICHN_HD_VCAP_VENC)
               {

                    if(demoId == DEMO_VCAP_VENC_VDIS_HD)
                    {
                          printf(gDemo_displayHDDemoChanMenu);
                    }

                   Demo_displaySwitchChannel(
                      Demo_getIntValue("Select DISPLAY HDMI(0/1)",
                                       0,
                                       1,
                                       0),
                      Demo_getIntValue("DISPLAY Channel No. (0-3)",
                                       0,
                                       3,
                                       0));
               }
               else
               {
                  printf("Not Supported in this usecase\n");
               }
                break;
            case '8':
                if (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE)
                {
                    Int32 chNo;

                    chNo = Demo_getIntValue("SDTV channel:",
                                             0,
                                             gDemo_info.maxVcapChannels-1,
                                             0);
                    #if 0
                    if (queueNo == 0)
                    {
                        chNo = 0;
                    }
                    #endif
                    Demo_displaySwitchSDChan(VDIS_DEV_SD, chNo);
                }
                break;

            case '9':
            case 'a':
                if (demoId != VSYS_USECASE_MULTICHN_HD_VCAP_VENC)
                {
                    {
                        WINDOW_S chnlInfo, winCrop;
                        UInt32 winId = 0;

                        devId = VDIS_DEV_HDMI;

                        Vdis_getChnlInfoFromWinId(devId, winId,&chnlInfo);
                        winCrop.width = chnlInfo.width/2;
                        winCrop.height = chnlInfo.height/2;

                        if (ch == '9')
                        {
                            winCrop.start_X = chnlInfo.start_X;
                            winCrop.start_Y = chnlInfo.start_Y;
                        }
                        else
                        {
                            winCrop.start_X = chnlInfo.start_X + chnlInfo.width/4;
                            winCrop.start_Y = chnlInfo.start_Y + chnlInfo.height/4;
                        }

                        Vdis_SetCropParam(devId, winId,winCrop);
                    }
                }
                else
                {
                    printf(" This is not supported in this usecase");
                    OSA_waitMsecs(100);
                }
                break;
            case 'p':
                done = TRUE;
                break;

            case 'b':
                grpx_fb_demo();
                break;
        }
    }

    return 0;

}



