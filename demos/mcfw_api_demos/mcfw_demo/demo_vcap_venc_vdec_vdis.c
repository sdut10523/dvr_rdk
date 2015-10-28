
/**
  \file demo_vcap_venc_vdec_vdis.c
  \brief
  */

#include <demo_vcap_venc_vdec_vdis.h>
#include <demo_scd_bits_wr.h>

/* Setting secondary out <CIF> for 30 frames - this is the validated frame rate;
any higher number will impact performance. */

#ifdef TI_816X_BUILD
#define     CIF_FPS_ENC_NTSC         (30)
#define     CIF_FPS_ENC_PAL          (25)
#endif
#ifdef TI_814X_BUILD
#define     CIF_FPS_ENC_NTSC         (30)
#define     CIF_FPS_ENC_PAL          (25)
#endif
#define     CIF_BITRATE         (500)
#define     MJPEG_BITRATE       (100)


/* used to set resolution / buf size of ipcbits for varying resolutions based on usecase */
VcapVencVdecVdis_res    ipcbits_res;
Scd_Ctrl gScd_ctrl;

Void VcapVencVdecVdis_setChannels(int demoId, int *pEnable2ndOut)
{
    switch (demoId)
    {
        case    DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE:
            ipcbits_res.width = MCFW_IPCBITS_D1_WIDTH;
            ipcbits_res.height = MCFW_IPCBITS_D1_HEIGHT;
#ifdef TI_814X_BUILD
            gDemo_info.maxVcapChannels = 4;
            gDemo_info.maxVdisChannels = 8;
            gDemo_info.maxVencChannels = 4;
            gDemo_info.maxVdecChannels = 4;
            gDemo_info.VsysNumChs  = 4;
#else
            gDemo_info.maxVcapChannels = 16;
            gDemo_info.maxVdisChannels = 32;
            gDemo_info.maxVencChannels = 16;
            gDemo_info.maxVdecChannels = 16;
            gDemo_info.VsysNumChs  = 16;
#endif
            break;

        case    DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_NON_D1:

            ipcbits_res.width = MCFW_IPCBITS_CIF_WIDTH;
            ipcbits_res.height = MCFW_IPCBITS_CIF_HEIGHT;

            gDemo_info.maxVcapChannels = 16;
            gDemo_info.maxVdisChannels = 32;
            gDemo_info.maxVencChannels = 16;
            gDemo_info.maxVdecChannels = 16;
            gDemo_info.VsysNumChs  = 16;

            break;

        case    DEMO_VCAP_VENC_VDEC_VDIS_INTERLACED:
            ipcbits_res.width = MCFW_IPCBITS_D1_WIDTH;
            ipcbits_res.height = MCFW_IPCBITS_D1_HEIGHT;
#ifdef TI_816X_BUILD
            gDemo_info.maxVcapChannels = 16;
            gDemo_info.maxVdisChannels = 32;
            gDemo_info.maxVencChannels = 16;
            gDemo_info.maxVdecChannels = 16;
            gDemo_info.VsysCifOnly = FALSE;
            gDemo_info.VsysNumChs  = 16;
#endif
            break;
        case    DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_4CH:
            ipcbits_res.width = MCFW_IPCBITS_D1_WIDTH;
            ipcbits_res.height = MCFW_IPCBITS_D1_HEIGHT;
            gDemo_info.maxVcapChannels = 4;
            gDemo_info.maxVdisChannels = 8;
            gDemo_info.maxVencChannels = 4;
            gDemo_info.maxVdecChannels = 4;
            gDemo_info.VsysNumChs  = 4;
            break;

        case    DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH:
            ipcbits_res.width = MCFW_IPCBITS_D1_WIDTH;
            ipcbits_res.height = MCFW_IPCBITS_D1_HEIGHT;
            gDemo_info.maxVcapChannels = 8;
            gDemo_info.maxVdisChannels = 16;
            gDemo_info.maxVencChannels = 8;
            gDemo_info.maxVdecChannels = 8;
            gDemo_info.VsysNumChs  = 8;
            break;

        default:
            break;
    }
}


