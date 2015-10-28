/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2009 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

#include "multich_common.h"

/*
                                                                                        <-+
                        Capture (YUV422I)                                                 |
                          |          |                                                    |
          1 or 2 CH D1    |          |  4Ch D1                                            |
         60fps YUV422I    |          |  60fps YUV422I                                     | CAPTURE Subsystem
                          |          |                                                    |
            IPC_FRAMES_OUT_0 --------]------------------------- IPC_FRAMES_IN_0 -- OSD0   |
                          |          |                                                    |
                          |        IPC_FRAMES_OUT_1 ----------- IPC_FRAME_IN_1 -- OSD1    |
                          |          |                                                    |
                        DEI         DEI                                                   |
               (VIP-SC YUV420)      (VIP-SC YUV420)                                       |
                          |           |                                                   |
                         DUP         DUP                                                  |
                         | |         | |                                                  |
                         | |         | |                                                  |
                         | |         | SW MS (SC5 - YUV422I out)                          |
                         | |         | |                                                  |
                         | |         | NSF (YUV420 out)                                   |
                         | |         | |                                                <-+
                         | |         | |                           DECODE Subsystem
                         | |         | |             +----------------------------------------------------------------+
            +------------+ |         +-]-------+     |                                                                |
            |              |           |       |     V                                                                V
            |              +-----------]-----+ | +----- IPC_M3_IN -- IPC_M3_OUT -- DEC -- IPC_BITS_IN -- IPC_BITS_OUT
            |                          |     | | |
            |   +----------------------+     | | |
            |   |                            | | |
            MERGE                            MERGE
              |        <-+                     |            <-+
          IPC_M3_OUT     |                   SWMS(SC5)        |
              |          |                     |              |  DISPLAY Subsystem
          IPC_M3_IN      | ENCODE            DISPLAY          |
              |          | Subsystem         (SDTV)           |
             ENC         |                                  <-+
              |          |
          IPC_BITS_OUT   |
              |          |
          IPC_BITS_IN    |
                       <-+
*/

#define VCAP_OUT0_DUP_LINK_IDX (0)
#define VCAP_OUT1_DUP_LINK_IDX (1)
#define NUM_DUP_LINK           (2)

#define VENC_MERGE_LINK_IDX    (0)
#define VDIS_MERGE_LINK_IDX    (1)
#define NUM_MERGE_LINK         (2)

#define MAX_DEC_OUT_FRAMES_PER_CH   (5)

#define     NUM_CAPTURE_DEVICES          1


typedef struct {

    /* capture subsystem link ID's - other link ID's are in gVcapModuleContext */
    UInt32 dupId[NUM_DUP_LINK];
    UInt32 mergeId[NUM_MERGE_LINK];

    /* encode, deccode, display subsystem link ID's are in gVxxxModuleContext */

} MultiCh_CustomVcapVencVdecVdisObj;

MultiCh_CustomVcapVencVdecVdisObj gMultiCh_customVcapVencVdecVdisObj;


static SystemVideo_Ivahd2ChMap_Tbl systemVid_encDecIvaChMapTbl =
{
    .isPopulated = 1,
    .ivaMap[0] =
    {
        .EncNumCh  = 8,
        .EncChList = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0 , 0, 0},

        .DecNumCh  = 8,
        .DecChList = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0 , 0, 0},
    },
    .ivaMap[1] =  {  .EncNumCh  = 0,  .DecNumCh  = 0,  },
    .ivaMap[2] =  {  .EncNumCh  = 0,  .DecNumCh  = 0,  },
};


Int32 MultiCh_createCustomVcap(
            System_LinkInQueParams *vdecOutQue,
            System_LinkInQueParams *vencInQue,
            System_LinkInQueParams *vdisInQue,
            UInt32 numVipInst,
            UInt32 tilerEnable
        );

Int32 MultiCh_createCustomVdis(System_LinkInQueParams *vdisInQue);



