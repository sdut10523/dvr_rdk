/*
	Capture (YUV422I) 16CH D1 60fps
					|
			NSF (YUV420SP)
					|
			IPCFRAMEOUT(VPS) <-->IPCFRAMEINDSPIN - ALG LINK
					|
		SW Mosaic(SC5 YUV422I)
					|
		On-Chip HDMI 1080p60
 */

#include "multich_common.h"

/* =============================================================================
 * Externs
 * =============================================================================
 */


#define     NUM_CAPTURE_DEVICES          (4)



/* =============================================================================
 * Use case code
 * =============================================================================
 */

Void MultiCh_createVcapVdis()
{
	printf("+++++Jason: Changed_MultiCh_createVcapVdis is begining\n+++++");

	Bool enableAlgLink = TRUE;

	CaptureLink_CreateParams    capturePrm;
	NsfLink_CreateParams        nsfPrm;
	IpcFramesOutLinkRTOS_CreateParams ipcFramesOutVpssPrm;
	IpcFramesInLinkRTOS_CreateParams  ipcFramesInDspPrm;
	AlgLink_CreateParams              dspAlgPrm;
	static SwMsLink_CreateParams       swMsPrm;
    DisplayLink_CreateParams    displayPrm;

	CaptureLink_VipInstParams *pCaptureInstPrm;
    CaptureLink_OutParams     *pCaptureOutPrm;

	VCAP_VIDDEC_PARAMS_S vidDecVideoModeArgs[NUM_CAPTURE_DEVICES];

    UInt32 vipInstId;
	UInt32 i;
    UInt32 numSubChains;

	MULTICH_INIT_STRUCT(IpcFramesOutLinkRTOS_CreateParams,ipcFramesOutVpssPrm);
	MULTICH_INIT_STRUCT(IpcFramesInLinkRTOS_CreateParams,ipcFramesInDspPrm);
	MULTICH_INIT_STRUCT(AlgLink_CreateParams, dspAlgPrm);
    MULTICH_INIT_STRUCT(DisplayLink_CreateParams,displayPrm);
	MULTICH_INIT_STRUCT(SwMsLink_CreateParams ,swMsPrm);

    System_init();

    MultiCh_detectBoard();

    System_linkControl(
        SYSTEM_LINK_ID_M3VPSS,
        SYSTEM_M3VPSS_CMD_RESET_VIDEO_DEVICES,
        NULL,
        0,
        TRUE
        );

	gVcapModuleContext.captureId    = SYSTEM_LINK_ID_CAPTURE;
    gVcapModuleContext.nsfId[0]     = SYSTEM_LINK_ID_NSF_0;
	if(enableAlgLink)
    {
		gVcapModuleContext.ipcFramesOutVpssId[0] = SYSTEM_VPSS_LINK_ID_IPC_FRAMES_OUT_0;
        gVcapModuleContext.ipcFramesInDspId[0] = SYSTEM_DSP_LINK_ID_IPC_FRAMES_IN_0;
        gVcapModuleContext.dspAlgId[0] = SYSTEM_LINK_ID_ALG_0;
    }
	gVdisModuleContext.swMsId[0]      = SYSTEM_LINK_ID_SW_MS_MULTI_INST_0;
    swMsPrm.numSwMsInst   = 1;
    swMsPrm.swMsInstId[0] = SYSTEM_SW_MS_SC_INST_SC5;
    gVdisModuleContext.displayId[0] = SYSTEM_LINK_ID_DISPLAY_0; // ON CHIP HDMI

	numSubChains             = 2;

	CaptureLink_CreateParams_Init(&capturePrm);
    capturePrm.numVipInst    = 2 * numSubChains;
    capturePrm.outQueParams[0].nextLink = gVcapModuleContext.nsfId[0];
    capturePrm.tilerEnable              = FALSE;
    capturePrm.enableSdCrop             = FALSE;
	for(vipInstId=0; vipInstId<capturePrm.numVipInst; vipInstId++)
    {
        pCaptureInstPrm                     = &capturePrm.vipInst[vipInstId];
        pCaptureInstPrm->vipInstId          = (SYSTEM_CAPTURE_INST_VIP1_PORTB-
                                              vipInstId)%SYSTEM_CAPTURE_INST_MAX;//(SYSTEM_CAPTURE_INST_VIP0_PORTA+vipInstId)%SYSTEM_CAPTURE_INST_MAX;
        pCaptureInstPrm->videoDecoderId     = SYSTEM_DEVICE_VID_DEC_TVP5158_DRV;
        pCaptureInstPrm->inDataFormat       = SYSTEM_DF_YUV422P;
        pCaptureInstPrm->standard           = SYSTEM_STD_MUX_4CH_D1;
        pCaptureInstPrm->numOutput          = 1;

        pCaptureOutPrm                      = &pCaptureInstPrm->outParams[0];
        pCaptureOutPrm->dataFormat          = SYSTEM_DF_YUV422I_YUYV;
        pCaptureOutPrm->scEnable            = FALSE;
        pCaptureOutPrm->scOutWidth          = 0;
        pCaptureOutPrm->scOutHeight         = 0;
        pCaptureOutPrm->outQueId            = 0;
    }

	for(i = 0; i < NUM_CAPTURE_DEVICES; i++)
    {
        vidDecVideoModeArgs[i].videoIfMode        = DEVICE_CAPT_VIDEO_IF_MODE_8BIT;
        vidDecVideoModeArgs[i].videoDataFormat    = SYSTEM_DF_YUV422P;
        vidDecVideoModeArgs[i].standard           = SYSTEM_STD_MUX_4CH_D1;
        vidDecVideoModeArgs[i].videoCaptureMode   =
                    DEVICE_CAPT_VIDEO_CAPTURE_MODE_MULTI_CH_PIXEL_MUX_EMBEDDED_SYNC;
        vidDecVideoModeArgs[i].videoSystem        =
                                      DEVICE_VIDEO_DECODER_VIDEO_SYSTEM_AUTO_DETECT;
        vidDecVideoModeArgs[i].videoCropEnable    = FALSE;
        vidDecVideoModeArgs[i].videoAutoDetectTimeout = -1;
    }

    Vcap_configVideoDecoder(vidDecVideoModeArgs, NUM_CAPTURE_DEVICES);

	NsfLink_CreateParams_Init(&nsfPrm);
    nsfPrm.bypassNsf                 = FALSE;
    nsfPrm.tilerEnable               = FALSE;
    nsfPrm.inQueParams.prevLinkId    = gVcapModuleContext.captureId;
    nsfPrm.inQueParams.prevLinkQueId = 0;
    nsfPrm.numOutQue                 = 1;
    nsfPrm.outQueParams[0].nextLink  = gVdisModuleContext.swMsId[0];

	if(enableAlgLink)
	{
		nsfPrm.outQueParams[0].nextLink  = gVcapModuleContext.ipcFramesOutVpssId[0];

		ipcFramesOutVpssPrm .baseCreateParams.inQueParams.prevLinkId   = gVcapModuleContext.nsfId[0];
        ipcFramesOutVpssPrm.baseCreateParams.inQueParams.prevLinkQueId = 0;
        ipcFramesOutVpssPrm.baseCreateParams.outQueParams[0].nextLink  = gVdisModuleContext.swMsId[0];
        ipcFramesOutVpssPrm.baseCreateParams.processLink               = gVcapModuleContext.ipcFramesInDspId[0];
        ipcFramesOutVpssPrm.baseCreateParams.notifyPrevLink            = TRUE;
        ipcFramesOutVpssPrm.baseCreateParams.notifyNextLink            = TRUE;
        ipcFramesOutVpssPrm.baseCreateParams.notifyProcessLink         = TRUE;
        ipcFramesOutVpssPrm.baseCreateParams.noNotifyMode              = FALSE;
        ipcFramesOutVpssPrm.baseCreateParams.numOutQue                 = 1;

        ipcFramesInDspPrm.baseCreateParams.inQueParams.prevLinkId      = gVcapModuleContext.ipcFramesOutVpssId[0];
        ipcFramesInDspPrm.baseCreateParams.inQueParams.prevLinkQueId   = 0;
        ipcFramesInDspPrm.baseCreateParams.outQueParams[0].nextLink    = gVcapModuleContext.dspAlgId[0];
        ipcFramesInDspPrm.baseCreateParams.notifyPrevLink              = TRUE;
        ipcFramesInDspPrm.baseCreateParams.notifyNextLink              = TRUE;
        ipcFramesInDspPrm.baseCreateParams.noNotifyMode                = FALSE;
        ipcFramesInDspPrm.baseCreateParams.numOutQue                   = 1;

        dspAlgPrm.inQueParams.prevLinkId = gVcapModuleContext.ipcFramesInDspId[0];
        dspAlgPrm.inQueParams.prevLinkQueId = 0;
	}

	swMsPrm.inQueParams.prevLinkId = gVcapModuleContext.nsfId[0];
	if(enableAlgLink)
	{
		swMsPrm.inQueParams.prevLinkId = gVcapModuleContext.ipcFramesOutVpssId[0];
	}
	swMsPrm.inQueParams.prevLinkQueId = 0;
	swMsPrm.outQueParams.nextLink     = gVdisModuleContext.displayId[0];
	swMsPrm.maxInputQueLen            = SYSTEM_SW_MS_DEFAULT_INPUT_QUE_LEN;
	swMsPrm.maxOutRes                 = gVdisModuleContext.vdisConfig.deviceParams[0].resolution;
	swMsPrm.lineSkipMode = TRUE;
	swMsPrm.enableLayoutGridDraw = gVdisModuleContext.vdisConfig.enableLayoutGridDraw;
	MultiCh_swMsGetDefaultLayoutPrm(0, &swMsPrm, FALSE); /* Since only live preview is there, show it on both displays */

	displayPrm.inQueParams[0].prevLinkId    	= gVdisModuleContext.swMsId[0];
	displayPrm.inQueParams[0].prevLinkQueId 	= 0;
	displayPrm.displayRes                	= swMsPrm.maxOutRes;

	MultiCh_displayCtrlInit(&gVdisModuleContext.vdisConfig);
#ifndef SYSTEM_USE_VIDEO_DECODER
    capturePrm.isPalMode = Vcap_isPalMode();
#endif

	System_linkCreate (gVcapModuleContext.captureId, &capturePrm, sizeof(capturePrm));
    System_linkCreate(gVcapModuleContext.nsfId[0] , &nsfPrm, sizeof(nsfPrm));
	if(enableAlgLink)
    {
        System_linkCreate(gVcapModuleContext.ipcFramesOutVpssId[0], &ipcFramesOutVpssPrm, sizeof(ipcFramesOutVpssPrm));
        System_linkCreate(gVcapModuleContext.ipcFramesInDspId[0], &ipcFramesInDspPrm, sizeof(ipcFramesInDspPrm));
        System_linkCreate(gVcapModuleContext.dspAlgId[0], &dspAlgPrm, sizeof(dspAlgPrm));
    }
	System_linkCreate(gVdisModuleContext.swMsId[0]  , &swMsPrm, sizeof(swMsPrm));
    System_linkCreate(gVdisModuleContext.displayId[0], &displayPrm, sizeof(displayPrm));

    MultiCh_memPrintHeapStatus();
}