Void VcapVencVdecVdis_start( Bool doProgressiveVenc, Bool enableSecondaryOut, int demoId)
{
    UInt32 i;
    VSYS_PARAMS_S vsysParams;
    VCAP_PARAMS_S vcapParams;
    VENC_PARAMS_S vencParams;
    VDEC_PARAMS_S vdecParams;
    VDIS_PARAMS_S vdisParams;
    Bool forceLowCostScale = FALSE, enableFWrite = FALSE;
    Int32 Enable2ndOut = enableSecondaryOut;
    VENC_CHN_DYNAMIC_PARAM_S params = { 0 };
    UInt32 chId;
    UInt8 osdFormat[ALG_LINK_OSD_MAX_CH];

    Vsys_params_init(&vsysParams);
    Vcap_params_init(&vcapParams);
    Venc_params_init(&vencParams);
    Vdec_params_init(&vdecParams);
    Vdis_params_init(&vdisParams);

    VcapVencVdecVdis_setChannels(demoId, &Enable2ndOut);


    vcapParams.numChn = gDemo_info.maxVcapChannels;
    vencParams.numPrimaryChn = gDemo_info.maxVencChannels;
    vencParams.numSecondaryChn = 0;
    vdecParams.numChn = gDemo_info.maxVdecChannels;
    vdisParams.numChannels = gDemo_info.maxVdisChannels;
    vsysParams.cifonly = gDemo_info.VsysCifOnly;
    vsysParams.numChs  = gDemo_info.VsysNumChs;

    enableSecondaryOut = (Bool)Enable2ndOut;

    /* Most of progressive use cases have OSD in YUV420 SP format */
    memset(osdFormat, SYSTEM_DF_YUV420SP_UV, ALG_LINK_OSD_MAX_CH);

    if( doProgressiveVenc)
    {
        switch (demoId)
        {
            case  DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE:
                vsysParams.systemUseCase = VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC;
#ifndef TI_814X_BUILD
				memset(osdFormat, SYSTEM_DF_YUV422I_YUYV, ALG_LINK_OSD_MAX_CH);
#endif 
                break;
            case  DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_NON_D1:
                vsysParams.systemUseCase = VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_NON_D1;
                break;
            case  DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_4CH:
                vsysParams.systemUseCase = VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH;
                break;
            case  DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH:
                vsysParams.systemUseCase = VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_8CH;
                break;
            default:
                vsysParams.systemUseCase = VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_NON_D1;
        }

        if (enableSecondaryOut == FALSE)
        {
            vsysParams.enableSecondaryOut = FALSE;
            vsysParams.enableNsf     = FALSE;
        }
        else
        {
            vsysParams.enableSecondaryOut = TRUE;
            vsysParams.enableNsf     = TRUE;
            vsysParams.enableScd     = FALSE;
            vsysParams.enableMjpegEnc = TRUE;
            if((demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_4CH) || 
               (demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE))
               vsysParams.enableScd     = TRUE;
        }

        vsysParams.enableCapture = TRUE;
        vsysParams.enableNullSrc = FALSE;
        vsysParams.enableOsd     = TRUE;

#ifdef TI_814X_BUILD
        vsysParams.enableScd     = TRUE;
        vsysParams.numDeis       = 1;
        vsysParams.numSwMs       = 2;
        vsysParams.numDisplays   = 2;
        vsysParams.enableAVsync  = FALSE;
#else
        vsysParams.numDeis       = 2;
        vsysParams.numSwMs       = 2;
        vsysParams.numDisplays   = 3;
        vsysParams.enableAVsync  = FALSE;
#endif
    }
    else
    {
        vsysParams.systemUseCase = VSYS_USECASE_MULTICHN_INTERLACED_VCAP_VDIS_VENC_VDEC;
        vsysParams.enableCapture = TRUE;
        vsysParams.enableNsf     = TRUE;
        vsysParams.enableNullSrc = FALSE;
        vsysParams.numDeis       = 0;
        vsysParams.enableOsd     = TRUE;
        vsysParams.enableSecondaryOut = enableSecondaryOut;
        vdecParams.forceUseDecChannelParams = TRUE;
#ifdef TI_814X_BUILD
        vsysParams.numSwMs       = 2;
        vsysParams.numDisplays   = 2;
#else
        vsysParams.numSwMs       = 2;
        vsysParams.numDisplays   = 2;
#endif
    }
    if (enableSecondaryOut)
    {
        vencParams.numSecondaryChn = gDemo_info.maxVencChannels;
        if(vsysParams.enableMjpegEnc == FALSE)
           gDemo_info.maxVencChannels *= 2;
        else
           gDemo_info.maxVencChannels *= 3;
    }


    printf ("--------------- CHANNEL DETAILS-------------\n");
    printf ("Capture Channels => %d\n", vcapParams.numChn);
    printf ("Enc Channels => Primary %d, Secondary %d\n", vencParams.numPrimaryChn, vencParams.numSecondaryChn);
    printf ("Dec Channels => %d\n", vdecParams.numChn);
    printf ("Disp Channels => %d\n", vdisParams.numChannels);
    printf ("-------------------------------------------\n");

    /* Override the context here as needed */
    Vsys_init(&vsysParams);

    /* Override the context here as needed */
    Vcap_init(&vcapParams);

    /*Enabling generation of motion vector for channel 0 only,
         * for other channels please add to the below line*/

    vencParams.encChannelParams[0].enableAnalyticinfo = 1;
    //vencParams.encChannelParams[1].enableAnalyticinfo = 1;
    vencParams.encChannelParams[0].maxBitRate = -1;

    if (vsysParams.cifonly == TRUE)
    {
        for (i=0; i < VENC_PRIMARY_CHANNELS; i++)
        {
            vencParams.encChannelParams[i].dynamicParam.targetBitRate = .5 * 1000 * 1000;
        }
    }

    /* Override the context here as needed */
    Venc_init(&vencParams);


    /* Override the context here as needed */
    Vdec_init(&vdecParams);

    /* Override the context here as needed */
    vdisParams.deviceParams[VDIS_DEV_HDMI].resolution   = VSYS_STD_1080P_60;
    vdisParams.deviceParams[VDIS_DEV_HDCOMP].resolution = VSYS_STD_1080P_60;
    Vdis_tiedVencInit(VDIS_DEV_HDCOMP, VDIS_DEV_DVO2, &vdisParams);

    vdisParams.deviceParams[VDIS_DEV_SD].resolution     = VSYS_STD_NTSC;

    vdisParams.enableLayoutGridDraw = FALSE;

    if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_INTERLACED_VCAP_VDIS_VENC_VDEC)
        forceLowCostScale = TRUE;