/*
    Create use-case

    Separate create function for each subsystem.
    Create gets called only if the subsystem is enabled.
*/
Void MultiCh_createCustomVcapVencVdecVdis()
{
    Int32 i;
    Bool tilerEnable;
    UInt32 vdecNextLinkId;
    UInt32 numVipInst;

    System_LinkInQueParams   vdecOutQue;
    System_LinkInQueParams   vencInQue;
    System_LinkInQueParams   vdisInQue;


    /* Init McFW system */
    System_init();

    tilerEnable = FALSE;

    /* init gMultiCh_customVcapVencVdecVdisObj to known default state */
    for(i=0; i<NUM_MERGE_LINK; i++)
    {
        gMultiCh_customVcapVencVdecVdisObj.mergeId[i] = SYSTEM_LINK_ID_INVALID;
    }

    for(i=0; i<NUM_DUP_LINK; i++)
    {
        gMultiCh_customVcapVencVdecVdisObj.dupId[i] = SYSTEM_LINK_ID_INVALID;
    }

    gVcapModuleContext.capSwMsId = SYSTEM_LINK_ID_INVALID;

    gMultiCh_customVcapVencVdecVdisObj.mergeId[VDIS_MERGE_LINK_IDX]  = SYSTEM_VPSS_LINK_ID_MERGE_1;

    System_LinkInQueParams_Init(&vdecOutQue);
    System_LinkInQueParams_Init(&vencInQue);
    System_LinkInQueParams_Init(&vdisInQue);

    /* setup enc/dec channel map */
    System_linkControl(
        SYSTEM_LINK_ID_M3VIDEO,
        SYSTEM_COMMON_CMD_SET_CH2IVAHD_MAP_TBL,
        &systemVid_encDecIvaChMapTbl,
        sizeof(SystemVideo_Ivahd2ChMap_Tbl),
        TRUE
    );


    if(gVsysModuleContext.vsysConfig.enableDecode)
    {
        /* create decode subsystem */

        if(gVsysModuleContext.vsysConfig.enableCapture)
            vdecNextLinkId = gMultiCh_customVcapVencVdecVdisObj.mergeId[VDIS_MERGE_LINK_IDX];
        else
            vdecNextLinkId = SYSTEM_LINK_ID_SW_MS_MULTI_INST_0;

        Vdec_create(
            &vdecOutQue, /* output by this function */
            vdecNextLinkId,
            tilerEnable,
            MAX_DEC_OUT_FRAMES_PER_CH
            );
    }
    else
    {
        /* if decode is not enabled then capture MUST be enabled */
        gVsysModuleContext.vsysConfig.enableCapture = TRUE;
    }

    if(gVsysModuleContext.vsysConfig.enableCapture)
    {
        if(gVsysModuleContext.vsysConfig.enableSecondaryOut)
            numVipInst = 2;
        else
            numVipInst = 1;

        /* create capture subsystem */
        MultiCh_createCustomVcap(
            &vdecOutQue, /* input to this function */
            &vencInQue,  /* output by this function */
            &vdisInQue,  /* output by this function */
            numVipInst,
            FALSE
            );
    }
    else
    {
        /* if capture is not enabled then display input = decode output */
        vdisInQue = vdecOutQue;
    }

    /* create display subsystem based on output of capture and decode subsystem */
    MultiCh_createCustomVdis(
        &vdisInQue  /* input to this function */
        );

    /* if capture is enabled then encode is also enabled */
    if(gVsysModuleContext.vsysConfig.enableCapture)
    {
        /* create encode subsystem based on output of capture subsystem */
        Venc_create(
            &vencInQue  /* input to this function */
            );
    }

    /* print memory consumed status */
    MultiCh_memPrintHeapStatus();
}