Void MultiCh_deleteVcapVdis()
{
	Bool enableAlgLink = TRUE;

    gVcapModuleContext.captureId    = SYSTEM_LINK_ID_CAPTURE;
    gVcapModuleContext.nsfId[0]     = SYSTEM_LINK_ID_NSF_0;
	if(enableAlgLink)
    {
		gVcapModuleContext.ipcFramesOutVpssId[0] = SYSTEM_VPSS_LINK_ID_IPC_FRAMES_OUT_0;
        gVcapModuleContext.ipcFramesInDspId[0] = SYSTEM_DSP_LINK_ID_IPC_FRAMES_IN_0;
        gVcapModuleContext.dspAlgId[0] = SYSTEM_LINK_ID_ALG_0;
    }
	gVdisModuleContext.swMsId[0]      = SYSTEM_LINK_ID_SW_MS_MULTI_INST_0;
    gVdisModuleContext.displayId[0] = SYSTEM_LINK_ID_DISPLAY_0; // ON CHIP HDMI

    MultiCh_displayCtrlDeInit(&gVdisModuleContext.vdisConfig);

    System_linkDelete(gVcapModuleContext.captureId);
    System_linkDelete(gVcapModuleContext.nsfId[0]);
	if(enableAlgLink)
    {
        System_linkDelete(gVcapModuleContext.ipcFramesOutVpssId[0]);
        System_linkDelete(gVcapModuleContext.ipcFramesInDspId[0]);
        System_linkDelete(gVcapModuleContext.dspAlgId[0]);
    }
	System_linkDelete(gVdisModuleContext.swMsId[0] );
	System_linkDelete(gVdisModuleContext.displayId[0]);

    /* Print the HWI, SWI and all tasks load */
    /* Reset the accumulated timer ticks */
    MultiCh_prfLoadCalcEnable(FALSE, TRUE, FALSE);

    System_deInit();
}