#ifdef TI_814X_BUILD
    /* set for 2 displays */
    i = 0;
    Demo_swMsGenerateLayout(VDIS_DEV_HDMI, 0, gDemo_info.maxVdisChannels,
                          DEMO_LAYOUT_MODE_7CH_1CH,
                          &vdisParams.mosaicParams[i], forceLowCostScale,
                          gDemo_info.Type,
                          vdisParams.deviceParams[VDIS_DEV_HDMI].resolution);
    vdisParams.mosaicParams[i].userSetDefaultSWMLayout = TRUE;
    i = 1;
    Demo_swMsGenerateLayout(VDIS_DEV_SD, 0, gDemo_info.maxVdisChannels,
                          DEMO_LAYOUT_MODE_7CH_1CH,
                          &vdisParams.mosaicParams[i], forceLowCostScale,
                          gDemo_info.Type,
                          vdisParams.deviceParams[VDIS_DEV_SD].resolution);
    vdisParams.mosaicParams[i].userSetDefaultSWMLayout = TRUE;
#else
    /* set for 3 displays */

    i = 0;
    Demo_swMsGenerateLayout(VDIS_DEV_HDMI, 0, gDemo_info.maxVdisChannels,
                          DEMO_LAYOUT_MODE_16CH,
                          &vdisParams.mosaicParams[i], forceLowCostScale,
                          gDemo_info.Type,
                          vdisParams.deviceParams[VDIS_DEV_HDMI].resolution);
    vdisParams.mosaicParams[i].userSetDefaultSWMLayout = TRUE;


    if (vsysParams.systemUseCase != VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
    {
        i = 1;
        Demo_swMsGenerateLayout(VDIS_DEV_HDCOMP, 16, gDemo_info.maxVdisChannels,
                              DEMO_LAYOUT_MODE_16CH,
                              &vdisParams.mosaicParams[i], forceLowCostScale,
                              gDemo_info.Type,
                              vdisParams.deviceParams[VDIS_DEV_HDCOMP].resolution);
        vdisParams.mosaicParams[i].userSetDefaultSWMLayout = TRUE;
    }
    if (vsysParams.systemUseCase != VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
        i = 2;
    else
        i = 1;

    /* For DM8168 16 CH Progressive usecase, SDTV does not support mosaic.
      * SDTV input is directly fed from the capture link */   
    if(vsysParams.systemUseCase != VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC)
    {
        Demo_swMsGenerateLayout(VDIS_DEV_SD, 0, gDemo_info.maxVdisChannels,
                              DEMO_LAYOUT_MODE_7CH_1CH,
                              &vdisParams.mosaicParams[i], forceLowCostScale,
                              gDemo_info.Type,
                              vdisParams.deviceParams[VDIS_DEV_SD].resolution);
        vdisParams.mosaicParams[i].userSetDefaultSWMLayout = TRUE;
    }
#endif

    Vdis_init(&vdisParams);

    if(vsysParams.enableScd == TRUE)
    {
        Scd_bitsWriteCreate();//Be disabled
    }

    enableFWrite = Demo_getFileWriteEnable();

    /* Init the application specific module which will handle bitstream exchange */
    VcapVencVdecVdis_ipcBitsInit(ipcbits_res, enableFWrite);


    /* Create Link instances and connects compoent blocks */
    Vsys_create();

    if(vsysParams.enableOsd)
    {
		gDemo_info.osdEnable = TRUE;

        /* Create and initialize OSD window buffers */
        Demo_osdInit(gDemo_info.maxVencChannels, osdFormat);	
        if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_8CH)
        {
            /* set different OSD params for the secondary stream */
            for(chId = 8; chId < 16; chId++)
            {
                g_osdChParam[chId].numWindows = 2;
            }
        }
        
        for(chId = 0; chId < gDemo_info.maxVencChannels; chId++)
        {
            vcapParams.channelParams[chId].dynamicParams.osdChWinPrm = &g_osdChParam[chId];
            /* Initailize osdLink with created and set win params */
            Vcap_setDynamicParamChn(chId, &vcapParams.channelParams[chId].dynamicParams, VCAP_OSDWINPRM);
        }
    }

#if USE_FBDEV
    grpx_fb_init(GRPX_FORMAT_RGB565);
#endif

#ifndef SYSTEM_DISABLE_AUDIO
        Demo_audioEnable(TRUE);
#endif

#ifndef TI_814X_BUILD
    if(enableSecondaryOut)
    {
        if(doProgressiveVenc)
        {
            Int32 chId;

            /* now use VIP-SC secondary output, so input to VIP-SC and VIP-SC secondary channel are both
               half of the real input framerate */
            for (chId=0; chId < vencParams.numSecondaryChn ; chId++)
            {
                /* At capture level, CIF stream id is 0. Set for CIF channels 0 ~ MAX_CH */
                Vcap_setFrameRate(chId, 2, 30, 30);
            }

            for (chId=0; chId < vencParams.numPrimaryChn; chId++)
            {
                /* At capture level, D1 stream id is 1. Set for D1 channels 0 ~ MAX_CH */
                if(vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                {
                    Vcap_setFrameRate(chId, 1, 60, 60);
                    Vcap_setFrameRate(chId, 0, 60, 60);
                }
                else
                {
                    Vcap_setFrameRate(chId, 1, 30, 30);

                    Vcap_setFrameRate(chId, 0, 60, 0);
                }

                if(vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC)
                {
                    Int32 i;

                    for (i=0; i<vdisParams.mosaicParams[0].numberOfWindows; i++)
                    {
                        if((vdisParams.mosaicParams[0].chnMap[i] == chId) && (vdisParams.mosaicParams[0].useLowCostScaling[i] == FALSE))
                            Vcap_setFrameRate(chId,0,60,30);
                    }

                    for (i=0; i<vdisParams.mosaicParams[1].numberOfWindows; i++)
                    {
                        if((vdisParams.mosaicParams[1].chnMap[i] == chId) && (vdisParams.mosaicParams[1].useLowCostScaling[i] == FALSE))
                            Vcap_setFrameRate(chId,0,60,30);
                    }
                    for (i=0; i<vdisParams.mosaicParams[2].numberOfWindows; i++)
                    {
                        if((vdisParams.mosaicParams[2].chnMap[i] == chId) && (vdisParams.mosaicParams[2].useLowCostScaling[i] == FALSE))
                            Vcap_setFrameRate(chId,0,60,30);
                    }
                }
            }
        }
        else
        {
            Int32 chId;

            /* now use VIP-SC secondary output, so input to VIP-SC and VIP-SC secondary channel are both
               half of the real input framerate */
            for (chId=0; chId < vencParams.numSecondaryChn ; chId++)
            {
                /* At capture level, CIF stream id is 0. Set for CIF channels 0 ~ MAX_CH */
                Vcap_setFrameRate(chId, 0, 30, 16);
                Vcap_skipFidType(chId, VIDEO_FID_TYPE_BOT_FIELD);
            }

            for (chId=0; chId < vencParams.numPrimaryChn; chId++)
            {
                /* At capture level, D1 stream id is 1. Set for D1 channels 0 ~ MAX_CH */
                Vcap_setFrameRate(chId, 1, 60, 60);
            }

        }
    }
    else
    {
        if(doProgressiveVenc)
        {
            for (chId=0; chId < vencParams.numPrimaryChn; chId++)
            {
                /* At capture level, D1 stream id is 1. Set for D1 channels 0 ~ MAX_CH */
                if(vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                {
                    Vcap_setFrameRate(chId, 1, 60, 30);
                    Vcap_setFrameRate(chId, 0, 60, 60);
                }
                else
                {
                    Vcap_setFrameRate(chId, 1, 60, 30);

                    Vcap_setFrameRate(chId, 0, 60, 0);
                }

                if(vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC)
                {
                    Int32 i;

                    for (i=0; i<vdisParams.mosaicParams[0].numberOfWindows; i++)
                    {
                        if((vdisParams.mosaicParams[0].chnMap[i] == chId) && (vdisParams.mosaicParams[0].useLowCostScaling[i] == FALSE))
                            Vcap_setFrameRate(chId,0,60,30);
                    }

                    for (i=0; i<vdisParams.mosaicParams[1].numberOfWindows; i++)
                    {
                        if((vdisParams.mosaicParams[1].chnMap[i] == chId) && (vdisParams.mosaicParams[1].useLowCostScaling[i] == FALSE))
                            Vcap_setFrameRate(chId,0,60,30);
                    }
                    for (i=0; i<vdisParams.mosaicParams[2].numberOfWindows; i++)
                    {
                        if((vdisParams.mosaicParams[2].chnMap[i] == chId) && (vdisParams.mosaicParams[2].useLowCostScaling[i] == FALSE))
                            Vcap_setFrameRate(chId,0,60,30);
                    }
                }
            }
        }
    }
#else
    if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_8CH)
    {
        for (chId=0; chId < vencParams.numSecondaryChn ; chId++)
        {
            Vcap_skipFidType(chId, VIDEO_FID_TYPE_BOT_FIELD);
        }
    }
#endif

    /* Start components in reverse order */
    Vdis_start();
    Vdec_start();
    Venc_start();
    Vcap_start();

#if USE_FBDEV
     grpx_fb_draw(VDIS_DEV_HDMI);
#ifdef TI_816X_BUILD
     grpx_fb_draw(VDIS_DEV_DVO2);
#endif
#endif

    display_process_init();

    {
        /* Setting FPS for the Encoder Channels */
        for(chId=0; chId<vcapParams.numChn; chId++)
        {
            if(Demo_captureGetSignalStandard() == VSYS_STD_PAL)
            {
                memset(&params, 0, sizeof(params));

                Venc_setInputFrameRate(chId, 25);

                params.frameRate = 25;
                Venc_setDynamicParam(chId, 0, &params, VENC_FRAMERATE);

                if(enableSecondaryOut)
                {
                    memset(&params, 0, sizeof(params));

                    Venc_setInputFrameRate(chId+vencParams.numPrimaryChn, 25);

                    params.frameRate = CIF_FPS_ENC_PAL;
                    params.targetBitRate = CIF_BITRATE * 1000;
                    Venc_setDynamicParam(chId+vencParams.numPrimaryChn, 0, &params, VENC_FRAMERATE);
#ifdef TI_814X_BUILD
                    /* Set MJPEG Encode to 30 fps as frame rate control is done @ NSF for 4D1 case */
                    if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC
                            || vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                    {
                        memset(&params, 0, sizeof(params));

                        Venc_setInputFrameRate(chId+(vencParams.numPrimaryChn * 2), 25);

                        params.frameRate = 25;
                        params.targetBitRate = MJPEG_BITRATE * 1000;

                        Venc_setDynamicParam(chId+(vencParams.numPrimaryChn * 2), 0, &params, VENC_FRAMERATE);
                    }
#endif
#ifdef TI_816X_BUILD
                    /* Set MJPEG Encode to 30 fps as frame rate control is done @ NSF for 4D1 case */
                    if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                    {
                        memset(&params, 0, sizeof(params));

                        Venc_setInputFrameRate(chId+(vencParams.numPrimaryChn * 2), 25);

                        params.frameRate = 25;
                        params.targetBitRate = MJPEG_BITRATE * 1000;

                        Venc_setDynamicParam(chId+(vencParams.numPrimaryChn * 2), 0, &params, VENC_FRAMERATE);
                    }
#endif
                }
            }
            else if(Demo_captureGetSignalStandard() == VSYS_STD_NTSC)
            {
                memset(&params, 0, sizeof(params));
                Venc_setInputFrameRate(chId, 30);

                params.frameRate = 30;

                Venc_setDynamicParam(chId, 0, &params, VENC_FRAMERATE);

                if(enableSecondaryOut)
                {
                    memset(&params, 0, sizeof(params));
                    if (doProgressiveVenc)
                    {
                        Venc_setInputFrameRate(chId+vencParams.numPrimaryChn, 30);
                        params.frameRate = CIF_FPS_ENC_NTSC;
                    }
                    else
                    {
                        Venc_setInputFrameRate(chId+vencParams.numPrimaryChn, 16);
                        params.frameRate = 16;
                    }

                    params.targetBitRate = CIF_BITRATE * 1000;

                    Venc_setDynamicParam(chId+vencParams.numPrimaryChn, 0, &params, VENC_FRAMERATE);
#ifdef TI_814X_BUILD
                    /* Set MJPEG Encode to 30 fps as frame rate control is done @ NSF for 4D1 case */
                    if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC
                            || vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                    {
                        memset(&params, 0, sizeof(params));

                        Venc_setInputFrameRate(chId+(vencParams.numPrimaryChn * 2), 30);

                        params.frameRate = 30;
                        params.targetBitRate = MJPEG_BITRATE * 1000;

                        Venc_setDynamicParam(chId+(vencParams.numPrimaryChn * 2), 0, &params, VENC_FRAMERATE);
                    }
#endif
#ifdef TI_816X_BUILD
                    /* Set MJPEG Encode to 30 fps as frame rate control is done @ NSF for 4D1 case */
                    if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                    {
                        memset(&params, 0, sizeof(params));

                        Venc_setInputFrameRate(chId+(vencParams.numPrimaryChn * 2), 30);

                        params.frameRate = 30;
                        params.targetBitRate = MJPEG_BITRATE * 1000;

                        Venc_setDynamicParam(chId+(vencParams.numPrimaryChn * 2), 0, &params, VENC_FRAMERATE);
                    }
#endif
                }
            }
            else
            {
                memset(&params, 0, sizeof(params));
                Venc_setInputFrameRate(chId, 30);

                params.frameRate = 30;

                Venc_setDynamicParam(chId, 0, &params, VENC_FRAMERATE);

                if(enableSecondaryOut)
                {
                    memset(&params, 0, sizeof(params));
                    if (doProgressiveVenc)
                    {
                        Venc_setInputFrameRate(chId+vencParams.numPrimaryChn, 30);
                        params.frameRate = CIF_FPS_ENC_NTSC;
                    }
                    else
                    {
                        Venc_setInputFrameRate(chId+vencParams.numPrimaryChn, 16);
                        params.frameRate = 16;
                    }

                    params.targetBitRate = CIF_BITRATE * 1000;

                    Venc_setDynamicParam(chId+vencParams.numPrimaryChn, 0, &params, VENC_FRAMERATE);

                }

#ifdef TI_814X_BUILD
                /* Set MJPEG Encode to 30 fps as frame rate control is done @ NSF for 4D1 case */
                if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC
                        || vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                {
                    memset(&params, 0, sizeof(params));

                    Venc_setInputFrameRate(chId+(vencParams.numPrimaryChn * 2), 1);

                    params.frameRate = 1;
                    params.targetBitRate = MJPEG_BITRATE * 1000;

                    Venc_setDynamicParam(chId+(vencParams.numPrimaryChn * 2), 0, &params, VENC_FRAMERATE);
                }
#endif
#ifdef TI_816X_BUILD
                /* Set MJPEG Encode to 30 fps as frame rate control is done @ NSF for 4D1 case */
                if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_4CH)
                {
                    memset(&params, 0, sizeof(params));

                    Venc_setInputFrameRate(chId+(vencParams.numPrimaryChn * 2), 1);

                    params.frameRate = 1;
                    params.targetBitRate = MJPEG_BITRATE * 1000;

                    Venc_setDynamicParam(chId+(vencParams.numPrimaryChn * 2), 0, &params, VENC_FRAMERATE);
                }
#endif
                printf (" DEMO: No video detected at CH [%d] !!!\n",
                     chId);

            }
        }

        #ifdef TI_814X_BUILD
        if (vsysParams.systemUseCase == VSYS_USECASE_MULTICHN_PROGRESSIVE_VCAP_VDIS_VENC_VDEC_8CH)
        {
        VENC_CHN_DYNAMIC_PARAM_S params_venc = { 0 };
        VCAP_CHN_DYNAMIC_PARAM_S params_vcap = { 0 };
        /*HACK HACK set secondary enc fps to 1fps */
        memset(&params_venc, 0, sizeof(params_venc));
        params_venc.frameRate = 30;
        for (chId = 8; chId < 16; chId++)
        {
            Venc_setInputFrameRate(chId, 30);
            Venc_setDynamicParam(chId, 0, &params_venc, VENC_FRAMERATE);
        }
        memset(&params_vcap, 0, sizeof(params_vcap));
        params_vcap.chDynamicRes.pathId = 1;
        params_vcap.chDynamicRes.width  = 704;
        params_vcap.chDynamicRes.height = 480;
        Vcap_setDynamicParamChn(0, &params_vcap, VCAP_RESOLUTION);
        Vcap_setDynamicParamChn(4, &params_vcap, VCAP_RESOLUTION);
        params_vcap.chDynamicRes.width  = 352;
        params_vcap.chDynamicRes.height = 240;
        Vcap_setDynamicParamChn(1, &params_vcap, VCAP_RESOLUTION);
        Vcap_setDynamicParamChn(2, &params_vcap, VCAP_RESOLUTION);
        Vcap_setDynamicParamChn(3, &params_vcap, VCAP_RESOLUTION);
        Vcap_setDynamicParamChn(5, &params_vcap, VCAP_RESOLUTION);
        Vcap_setDynamicParamChn(6, &params_vcap, VCAP_RESOLUTION);
        Vcap_setDynamicParamChn(7, &params_vcap, VCAP_RESOLUTION);
        }
        #endif
    }

}