Int32 MultiCh_createCustomVcap(
            System_LinkInQueParams *vdecOutQue,
            System_LinkInQueParams *vencInQue,
            System_LinkInQueParams *vdisInQue,
            UInt32 numVipInst,
            UInt32 tilerEnable
        )
{
    CaptureLink_CreateParams    capturePrm;
    DeiLink_CreateParams        deiPrm[NUM_DUP_LINK];
    DupLink_CreateParams        dupPrm[NUM_DUP_LINK];
    static SwMsLink_CreateParams       swMsPrm;
    NsfLink_CreateParams        nsfPrm;
    MergeLink_CreateParams      mergePrm[NUM_MERGE_LINK];

    CaptureLink_VipInstParams  *pCaptureInstPrm;
    CaptureLink_OutParams      *pCaptureOutPrm;

    VCAP_VIDDEC_PARAMS_S vidDecVideoModeArgs[NUM_CAPTURE_DEVICES];
    UInt32 i, vipInstId;

    gVcapModuleContext.captureId                                     = SYSTEM_LINK_ID_CAPTURE;
    gVcapModuleContext.deiId[0]                                      = SYSTEM_LINK_ID_DEI_0; // use the same DEI
    gVcapModuleContext.deiId[1]                                      = SYSTEM_LINK_ID_DEI_1; // use the same DEI

    gMultiCh_customVcapVencVdecVdisObj.dupId[VCAP_OUT0_DUP_LINK_IDX] = SYSTEM_VPSS_LINK_ID_DUP_0;
    gMultiCh_customVcapVencVdecVdisObj.dupId[VCAP_OUT1_DUP_LINK_IDX] = SYSTEM_VPSS_LINK_ID_DUP_1;

    gVcapModuleContext.nsfId[0]                                      = SYSTEM_LINK_ID_NSF_0;
    gVcapModuleContext.capSwMsId                     = SYSTEM_LINK_ID_SW_MS_MULTI_INST_1;

    gMultiCh_customVcapVencVdecVdisObj.mergeId[VENC_MERGE_LINK_IDX]  = SYSTEM_VPSS_LINK_ID_MERGE_0;

    vencInQue->prevLinkId    = gMultiCh_customVcapVencVdecVdisObj.mergeId[VENC_MERGE_LINK_IDX];
    vencInQue->prevLinkQueId = 0;

    vdisInQue->prevLinkId    = gMultiCh_customVcapVencVdecVdisObj.mergeId[VDIS_MERGE_LINK_IDX];
    vdisInQue->prevLinkQueId = 0;

    CaptureLink_CreateParams_Init(&capturePrm);

    if(numVipInst>2)
        numVipInst = 2;

    capturePrm.numVipInst    = numVipInst;
    capturePrm.outQueParams[0].nextLink = gVcapModuleContext.deiId[0];
    capturePrm.outQueParams[1].nextLink = gVcapModuleContext.deiId[1];

    capturePrm.tilerEnable              = FALSE;
    capturePrm.enableSdCrop             = FALSE;
    capturePrm.numBufsPerCh   = 8;

    for(vipInstId=0; vipInstId<capturePrm.numVipInst; vipInstId++)
    {
        DeiLink_CreateParams_Init(&deiPrm[vipInstId]);

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
        pCaptureOutPrm->outQueId            = vipInstId;

        deiPrm[vipInstId].inQueParams.prevLinkId                        = gVcapModuleContext.captureId;
        deiPrm[vipInstId].inQueParams.prevLinkQueId                     = vipInstId;
        deiPrm[vipInstId].outQueParams[DEI_LINK_OUT_QUE_VIP_SC].nextLink= gMultiCh_customVcapVencVdecVdisObj.dupId[vipInstId];
        deiPrm[vipInstId].enableOut[DEI_LINK_OUT_QUE_VIP_SC]            = TRUE;
        deiPrm[vipInstId].tilerEnable                                   = tilerEnable;
        deiPrm[vipInstId].inputFrameRate[DEI_LINK_OUT_QUE_VIP_SC]       = 60;
        deiPrm[vipInstId].outputFrameRate[DEI_LINK_OUT_QUE_VIP_SC]      = 30;
        deiPrm[vipInstId].enableOut[DEI_LINK_OUT_QUE_DEI_SC]                = FALSE;
        deiPrm[vipInstId].enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT]  = FALSE;

        /* rest all DEI params are default */

        dupPrm[vipInstId].inQueParams.prevLinkId         = gVcapModuleContext.deiId[vipInstId];
        dupPrm[vipInstId].inQueParams.prevLinkQueId      = DEI_LINK_OUT_QUE_VIP_SC;
        dupPrm[vipInstId].numOutQue                      = 2;
        if(vipInstId==0)
            dupPrm[vipInstId].outQueParams[0].nextLink       = gMultiCh_customVcapVencVdecVdisObj.mergeId[VENC_MERGE_LINK_IDX];
        else
            dupPrm[vipInstId].outQueParams[0].nextLink       = gVcapModuleContext.capSwMsId;
        dupPrm[vipInstId].outQueParams[1].nextLink       = gMultiCh_customVcapVencVdecVdisObj.mergeId[VDIS_MERGE_LINK_IDX];
        dupPrm[vipInstId].notifyNextLink                 = TRUE;
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

    Vcap_configVideoDecoder(vidDecVideoModeArgs, numVipInst);

    SwMsLink_CreateParams_Init(&swMsPrm);

    swMsPrm.numSwMsInst          = 1;
    swMsPrm.swMsInstId[0]        = SYSTEM_SW_MS_SC_INST_SC5;
    swMsPrm.swMsInstStartWin[0]  = 0;

    swMsPrm.inQueParams.prevLinkId     = gMultiCh_customVcapVencVdecVdisObj.dupId[1];
    swMsPrm.inQueParams.prevLinkQueId  = 0;
    swMsPrm.outQueParams.nextLink      = gVcapModuleContext.nsfId[0];

    swMsPrm.maxOutRes              = VSYS_STD_PAL;
    swMsPrm.lineSkipMode           = FALSE;
    swMsPrm.maxInputQueLen         = SYSTEM_SW_MS_DEFAULT_INPUT_QUE_LEN;

    swMsPrm.enableLayoutGridDraw = gVdisModuleContext.vdisConfig.enableLayoutGridDraw;

    MultiCh_swMsGetDefaultLayoutPrm(SYSTEM_DC_VENC_SD, &swMsPrm, FALSE);

    NsfLink_CreateParams_Init(&nsfPrm);
    nsfPrm.bypassNsf                 = TRUE;
    nsfPrm.tilerEnable               = tilerEnable;
    nsfPrm.inQueParams.prevLinkId    = gVcapModuleContext.capSwMsId;
    nsfPrm.inQueParams.prevLinkQueId = 0;
    nsfPrm.numOutQue                 = 1;
    nsfPrm.outQueParams[0].nextLink  = gMultiCh_customVcapVencVdecVdisObj.mergeId[VENC_MERGE_LINK_IDX];

    mergePrm[VENC_MERGE_LINK_IDX].numInQue                     = capturePrm.numVipInst;
    mergePrm[VENC_MERGE_LINK_IDX].inQueParams[0].prevLinkId    = gMultiCh_customVcapVencVdecVdisObj.dupId[0];
    mergePrm[VENC_MERGE_LINK_IDX].inQueParams[0].prevLinkQueId = 0;
    mergePrm[VENC_MERGE_LINK_IDX].inQueParams[1].prevLinkId    = gVcapModuleContext.nsfId[0];
    mergePrm[VENC_MERGE_LINK_IDX].inQueParams[1].prevLinkQueId = 0;
    mergePrm[VENC_MERGE_LINK_IDX].outQueParams.nextLink        = SYSTEM_VPSS_LINK_ID_IPC_OUT_M3_0;
    mergePrm[VENC_MERGE_LINK_IDX].notifyNextLink               = TRUE;

    mergePrm[VDIS_MERGE_LINK_IDX].numInQue                     = capturePrm.numVipInst;

    if(gVsysModuleContext.vsysConfig.enableDecode)
        mergePrm[VDIS_MERGE_LINK_IDX].numInQue++;   /* + 1 for decode CHs */

    for(vipInstId=0; vipInstId<capturePrm.numVipInst; vipInstId++)
    {
        mergePrm[VDIS_MERGE_LINK_IDX].inQueParams[vipInstId].prevLinkId    = gMultiCh_customVcapVencVdecVdisObj.dupId[vipInstId];
        mergePrm[VDIS_MERGE_LINK_IDX].inQueParams[vipInstId].prevLinkQueId = 1;
    }
    mergePrm[VDIS_MERGE_LINK_IDX].inQueParams[vipInstId].prevLinkId    = vdecOutQue->prevLinkId;
    mergePrm[VDIS_MERGE_LINK_IDX].inQueParams[vipInstId].prevLinkQueId = vdecOutQue->prevLinkQueId;
    mergePrm[VDIS_MERGE_LINK_IDX].outQueParams.nextLink        = SYSTEM_LINK_ID_SW_MS_MULTI_INST_0;
    mergePrm[VDIS_MERGE_LINK_IDX].notifyNextLink               = TRUE;

#ifndef SYSTEM_USE_VIDEO_DECODER
    capturePrm.isPalMode = Vcap_isPalMode();
#endif
    System_linkCreate (gVcapModuleContext.captureId, &capturePrm, sizeof(capturePrm));

    for(vipInstId=0; vipInstId<capturePrm.numVipInst; vipInstId++)
    {
        System_linkCreate(
            gVcapModuleContext.deiId[vipInstId],
            &deiPrm[vipInstId],
            sizeof(deiPrm[vipInstId])
            );

        System_linkCreate(
            gMultiCh_customVcapVencVdecVdisObj.dupId[vipInstId],
            &dupPrm[vipInstId],
            sizeof(dupPrm[vipInstId])
            );
    }

    if(capturePrm.numVipInst>1)
    {
        System_linkCreate(gVcapModuleContext.capSwMsId , &swMsPrm, sizeof(swMsPrm));
        System_linkCreate(gVcapModuleContext.nsfId[0] , &nsfPrm, sizeof(nsfPrm));
    }

    for(i=0; i<NUM_MERGE_LINK; i++)
    {
        System_linkCreate(
            gMultiCh_customVcapVencVdecVdisObj.mergeId[i],
            &mergePrm[i],
            sizeof(mergePrm[i])
          );
    }

    /* disable channels that are not required */
    {
        DeiLink_ChannelInfo deiChDisable;

        deiChDisable.channelId  = 2;
        deiChDisable.streamId   = DEI_LINK_OUT_QUE_VIP_SC;
        deiChDisable.enable     = FALSE;

        System_linkControl(
            gVcapModuleContext.deiId[0],
            DEI_LINK_CMD_DISABLE_CHANNEL,
            &deiChDisable,
            sizeof(deiChDisable),
            TRUE
            );

        deiChDisable.channelId  = 3;

        System_linkControl(
            gVcapModuleContext.deiId[0],
            DEI_LINK_CMD_DISABLE_CHANNEL,
            &deiChDisable,
            sizeof(deiChDisable),
            TRUE
            );

        if(vipInstId>1)
        {
            deiChDisable.channelId  = 1;

            System_linkControl(
                gVcapModuleContext.deiId[0],
                DEI_LINK_CMD_DISABLE_CHANNEL,
                &deiChDisable,
                sizeof(deiChDisable),
                TRUE
            );
        }
    }

    return 0;
}

Int32 MultiCh_createCustomVdis(System_LinkInQueParams *vdisInQue)
{

    static SwMsLink_CreateParams       swMsPrm;
    DisplayLink_CreateParams    displayPrm;

    UInt32 displayIdx;

    MultiCh_displayCtrlInit(&gVdisModuleContext.vdisConfig);


#ifdef TI_814X_BUILD
    displayIdx = 1; // SDTV on Centaurus
#else
    displayIdx = 2; // SDTV on Netra
#endif

    gVdisModuleContext.swMsId[displayIdx]    = SYSTEM_LINK_ID_SW_MS_MULTI_INST_0;
    gVdisModuleContext.displayId[displayIdx] = SYSTEM_LINK_ID_DISPLAY_2; // SDTV

    MULTICH_INIT_STRUCT(SwMsLink_CreateParams ,swMsPrm);

    swMsPrm.numSwMsInst          = 1;
    swMsPrm.swMsInstId[0]        = SYSTEM_SW_MS_SC_INST_SC5;
    swMsPrm.swMsInstStartWin[0]  = 0;

    swMsPrm.inQueParams.prevLinkId     = vdisInQue->prevLinkId;
    swMsPrm.inQueParams.prevLinkQueId  = vdisInQue->prevLinkQueId;
    swMsPrm.outQueParams.nextLink      = gVdisModuleContext.displayId[displayIdx];

    swMsPrm.maxOutRes              = VSYS_STD_PAL;
    swMsPrm.lineSkipMode           = FALSE;
    swMsPrm.maxInputQueLen         = SYSTEM_SW_MS_INVALID_INPUT_QUE_LEN;

    swMsPrm.enableLayoutGridDraw = gVdisModuleContext.vdisConfig.enableLayoutGridDraw;

    MultiCh_swMsGetDefaultLayoutPrm(SYSTEM_DC_VENC_SD, &swMsPrm, FALSE);
    MULTICH_INIT_STRUCT(DisplayLink_CreateParams ,displayPrm);
    displayPrm.inQueParams[0].prevLinkId    = gVdisModuleContext.swMsId[displayIdx];
    displayPrm.inQueParams[0].prevLinkQueId = 0;
    displayPrm.displayRes  = gVdisModuleContext.vdisConfig.deviceParams[VDIS_DEV_SD].resolution;


    System_linkCreate(gVdisModuleContext.swMsId[displayIdx]   , &swMsPrm   , sizeof(swMsPrm)   );
    System_linkCreate(gVdisModuleContext.displayId[displayIdx], &displayPrm, sizeof(displayPrm));

    return 0;
}