Void VcapVencVdecVdis_stop()
{
    VSYS_PARAMS_S contextInf;
    Vsys_getContext(&contextInf);
    display_process_deinit();

    if(contextInf.enableScd)
        Scd_bitsWriteStop();

    VcapVencVdecVdis_ipcBitsStop();
    /* Stop components */
    Vcap_stop();
    Venc_stop();
    Vdec_stop();
    Vdis_stop();

#if USE_FBDEV
     grpx_fb_exit();
#endif

	 /* Free the osd buffers */
    Demo_osdDeinit();	

    Vsys_delete();
    if(contextInf.enableScd)
        Scd_bitsWriteDelete();


    VcapVencVdecVdis_ipcBitsExit();

    /* De-initialize components */
    Vcap_exit();
    Venc_exit();
    Vdec_exit();
    Vdis_exit();
    Vsys_exit();

    #ifndef SYSTEM_DISABLE_AUDIO
    Demo_audioEnable(FALSE);
    #endif
}

#ifdef TI_814X_BUILD
/* TODO: add NTSC/PAL check ...*/
int Demo_change8ChMode(int demoId)
{
    int value;
    int chId;

    VENC_CHN_DYNAMIC_PARAM_S params_venc = { 0 };
    VCAP_CHN_DYNAMIC_PARAM_S params_vcap = { 0 };

    if (demoId != DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH)
    {
        printf("This function is valid ONLY for DM814X 8CH usecase!!!!!\n");
        return 0;
    }

    value = Demo_getIntValue("Select Mode(0:2D1+6CIF, 1:8 2CIF, 2:8D1 non-realtime)", 0, 2, 0);

    /*HACK HACK set secondary enc fps to 1fps */
    memset(&params_venc, 0, sizeof(params_venc));
    params_venc.frameRate = 30;
    for (chId = 8; chId < 16; chId++)
    {
        Venc_setInputFrameRate(chId, 30);
        Venc_setDynamicParam(chId, 0, &params_venc, VENC_FRAMERATE);
    }
    switch(value)
    {
        case 0:
            memset(&params_vcap, 0, sizeof(params_vcap));
            memset(&params_venc, 0, sizeof(params_venc));
            params_venc.frameRate = 30;
            for (chId = 0; chId < 8; chId++)
            {
                Venc_setInputFrameRate(chId, 30);
                Venc_setDynamicParam(chId, 0, &params_venc, VENC_FRAMERATE);
            }
            params_vcap.chDynamicRes.pathId = 1;
            params_vcap.chDynamicRes.width  = 704;
            params_vcap.chDynamicRes.height = 480;
            Vcap_setDynamicParamChn(0, &params_vcap, VCAP_RESOLUTION);
            Vcap_setDynamicParamChn(4, &params_vcap, VCAP_RESOLUTION);
            params_vcap.chDynamicRes.width  = 352;
            params_vcap.chDynamicRes.height = 240;
            Vcap_setDynamicParamChn(1, &params_vcap, VCAP_RESOLUTION);
            Vcap_setDynamicParamChn(2, &params_vcap, VCAP_RESOLUTION);
            Vcap_setDynamicParamChn(3, &params_vcap, VCAP_RESOLUTION);
            Vcap_setDynamicParamChn(5, &params_vcap, VCAP_RESOLUTION);
            Vcap_setDynamicParamChn(6, &params_vcap, VCAP_RESOLUTION);
            Vcap_setDynamicParamChn(7, &params_vcap, VCAP_RESOLUTION);
            break;
        case 1:
            memset(&params_vcap, 0, sizeof(params_vcap));
            memset(&params_venc, 0, sizeof(params_venc));
            params_vcap.chDynamicRes.pathId = 1;
            params_vcap.chDynamicRes.width  = 704;
            params_vcap.chDynamicRes.height = 240;
            params_venc.frameRate = 30;
            for (chId = 0; chId < 8; chId++)
            {
                Vcap_setDynamicParamChn(chId, &params_vcap, VCAP_RESOLUTION);
                Venc_setInputFrameRate(chId, 30);
                Venc_setDynamicParam(chId, 0, &params_venc, VENC_FRAMERATE);
            }
            break;
        case 2:
            memset(&params_vcap,  0, sizeof(params_vcap));
            memset(&params_venc, 0, sizeof(params_venc));
            params_vcap.chDynamicRes.pathId = 1;
            params_vcap.chDynamicRes.width  = 704;
            params_vcap.chDynamicRes.height = 480;
            params_venc.frameRate = 15;
            for (chId = 0; chId < 8; chId++)
            {
                Vcap_setDynamicParamChn(chId, &params_vcap, VCAP_RESOLUTION);
                Venc_setInputFrameRate(chId, 30);
                Venc_setDynamicParam(chId, 0, &params_venc, VENC_FRAMERATE);

            }
            break;
        default:
            break;
    }



    return 0;
}
#endif