/*
    Delete use-case

    No separate delete function for each subsystem, since the overall
    delete sequence is very short

    While deleting, the McFW APIs knows whats created.
    So delete is called for each sub-system.
    If the sub-system was not created to being with then the
    delete will have no effect.
*/
Void MultiCh_deleteCustomVcapVencVdecVdis()
{
    Int32 i;

    /* stop display Venc */
    MultiCh_displayCtrlDeInit(&gVdisModuleContext.vdisConfig);

    /* delete individual subsystem  */
    Vcap_delete();
    Vdec_delete();
    Venc_delete();
    Vdis_delete();

    /* delete additional use-case specific link's   */
    for(i=0; i<NUM_MERGE_LINK; i++)
    {
        if(gMultiCh_customVcapVencVdecVdisObj.mergeId[i]
                !=
            SYSTEM_LINK_ID_INVALID
            )
        {
            System_linkDelete(gMultiCh_customVcapVencVdecVdisObj.mergeId[i]);
        }
    }

    for(i=0; i<NUM_DUP_LINK; i++)
    {
        if(gMultiCh_customVcapVencVdecVdisObj.dupId[i]
                !=
            SYSTEM_LINK_ID_INVALID
            )
        {
            System_linkDelete(gMultiCh_customVcapVencVdecVdisObj.dupId[i]);
        }
    }

    /* Print slave processor load info */
    MultiCh_prfLoadCalcEnable(FALSE, TRUE, FALSE);

    /* De-init McFW system */
    System_deInit();
}

