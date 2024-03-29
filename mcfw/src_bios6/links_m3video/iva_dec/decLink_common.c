/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2009 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

#include <stdlib.h>
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Task.h>
#include <mcfw/src_bios6/utils/utils.h>
#include <mcfw/src_bios6/utils/utils_mem.h>
#include <mcfw/src_bios6/links_m3video/codec_utils/utils_encdec.h>
#include <mcfw/src_bios6/links_m3video/codec_utils/iresman_hdvicp2_earlyacquire.h>
#include <mcfw/interfaces/link_api/system_tiler.h>
#include "decLink_priv.h"

#define DEC_LINK_HDVICP2_EARLY_ACQUIRE_ENABLE

static Int32 DecLink_codecCreateReqObj(DecLink_Obj * pObj);
static Int32 DecLink_codecCreateOutObj(DecLink_Obj * pObj);
static Int32 DecLink_codecCreateChObj(DecLink_Obj * pObj, UInt32 chId);
static Int32 DecLink_codecCreateDecObj(DecLink_Obj * pObj, UInt32 chId);
static Void DecLink_codecProcessTskFxn(UArg arg1, UArg arg2);
static Int32 DecLink_codecCreateProcessTsk(DecLink_Obj * pObj, UInt32 tskId);
static Int32 DecLink_codecDeleteProcessTsk(DecLink_Obj * pObj, UInt32 tskId);
static Int32 DecLink_codecQueueBufsToChQue(DecLink_Obj * pObj);
static Int32 DecLink_codecSubmitData(DecLink_Obj * pObj);
static Int32 DecLink_codecGetProcessedData(DecLink_Obj * pObj);
static Int32 DecLink_dupFrame(DecLink_Obj * pObj, FVID2_Frame * pOrgFrame,
                              FVID2_Frame ** ppDupFrame);
static Int32 DecLink_PrepareBatch (DecLink_Obj *pObj, UInt32 tskId, 
                                   DecLink_ReqObj *pReqObj, 
                                   DecLink_ReqBatch *pReqObjBatch);

DecLink_ChCreateParams DECLINK_DEFAULTCHCREATEPARAMS_H264 = {
    .format = IVIDEO_H264HP,
    .profile = IH264VDEC_PROFILE_ANY,
    .fieldMergeDecodeEnable = FALSE,
    .targetMaxWidth = UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH,
    .targetMaxHeight = UTILS_ENCDEC_RESOLUTION_CLASS_D1_HEIGHT,
    .displayDelay = DEC_LINK_DEFAULT_ALGPARAMS_DISPLAYDELAY,
    .processCallLevel = IH264VDEC_FIELDLEVELPROCESSCALL,
    .numBufPerCh = DEC_LINK_NUM_BUFFERS_PER_CHANNEL,   
    .dpbBufSizeInFrames = DEC_LINK_DPB_SIZE_IN_FRAMES_DEFAULT,
    .defaultDynamicParams = {
                             .targetFrameRate =
                             DEC_LINK_DEFAULT_ALGPARAMS_TARGETFRAMERATE,
                             .targetBitRate =
                             DEC_LINK_DEFAULT_ALGPARAMS_TARGETBITRATE,
                             }
};

DecLink_ChCreateParams DECLINK_DEFAULTCHCREATEPARAMS_MPEG4 = {
    .format = IVIDEO_MPEG4ASP,
    .profile = NULL,
    .fieldMergeDecodeEnable = TRUE,
    .targetMaxWidth = UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH,
    .targetMaxHeight = UTILS_ENCDEC_RESOLUTION_CLASS_D1_HEIGHT,
    .displayDelay = DEC_LINK_DEFAULT_ALGPARAMS_DISPLAYDELAY,
    .numBufPerCh = DEC_LINK_NUM_BUFFERS_PER_CHANNEL,
    .defaultDynamicParams = {
                             .targetFrameRate = NULL,
                             .targetBitRate = NULL,
                             }
};

DecLink_ChCreateParams DECLINK_DEFAULTCHCREATEPARAMS_JPEG = {
    .format = IVIDEO_MJPEG,
    .profile = NULL,
    .fieldMergeDecodeEnable = TRUE,
    .targetMaxWidth = UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH,
    .targetMaxHeight = UTILS_ENCDEC_RESOLUTION_CLASS_D1_HEIGHT,
    .displayDelay = DEC_LINK_DEFAULT_ALGPARAMS_DISPLAYDELAY,
    .numBufPerCh = DEC_LINK_NUM_BUFFERS_PER_CHANNEL,    
    .dpbBufSizeInFrames = DEC_LINK_DPB_SIZE_IN_FRAMES_DEFAULT,
    .defaultDynamicParams = {
                             .targetFrameRate = NULL,
                             .targetBitRate = NULL,
                             }
};


static Int32 DecLink_codecCreateReqObj(DecLink_Obj * pObj)
{
    Int32 status;
    UInt32 reqId;

    memset(pObj->reqObj, 0, sizeof(pObj->reqObj));

    status = Utils_queCreate(&pObj->reqQue,
                             DEC_LINK_MAX_REQ,
                             pObj->reqQueMem, UTILS_QUE_FLAG_BLOCK_QUE_GET);
    UTILS_assert(status == FVID2_SOK);

    pObj->reqQueCount = 0;
    pObj->isReqPend = FALSE;

    for (reqId = 0; reqId < DEC_LINK_MAX_REQ; reqId++)
    {
        status =
            Utils_quePut(&pObj->reqQue, &pObj->reqObj[reqId], BIOS_NO_WAIT);
        UTILS_assert(status == FVID2_SOK);
    }

    return FVID2_SOK;
}

static Int32 DecLink_codecCreateDupObj(DecLink_Obj * pObj)
{
    Int32 status;
    Int i;

    memset(&pObj->dupObj, 0, sizeof(pObj->dupObj));
    status = Utils_queCreate(&pObj->dupObj.dupQue,
                             UTILS_ARRAYSIZE(pObj->dupObj.dupQueMem),
                             pObj->dupObj.dupQueMem,
                             UTILS_QUE_FLAG_BLOCK_QUE_GET);
    UTILS_assertError(!UTILS_ISERROR(status),
                      status,
                      DEC_LINK_E_DUPOBJ_CREATE_FAILED, pObj->linkId, -1);
    if (!UTILS_ISERROR(status))
    {
        for (i = 0; i < DEC_LINK_MAX_DUP_FRAMES; i++)
        {
            pObj->dupObj.frameInfo[i].pVdecOrgFrame = NULL;
            pObj->dupObj.frameInfo[i].vdecRefCount = 0;
            pObj->dupObj.dupFrameMem[i].appData = &(pObj->dupObj.frameInfo[i]);
            status = Utils_quePut(&pObj->dupObj.dupQue,
                                  &pObj->dupObj.dupFrameMem[i], BIOS_NO_WAIT);
            UTILS_assert(!UTILS_ISERROR(status));
        }
    }
    return status;
}

static Int32 DecLink_codecDeleteDupObj(DecLink_Obj * pObj)
{
    Int32 status;

    UTILS_assertError((Utils_queIsFull(&pObj->dupObj.dupQue) == TRUE),
                      status,
                      DEC_LINK_E_DUPOBJ_DELETE_FAILED, pObj->linkId, -1);
    status = Utils_queDelete(&pObj->dupObj.dupQue);
    UTILS_assertError(!UTILS_ISERROR(status),
                      status,
                      DEC_LINK_E_DUPOBJ_DELETE_FAILED, pObj->linkId, -1);
    return status;
}

static Void DecLink_codecPrdCalloutFcn(UArg arg)
{
    DecLink_Obj *pObj = (DecLink_Obj *) arg;
    Int32 status;

    status = System_sendLinkCmd(pObj->linkId, DEC_LINK_CMD_GET_PROCESSED_DATA);


    if (UTILS_ISERROR(status))
    {
        #ifdef SYSTEM_DEBUG_CMD_ERROR
        UTILS_warn("DECLINK:[%s:%d]:"
                   "System_sendLinkCmd DEC_LINK_CMD_GET_PROCESSED_DATA failed"
                   "errCode = %d", __FILE__, __LINE__, status);
        #endif
    }

}

static Int32 DecLink_codecCreatePrdObj(DecLink_Obj * pObj)
{
    Clock_Params clockParams;

    Clock_Params_init(&clockParams);
    clockParams.arg = (UArg) pObj;
    UTILS_assert(pObj->periodicObj.clkHandle == NULL);

    Clock_construct(&(pObj->periodicObj.clkStruct), DecLink_codecPrdCalloutFcn,
                    1, &clockParams);
    pObj->periodicObj.clkHandle = Clock_handle(&pObj->periodicObj.clkStruct);
    pObj->periodicObj.clkStarted = FALSE;

    return DEC_LINK_S_SUCCESS;

}

static Int32 DecLink_codecDeletePrdObj(DecLink_Obj * pObj)
{
    /* Stop the clock */
    Clock_stop(pObj->periodicObj.clkHandle);
    Clock_destruct(&(pObj->periodicObj.clkStruct));
    pObj->periodicObj.clkHandle = NULL;
    pObj->periodicObj.clkStarted = FALSE;

    return DEC_LINK_S_SUCCESS;
}

static Int32 DecLink_codecStartPrdObj(DecLink_Obj * pObj, UInt period)
{

    UTILS_assert(pObj->periodicObj.clkHandle != NULL);

    if (FALSE == pObj->periodicObj.clkStarted)
    {
        Clock_setPeriod(pObj->periodicObj.clkHandle, period);
        Clock_setTimeout(pObj->periodicObj.clkHandle, period);
        Clock_start(pObj->periodicObj.clkHandle);
        pObj->periodicObj.clkStarted = TRUE;
    }

    return DEC_LINK_S_SUCCESS;
}

static Int32 DecLink_codecStopPrdObj(DecLink_Obj * pObj)
{

    UTILS_assert(pObj->periodicObj.clkHandle != NULL);

    if (TRUE == pObj->periodicObj.clkStarted)
    {
        Clock_stop(pObj->periodicObj.clkHandle);
        pObj->periodicObj.clkStarted = FALSE;
    }
    return DEC_LINK_S_SUCCESS;
}

static Int32 DecLink_codecCreateOutObj(DecLink_Obj * pObj)
{
    DecLink_OutObj *pOutObj;
    Int32 status;
    UInt32 frameId, outId, chId;
    FVID2_Format *pFormat;
    System_LinkChInfo *pOutChInfo;
    UInt32 totalframeCnt;
    System_FrameInfo *pFrameInfo;

    pOutObj = &pObj->outObj;
    status = Utils_bufCreateExt(&pOutObj->bufOutQue, TRUE, FALSE,
                                pObj->outObj.numAllocPools);
    UTILS_assert(status == FVID2_SOK);

    status = Utils_queCreate(&pObj->processDoneQue,
                             DEC_LINK_MAX_OUT_FRAMES,
                             pObj->processDoneQueMem,
                             (UTILS_QUE_FLAG_BLOCK_QUE_GET |
                              UTILS_QUE_FLAG_BLOCK_QUE_PUT));
    UTILS_assert(status == FVID2_SOK);



    totalframeCnt = 0;
    for (outId = 0u; outId < pOutObj->numAllocPools; outId++)
    {
        pOutObj = &pObj->outObj;

    pOutObj->outNumFrames[outId] =
         pOutObj->numChPerResolutionClass[outId] *
         pObj->createArgs.numBufPerPool[outId];

        UTILS_assert(pOutObj->numChPerResolutionClass[outId] <= DEC_LINK_MAX_CH);
        UTILS_assert(pOutObj->outNumFrames[outId] <= DEC_LINK_MAX_OUT_FRAMES);

        pFormat = &pOutObj->outFormat[outId];

        pFormat->channelNum = 0;
        pFormat->dataFormat = FVID2_DF_YUV420SP_UV;
        pFormat->fieldMerged[0] = FALSE;
        pFormat->fieldMerged[1] = FALSE;
        pFormat->fieldMerged[2] = FALSE;

        UTILS_assert(outId < UTILS_ENCDEC_RESOLUTION_CLASS_COUNT);
        UTILS_assert(outId < UTILS_BUF_MAX_ALLOC_POOLS);
        switch (pOutObj->reslutionClass[outId])
        {
                /* Modify this with formula to calculate the single buffer
                 * size, if possible */
            case UTILS_ENCDEC_RESOLUTION_CLASS_CIF:       // CIF
                pFormat->width =
                    UTILS_ENCDEC_GET_PADDED_WIDTH
                    (UTILS_ENCDEC_RESOLUTION_CLASS_CIF_WIDTH);
                pFormat->height =
                    UTILS_ENCDEC_GET_PADDED_HEIGHT
                    (UTILS_ENCDEC_RESOLUTION_CLASS_CIF_HEIGHT);
                pFormat->pitch[0] =
                    VpsUtils_align(pFormat->width, VPS_BUFFER_ALIGNMENT);
                break;
            case UTILS_ENCDEC_RESOLUTION_CLASS_D1:        // D1
                pFormat->width =
                    UTILS_ENCDEC_GET_PADDED_WIDTH
                    (UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH);
                pFormat->height =
                    UTILS_ENCDEC_GET_PADDED_HEIGHT
                    (UTILS_ENCDEC_RESOLUTION_CLASS_D1_HEIGHT);
                pFormat->pitch[0] =
                    VpsUtils_align(pFormat->width, VPS_BUFFER_ALIGNMENT);
                break;
            case UTILS_ENCDEC_RESOLUTION_CLASS_720P:      // 720p
                pFormat->width =
                    UTILS_ENCDEC_GET_PADDED_WIDTH
                    (UTILS_ENCDEC_RESOLUTION_CLASS_720P_WIDTH);
                pFormat->height =
                    UTILS_ENCDEC_GET_PADDED_HEIGHT
                    (UTILS_ENCDEC_RESOLUTION_CLASS_720P_HEIGHT);
                pFormat->pitch[0] =
                    VpsUtils_align(pFormat->width, VPS_BUFFER_ALIGNMENT);
                break;
            case UTILS_ENCDEC_RESOLUTION_CLASS_1080P:     // 1080p
                pFormat->width =
                    UTILS_ENCDEC_GET_PADDED_WIDTH
                    (UTILS_ENCDEC_RESOLUTION_CLASS_1080P_WIDTH);
                pFormat->height =
                    UTILS_ENCDEC_GET_PADDED_HEIGHT
                    (UTILS_ENCDEC_RESOLUTION_CLASS_1080P_HEIGHT);
                pFormat->pitch[0] =
                    VpsUtils_align(pFormat->width, VPS_BUFFER_ALIGNMENT);
                break;
            default:
                UTILS_warn("DECLINK: Unknown reslutionClass");
                UTILS_assert(pOutObj->reslutionClass[outId] ==
                             UTILS_ENCDEC_RESOLUTION_CLASS_CIF);
                break;
        }
#if 0
        /* assume all channels have same width and height */
        pFormat->width =
            UTILS_ENCDEC_GET_PADDED_WIDTH
            (pObj->inQueInfo.chInfo[0].width);
    /*    pFormat->height =
            UTILS_ENCDEC_GET_PADDED_HEIGHT
            (pObj->inQueInfo.chInfo[0].height); */
        pFormat->pitch[0] =
            VpsUtils_align(pFormat->width, VPS_BUFFER_ALIGNMENT);
#endif
        pFormat->pitch[1] = pFormat->pitch[0];
        if (pObj->createArgs.tilerEnable &&
            (pFormat->dataFormat == FVID2_DF_YUV420SP_UV))
        {
            pFormat->pitch[0] = VPSUTILS_TILER_CNT_8BIT_PITCH;
            pFormat->pitch[1] = VPSUTILS_TILER_CNT_16BIT_PITCH;
        }
        pFormat->pitch[2] = 0;

        pFormat->scanFormat = FVID2_SF_PROGRESSIVE;
        pFormat->bpp = FVID2_BPP_BITS16;
        pFormat->reserved = NULL;

        if (pObj->createArgs.tilerEnable)
        {
            status = Utils_tilerFrameAlloc(&pOutObj->outFormat[outId],
                                           &pOutObj->outFrames[totalframeCnt],
                                           pOutObj->outNumFrames[outId]);
        }
        else
        {
            status = Utils_memFrameAlloc(&pOutObj->outFormat[outId],
                                         &pOutObj->outFrames[totalframeCnt],
                                         pOutObj->outNumFrames[outId]);
        }
        UTILS_assert(status == FVID2_SOK);

        for (frameId = 0; frameId < pOutObj->outNumFrames[outId]; frameId++)
        {
            UTILS_assert((frameId + totalframeCnt) < DEC_LINK_MAX_OUT_FRAMES);
            pFrameInfo = (System_FrameInfo *)
                           &pOutObj->frameInfo[frameId + totalframeCnt];
            UTILS_assert(pFrameInfo != NULL);
            pFrameInfo->allocPoolID = outId;
            pFrameInfo->rtChInfoUpdate = FALSE;
            pOutObj->outFrames[frameId + totalframeCnt].appData = pFrameInfo;
            status = Utils_bufPutEmptyFrameExt(&pOutObj->bufOutQue,
                           &pOutObj->outFrames[frameId + totalframeCnt]);
            UTILS_assert(status == FVID2_SOK);
        }
        totalframeCnt += pOutObj->outNumFrames[outId];
    }

    pObj->info.numQue = DEC_LINK_MAX_OUT_QUE;
    for (outId = 0u; outId < DEC_LINK_MAX_OUT_QUE; outId++)
    {
        pObj->info.queInfo[outId].numCh = pObj->inQueInfo.numCh;
    }

    for (chId = 0u; chId < pObj->inQueInfo.numCh; chId++)
    {
        for (outId = 0u; outId < DEC_LINK_MAX_OUT_QUE; outId++)
        {
            Uint32 poolId = pOutObj->ch2poolMap[chId];

            pFormat = &pObj->outObj.outFormat[poolId];
            pOutChInfo = &pObj->info.queInfo[outId].chInfo[chId];

            pOutChInfo->bufType = SYSTEM_BUF_TYPE_VIDFRAME;
            pOutChInfo->codingformat =
                pObj->createArgs.chCreateParams[chId].format;
            pOutChInfo->memType = VPS_VPDMA_MT_NONTILEDMEM;
            if (pObj->createArgs.tilerEnable)
            {
                pOutChInfo->memType = VPS_VPDMA_MT_TILEDMEM;
            }
            pOutChInfo->dataFormat = pFormat->dataFormat;
            pOutChInfo->scanFormat = pObj->inQueInfo.chInfo[chId].scanFormat;
            if (pObj->createArgs.chCreateParams[chId].format == IVIDEO_MJPEG)
            {
                pOutChInfo->startX = 0;
                pOutChInfo->startY = 0;
            }
            else
            {
                pOutChInfo->startX = 32;
                pOutChInfo->startY = 24;
            }
            pOutChInfo->width = pObj->inQueInfo.chInfo[chId].width;
            pOutChInfo->height = pObj->inQueInfo.chInfo[chId].height;
            pOutChInfo->pitch[0] = pFormat->pitch[0];
            pOutChInfo->pitch[1] = pFormat->pitch[1];
        }
    }

    return (status);
}

static Void DecLink_codecInitFrameDebugObj(DecLink_ChObj *pChObj)
{
    pChObj->frameDebug.inBufIdx = 0;
    pChObj->frameDebug.outBufIdx = 0;
}


static Int32 DecLink_codecCreateChObj(DecLink_Obj * pObj, UInt32 chId)
{
    DecLink_ChObj *pChObj;
    Int32 status;

    pChObj = &pObj->chObj[chId];

    status = Utils_queCreate(&pChObj->inQue, DEC_LINK_MAX_REQ,
                             pChObj->inBitBufMem,
                             (UTILS_QUE_FLAG_BLOCK_QUE_GET |
                              UTILS_QUE_FLAG_BLOCK_QUE_PUT));
    UTILS_assert(status == FVID2_SOK);

    pChObj->allocPoolID = pObj->outObj.ch2poolMap[chId];
    pChObj->totalInFrameCnt = 0;
    DecLink_codecInitFrameDebugObj(pChObj);
    return FVID2_SOK;
}

static Int32 declink_codec_set_ch_alg_create_params(DecLink_Obj * pObj,
                                                    UInt32 chId)
{
    DecLink_ChObj *pChObj;

    pChObj = &pObj->chObj[chId];
    pChObj->algObj.algCreateParams.format =
        (IVIDEO_Format) pObj->createArgs.chCreateParams[chId].format;
    pChObj->algObj.algCreateParams.presetProfile =
        pObj->createArgs.chCreateParams[chId].profile;
    pChObj->algObj.algCreateParams.fieldMergeDecodeEnable =
        pObj->createArgs.chCreateParams[chId].fieldMergeDecodeEnable;
    pChObj->algObj.algCreateParams.maxWidth =
        pObj->createArgs.chCreateParams[chId].targetMaxWidth;
    pChObj->algObj.algCreateParams.maxHeight =
        pObj->createArgs.chCreateParams[chId].targetMaxHeight;
    pChObj->algObj.algCreateParams.maxFrameRate =
        pObj->createArgs.chCreateParams[chId].defaultDynamicParams.
        targetFrameRate;
    pChObj->algObj.algCreateParams.maxBitRate =
        pObj->createArgs.chCreateParams[chId].defaultDynamicParams.
        targetBitRate;
    pChObj->algObj.algCreateParams.tilerEnable = pObj->createArgs.tilerEnable;

    pChObj->algObj.algCreateParams.displayDelay =
        decLink_map_displayDelay2CodecParam(pObj->createArgs.chCreateParams[chId].displayDelay);

    Utils_encdecGetCodecLevel(pChObj->algObj.algCreateParams.format,
                              pChObj->algObj.algCreateParams.maxWidth,
                              pChObj->algObj.algCreateParams.maxHeight,
                              pObj->createArgs.chCreateParams[chId].
                              defaultDynamicParams.targetFrameRate,
                              pObj->createArgs.chCreateParams[chId].
                              defaultDynamicParams.targetBitRate,
                              &(pChObj->algObj.algCreateParams.presetLevel),
                              FALSE);
    pChObj->algObj.algCreateParams.dpbBufSizeInFrames =
                      pObj->createArgs.chCreateParams[chId].dpbBufSizeInFrames;

    return DEC_LINK_S_SUCCESS;
}

static Int32 declink_codec_set_ch_alg_default_dynamic_params(DecLink_Obj * pObj,
                                                             UInt32 chId)
{
    DecLink_ChObj *pChObj;

    pChObj = &pObj->chObj[chId];
    pChObj->algObj.algDynamicParams.decodeHeader =
        DEC_LINK_DEFAULT_ALGPARAMS_DECODEHEADER;
#if 0
    pChObj->algObj.algDynamicParams.displayWidth =
        DEC_LINK_DEFAULT_ALGPARAMS_DISPLAYWIDTH;
#endif
    pChObj->algObj.algDynamicParams.displayWidth =
        pObj->info.queInfo[0].chInfo[chId].pitch[0];
    pChObj->algObj.algDynamicParams.frameSkipMode =
        DEC_LINK_DEFAULT_ALGPARAMS_FRAMESKIPMODE;
    pChObj->algObj.algDynamicParams.newFrameFlag =
        DEC_LINK_DEFAULT_ALGPARAMS_NEWFRAMEFLAG;

    return DEC_LINK_S_SUCCESS;
}

static Int32 DecLink_codecCreateDecObj(DecLink_Obj * pObj, UInt32 chId)
{
    Int retVal;
    DecLink_ChObj *pChObj;
    Int scratchGroupID;
    UInt32 contentType;

    pChObj = &pObj->chObj[chId];
    scratchGroupID = -1;

    declink_codec_set_ch_alg_create_params(pObj, chId);
    declink_codec_set_ch_alg_default_dynamic_params(pObj, chId);

    contentType =
        Utils_encdecMapFVID2XDMContentType(pObj->inQueInfo.chInfo[chId].
                                           scanFormat);

    if (contentType == IVIDEO_INTERLACED)
    {
        pChObj->numReqObjPerProcess = 2;
    }
    else
    {
        pChObj->numReqObjPerProcess = 1;
    }
    pChObj->algObj.prevOutFrame = NULL;
    switch (pChObj->algObj.algCreateParams.format)
    {
        case IVIDEO_H264BP:
        case IVIDEO_H264MP:
        case IVIDEO_H264HP:
            retVal = DecLinkH264_algCreate(&pChObj->algObj.u.h264AlgIfObj,
                                           &pChObj->algObj.algCreateParams,
                                           &pChObj->algObj.algDynamicParams,
                                           pObj->linkId, chId, scratchGroupID);
            break;

    case IVIDEO_MPEG4SP:
    case IVIDEO_MPEG4ASP:
      retVal = DecLinkMPEG4_algCreate(&pChObj->algObj.u.mpeg4AlgIfObj,
                       &pChObj->algObj.algCreateParams,
                       &pChObj->algObj.algDynamicParams,
                       pObj->linkId, chId, scratchGroupID);
      break;

        case IVIDEO_MJPEG:
            retVal = DecLinkJPEG_algCreate(&pChObj->algObj.u.jpegAlgIfObj,
                                               &pChObj->algObj.algCreateParams,
                                               &pChObj->algObj.algDynamicParams,
                                               pObj->linkId, chId, scratchGroupID);
            break;

        default:
            retVal = DEC_LINK_E_UNSUPPORTEDCODEC;
            break;

    }
    UTILS_assert(retVal == DEC_LINK_S_SUCCESS);

    return retVal;
}

static Int32 declink_codec_init_outframe(DecLink_Obj * pObj,
                                         UInt32 chId, FVID2_Frame * pFrame)
{
    System_FrameInfo *pFrameInfo;

    pFrameInfo = (System_FrameInfo *) pFrame->appData;
    UTILS_assert((pFrameInfo != NULL)
                 &&
                 UTILS_ARRAYISVALIDENTRY(pFrameInfo,
                                         pObj->outObj.frameInfo));
    pFrameInfo->vdecRefCount = 1;
    pFrameInfo->pVdecOrgFrame = NULL;
    return DEC_LINK_S_SUCCESS;
}

static Int32 DecLink_codecGetDisplayFrame(DecLink_Obj * pObj,
                                          FVID2_Frame * outFrame,
                                          FVID2_FrameList * freeFrameList,
                                          FVID2_Frame ** displayFrame)
{
    Int i, j, status = DEC_LINK_S_SUCCESS;
    Bool doDup = TRUE;

    if (outFrame != NULL)
    {
        *displayFrame = NULL;
        UTILS_assert(UTILS_ARRAYISVALIDENTRY(outFrame,
                                             pObj->outObj.outFrames));

        for (i = 0; i < freeFrameList->numFrames; i++)
        {
            if (freeFrameList->frames[i] == outFrame)
            {
                /* This frame is going to be used as display frame. Remove
                 * it from the freeFrameList */
                for (j = (i + 1); j < freeFrameList->numFrames; j++)
                {
                    freeFrameList->frames[j - 1] = freeFrameList->frames[j];
                }
                freeFrameList->numFrames -= 1;
                doDup = FALSE;
                break;
            }
        }
    }
    else
    {
        doDup = FALSE;
    }
    if (FALSE == doDup)
    {
        *displayFrame = outFrame;
    }
    else
    {
        status = DecLink_dupFrame(pObj, outFrame, displayFrame);
    }
    return status;
}

Int32 DecLink_codecFreeProcessedFrames(DecLink_Obj * pObj,
                                       FVID2_FrameList * freeFrameList)
{
    Int i, status = DEC_LINK_S_SUCCESS;
    FVID2_Frame *freeFrame;
    FVID2_Frame *origFrame;
    System_FrameInfo *freeFrameInfo;
    UInt cookie;
    Bool bufFreeDone = FALSE;

    cookie = Hwi_disable();

    for (i = 0; i < freeFrameList->numFrames; i++)
    {
        freeFrame = freeFrameList->frames[i];
        UTILS_assert(freeFrame != NULL);
        freeFrameInfo = freeFrame->appData;
        UTILS_assert(freeFrameInfo != NULL);
        if (freeFrameInfo->pVdecOrgFrame)
        {
            UTILS_assert(UTILS_ARRAYISVALIDENTRY(freeFrame,
                                                 pObj->dupObj.dupFrameMem));
            origFrame = freeFrameInfo->pVdecOrgFrame;
            status = Utils_quePut(&pObj->dupObj.dupQue,
                                  freeFrame, BIOS_NO_WAIT);
            UTILS_assert(!UTILS_ISERROR(status));
            freeFrame = origFrame;
            freeFrameInfo = origFrame->appData;
        }
        UTILS_assert((freeFrameInfo->pVdecOrgFrame == NULL)
                     && (freeFrameInfo->vdecRefCount > 0));
        freeFrameInfo->vdecRefCount--;
        if (freeFrameInfo->vdecRefCount == 0)
        {
            status =
                Utils_bufPutEmptyFrameExt(&pObj->outObj.bufOutQue, freeFrame);
            UTILS_assert(!UTILS_ISERROR(status));
            bufFreeDone = TRUE;
        }
    }

    Hwi_restore(cookie);
    if ((TRUE == pObj->newDataProcessOnFrameFree)
      &&
      (bufFreeDone))
    {
        status = System_sendLinkCmd(pObj->linkId,
                                    SYSTEM_CMD_NEW_DATA);

        if (UTILS_ISERROR(status))
        {
            UTILS_warn("DECLINK:[%s:%d]:"
                       "System_sendLinkCmd SYSTEM_CMD_NEW_DATA failed"
                       "errCode = %d", __FILE__, __LINE__, status);
        }
        else
        {
          pObj->newDataProcessOnFrameFree = FALSE;
        }
    }
    return status;
}

static Void DecLink_codecProcessTskFxn(UArg arg1, UArg arg2)
{
    Int32 status, chId, i, j;
    DecLink_Obj *pObj;
    DecLink_ChObj *pChObj;
    DecLink_ReqObj *pReqObj;
    FVID2_FrameList freeFrameList;
    UInt32 tskId;

    pObj = (DecLink_Obj *) arg1;
    tskId = (UInt32) arg2;

    while (pObj->state != SYSTEM_LINK_STATE_STOP)
    {
        pObj->reqObjBatch[tskId].numReqObjsInBatch = 0;
        status = DEC_LINK_S_SUCCESS;
        status = Utils_queGet(&pObj->decProcessTsk[tskId].processQue,
                              (Ptr *) & pReqObj, 1, BIOS_WAIT_FOREVER);
        if (!UTILS_ISERROR(status))
        {
            status = DecLink_PrepareBatch (pObj, tskId, pReqObj, 
                                  &pObj->reqObjBatch[tskId]);
            
          if (UTILS_ISERROR(status))
          {
              UTILS_warn("DEC : IVAHDID : %d ENCLINK:ERROR in "
                         "DecLink_SubmitBatch.Status[%d]", tskId, status);
          }
          else 
          {
              /*Log Batch size statistics*/
              pObj->batchStatistics[tskId].numBatchesSubmitted++;
              
              pObj->batchStatistics[tskId].currentBatchSize = pObj->
                reqObjBatch[tskId].numReqObjsInBatch;
              
              if (pObj->batchStatistics[tskId].maxAchievedBatchSize <
                  pObj->batchStatistics[tskId].currentBatchSize)
              {
                pObj->batchStatistics[tskId].maxAchievedBatchSize =
                  pObj->batchStatistics[tskId].currentBatchSize;
              }
                            
              pObj->batchStatistics[tskId].aggregateBatchSize = 
                pObj->batchStatistics[tskId].aggregateBatchSize + 
                pObj->batchStatistics[tskId].currentBatchSize;
               
              pObj->batchStatistics[tskId].averageBatchSize = 
                pObj->batchStatistics[tskId].aggregateBatchSize /
                pObj->batchStatistics[tskId].numBatchesSubmitted;
          }            
        }
        freeFrameList.numFrames = 0;
        if (pObj->reqObjBatch[tskId].numReqObjsInBatch)
        {
            /*Its made sure that for every batch created all ReqObj have the same
            codec. And every Request Batch has atleast one ReqObj */
            chId = pObj->reqObjBatch[tskId].pReqObj[0]->InBuf->channelNum;
            pChObj = &pObj->chObj[chId];
            switch (pChObj->algObj.algCreateParams.format)
            {
                case IVIDEO_H264BP:
                case IVIDEO_H264MP:
                case IVIDEO_H264HP:
                    status = 
                        Declink_h264DecodeFrameBatch(pObj, 
                                                     &pObj->reqObjBatch[tskId],
                                                     &freeFrameList, tskId);
                    if (UTILS_ISERROR(status))
                    {
                         #ifndef DEC_LINK_SUPRESS_ERROR_AND_RESET
                         /*
                         UTILS_warn("DECLINK:ERROR in "
                              "Declink_h264DecodeFrameBatch.Status[%d]", status); 
                         */
                         #endif   
                    }
               break;
         
               case IVIDEO_MPEG4SP:
               case IVIDEO_MPEG4ASP:
                status = Declink_mpeg4DecodeFrameBatch(pObj, 
                                                            &pObj->reqObjBatch[tskId],
                                                            &freeFrameList);
                if (UTILS_ISERROR(status))
                {
                  #ifndef DEC_LINK_SUPRESS_ERROR_AND_RESET
                   UTILS_warn("DECLINK:ERROR in "
                      "Declink_mpeg4DecodeFrameBatch.Status[%d]", status);
                  #endif

                }
               break;

                case IVIDEO_MJPEG:

                   status = 
                      Declink_jpegDecodeFrameBatch(pObj, 
                                                   &pObj->reqObjBatch[tskId],
                                                   &freeFrameList);
                   if (UTILS_ISERROR(status))
                   {
                       UTILS_warn("DECLINK:ERROR in "
                             "Declink_jpegDecodeFrameBatch.Status[%d]", status);
                   }
                   
                 break;

                default:
                    UTILS_assert(FALSE);
            }
        }
        for (i = 0; i < pObj->reqObjBatch[tskId].numReqObjsInBatch; i++)
        {
            pReqObj = pObj->reqObjBatch[tskId].pReqObj[i];

            for (j = 0; j < pReqObj->OutFrameList.numFrames; j++)
            {
                FVID2_Frame *displayFrame;

                    DecLink_codecGetDisplayFrame(pObj,
                                             pReqObj->OutFrameList.frames[j],
                                             &freeFrameList, &displayFrame);
                    pReqObj->OutFrameList.frames[j] = displayFrame;

            }
            status = Utils_quePut(&pObj->processDoneQue, pReqObj,
                                  BIOS_NO_WAIT);
            UTILS_assert(status == FVID2_SOK);
        }
        
        DecLink_codecFreeProcessedFrames(pObj, &freeFrameList);
    }

    return;
}

#pragma DATA_ALIGN(gDecProcessTskStack, 32)
#pragma DATA_SECTION(gDecProcessTskStack, ".bss:taskStackSection")
UInt8 gDecProcessTskStack[NUM_HDVICP_RESOURCES][DEC_LINK_PROCESS_TSK_SIZE];

static Int32 DecLink_codecCreateProcessTsk(DecLink_Obj * pObj, UInt32 tskId)
{
    Int32 status = DEC_LINK_S_SUCCESS;
    Task_Params tskParams;
    Error_Block ebObj;
    Error_Block *eb = &ebObj;

    Error_init(eb);

    Task_Params_init(&tskParams);

    snprintf(pObj->decProcessTsk[tskId].name,
             (sizeof(pObj->decProcessTsk[tskId].name) - 1),
             "DEC_PROCESS_TSK_%d ", tskId);
    pObj->decProcessTsk[tskId].
          name[(sizeof(pObj->decProcessTsk[tskId].name) - 1)] = 0;
    tskParams.priority = IVASVR_TSK_PRI;
    tskParams.stack = &gDecProcessTskStack[tskId][0];
    tskParams.stackSize = DEC_LINK_PROCESS_TSK_SIZE;
    tskParams.arg0 = (UArg) pObj;
    tskParams.arg1 = (UArg) tskId;
    tskParams.instance->name = pObj->decProcessTsk[tskId].name;

    Task_construct(&pObj->decProcessTsk[tskId].tskStruct,
                   DecLink_codecProcessTskFxn, &tskParams, eb);
    UTILS_assertError((Error_check(eb) == FALSE), status,
                      DEC_LINK_E_TSKCREATEFAILED, pObj->linkId, tskId);
    if (DEC_LINK_S_SUCCESS == status)
    {
        pObj->decProcessTsk[tskId].tsk =
              Task_handle(&pObj->decProcessTsk[tskId].tskStruct);
        Utils_prfLoadRegister(pObj->decProcessTsk[tskId].tsk,
                              pObj->decProcessTsk[tskId].name);
    }
    return status;
}

static Int32 DecLink_codecDeleteProcessTsk(DecLink_Obj * pObj, UInt32 tskId)
{
    Int32 status = DEC_LINK_S_SUCCESS;

    Utils_queUnBlock(&pObj->decProcessTsk[tskId].processQue);
    Utils_queUnBlock(&pObj->processDoneQue);
    while (Task_getMode(pObj->decProcessTsk[tskId].tsk) != Task_Mode_TERMINATED)
    {
        Task_sleep(DEC_LINK_TASK_POLL_DURATION_MS);
    }
    Utils_prfLoadUnRegister(pObj->decProcessTsk[tskId].tsk);

    Task_destruct(&pObj->decProcessTsk[tskId].tskStruct);
    pObj->decProcessTsk[tskId].tsk = NULL;
    return status;
}

static Int32 DecLink_codecMapCh2ProcessTskId(DecLink_Obj * pObj)
{
    UInt32 chId;
    Int32 status = DEC_LINK_S_SUCCESS;

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pObj->ch2ProcessTskId[chId] = Utils_encdecGetDecoderIVAID (chId);
    }

    return status;
}

static Int32 DecLink_codecMapMaxNumBufsChPool(DecLink_Obj * pObj)
{
    UInt32 chId, i;
    UInt32 maxoutbuf[DEC_LINK_MAX_BUF_ALLOC_POOLS] = {0};
    Int32 status = DEC_LINK_S_SUCCESS;
    DecLink_OutObj *pOutObj;

    pOutObj = &pObj->outObj;

    /*For every set of channels in a pool, find the max of num of output buffers required*/
    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        if(maxoutbuf[pOutObj->ch2poolMap[chId]] 
            < pObj->createArgs.chCreateParams[chId].numBufPerCh)
        {           
            maxoutbuf[pOutObj->ch2poolMap[chId]] 
                      = pObj->createArgs.chCreateParams[chId].numBufPerCh;
        }
    }

    for (i=0; i < pOutObj->numAllocPools; i++)
    {
        if(maxoutbuf[i] != 0)
            pObj->createArgs.numBufPerPool[i] = maxoutbuf[i];
    }

    for (i=0; i<DEC_LINK_MAX_BUF_ALLOC_POOLS; i++)
    {
        UTILS_assert(i < UTILS_BUF_MAX_ALLOC_POOLS);
        if(pObj->createArgs.numBufPerPool[i] == 0)
            pObj->createArgs.numBufPerPool[i] = DEC_LINK_MAX_OUT_FRAMES_PER_CH;
    }

    return status;
}

static Int32 DecLink_codecMapCh2OutPollId(DecLink_Obj * pObj, UInt32 chId)
{
    Int32 status = DEC_LINK_E_FAIL;
    UInt32 outId;
    DecLink_OutObj *pOutObj;

    pOutObj = &pObj->outObj;

    if ((pObj->createArgs.chCreateParams[chId].targetMaxWidth <=
         UTILS_ENCDEC_RESOLUTION_CLASS_CIF_WIDTH) &&
        (pObj->createArgs.chCreateParams[chId].targetMaxHeight <=
         UTILS_ENCDEC_RESOLUTION_CLASS_CIF_HEIGHT))
    {
        for (outId = 0; outId < UTILS_BUF_MAX_ALLOC_POOLS; outId++)
        {
            if (pOutObj->reslutionClass[outId] ==
                UTILS_ENCDEC_RESOLUTION_CLASS_CIF)
            {
                pOutObj->ch2poolMap[chId] = outId;
                status = DEC_LINK_S_SUCCESS;
                break;
            }
        }
        if (status != DEC_LINK_S_SUCCESS)
        {
            pOutObj->reslutionClass[pOutObj->numAllocPools] =
                UTILS_ENCDEC_RESOLUTION_CLASS_CIF;
            pOutObj->ch2poolMap[chId] = pOutObj->numAllocPools++;
            status = DEC_LINK_S_SUCCESS;
        }
    }
    if ((status != DEC_LINK_S_SUCCESS) &&
        (pObj->createArgs.chCreateParams[chId].targetMaxWidth <=
         UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH) &&
        (pObj->createArgs.chCreateParams[chId].targetMaxHeight <=
         UTILS_ENCDEC_RESOLUTION_CLASS_D1_HEIGHT))
    {
        for (outId = 0; outId < UTILS_BUF_MAX_ALLOC_POOLS; outId++)
        {
            if (pOutObj->reslutionClass[outId] ==
                UTILS_ENCDEC_RESOLUTION_CLASS_D1)
            {
                pOutObj->ch2poolMap[chId] = outId;
                status = DEC_LINK_S_SUCCESS;
                break;
            }
        }
        if (status != DEC_LINK_S_SUCCESS)
        {
            pOutObj->reslutionClass[pOutObj->numAllocPools] =
                UTILS_ENCDEC_RESOLUTION_CLASS_D1;
            pOutObj->ch2poolMap[chId] = pOutObj->numAllocPools++;
            status = DEC_LINK_S_SUCCESS;
        }
    }
    if ((status != DEC_LINK_S_SUCCESS) &&
        (pObj->createArgs.chCreateParams[chId].targetMaxWidth <=
         UTILS_ENCDEC_RESOLUTION_CLASS_720P_WIDTH) &&
        (pObj->createArgs.chCreateParams[chId].targetMaxHeight <=
         UTILS_ENCDEC_RESOLUTION_CLASS_720P_HEIGHT))
    {
        for (outId = 0; outId < UTILS_BUF_MAX_ALLOC_POOLS; outId++)
        {
            if (pOutObj->reslutionClass[outId] ==
                UTILS_ENCDEC_RESOLUTION_CLASS_720P)
            {
                pOutObj->ch2poolMap[chId] = outId;
                status = DEC_LINK_S_SUCCESS;
                break;
            }
        }
        if (status != DEC_LINK_S_SUCCESS)
        {
            pOutObj->reslutionClass[pOutObj->numAllocPools] =
                UTILS_ENCDEC_RESOLUTION_CLASS_720P;
            pOutObj->ch2poolMap[chId] = pOutObj->numAllocPools++;
            status = DEC_LINK_S_SUCCESS;
        }
    }
    if ((status != DEC_LINK_S_SUCCESS) &&
        (pObj->createArgs.chCreateParams[chId].targetMaxWidth <=
         UTILS_ENCDEC_RESOLUTION_CLASS_1080P_WIDTH) &&
        (pObj->createArgs.chCreateParams[chId].targetMaxHeight <=
         UTILS_ENCDEC_RESOLUTION_CLASS_1080P_HEIGHT))
    {
        for (outId = 0; outId < UTILS_BUF_MAX_ALLOC_POOLS; outId++)
        {
            if (pOutObj->reslutionClass[outId] ==
                UTILS_ENCDEC_RESOLUTION_CLASS_1080P)
            {
                pOutObj->ch2poolMap[chId] = outId;
                status = DEC_LINK_S_SUCCESS;
                break;
            }
        }
        if (status != DEC_LINK_S_SUCCESS)
        {
            pOutObj->reslutionClass[pOutObj->numAllocPools] =
                UTILS_ENCDEC_RESOLUTION_CLASS_1080P;
            pOutObj->ch2poolMap[chId] = pOutObj->numAllocPools++;
            status = DEC_LINK_S_SUCCESS;
        }
    }
    UTILS_assert(status == DEC_LINK_S_SUCCESS);
    pOutObj->numChPerResolutionClass[pOutObj->ch2poolMap[chId]]++;

    return (status);
}

static void DecLink_codecCreateInitStats(DecLink_Obj * pObj)
{
    Int32 chId;
    DecLink_ChObj *pChObj;

    pObj->inBufGetCount = 0;
    pObj->inBufPutCount = 0;

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pChObj = &pObj->chObj[chId];

        pChObj->prevFrmRecvTime = 0;
        pChObj->totalProcessTime = 0;
        pChObj->totalFrameIntervalTime = 0;
        pChObj->totalInFrameCnt = 0;
        pChObj->inBufQueCount = 0;
        pChObj->processReqestCount = 0;
        pChObj->getProcessedBufCount = 0;
        pChObj->numBufsInCodec       = 0;

        pChObj->disableChn = FALSE;

        pChObj->skipFrame = FALSE;
    }

    return;
}

static void DecLink_initTPlayConfig(DecLink_Obj * pObj)
{
    Int32 chId;
    DecLink_ChObj *pChObj;

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pChObj = &pObj->chObj[chId];

        pChObj->trickPlayObj.skipFrame = FALSE;
        pChObj->trickPlayObj.frameSkipCtx.outputFrameRate = 30;
        pChObj->trickPlayObj.frameSkipCtx.inputFrameRate  = 30;
        pChObj->trickPlayObj.frameSkipCtx.inCnt = 0;
        pChObj->trickPlayObj.frameSkipCtx.outCnt = 0;
        pChObj->trickPlayObj.frameSkipCtx.multipleCnt = 0;
        pChObj->trickPlayObj.frameSkipCtx.firstTime = TRUE;

    }

    return;
}


Int32 DecLink_codecCreate(DecLink_Obj * pObj, DecLink_CreateParams * pPrm)
{
    Int32 status;
    Int32 chId, outId, tskId;
    DecLink_ChObj *pChObj;

    #ifdef SYSTEM_DEBUG_DEC
    Vps_printf(" %d: DECODE: Create in progress ... !!!\n",
            Utils_getCurTimeInMsec()
        );
    #endif

    UTILS_MEMLOG_USED_START();
    memcpy(&pObj->createArgs, pPrm, sizeof(*pPrm));
    status = System_linkGetInfo(pPrm->inQueParams.prevLinkId, &pObj->inTskInfo);
    UTILS_assert(status == FVID2_SOK);
    UTILS_assert(pPrm->inQueParams.prevLinkQueId < pObj->inTskInfo.numQue);

    memcpy(&pObj->inQueInfo,
           &pObj->inTskInfo.queInfo[pPrm->inQueParams.prevLinkQueId],
           sizeof(pObj->inQueInfo));
    UTILS_assert(pObj->inQueInfo.numCh <= DEC_LINK_MAX_CH);

    memset(&pObj->outObj.ch2poolMap, 0, sizeof(pObj->outObj.ch2poolMap));
    #ifndef SYSTEM_USE_TILER
    if (pObj->createArgs.tilerEnable)
    {
        Vps_printf("DECLINK:!!WARNING.FORCIBLY DISABLING TILER since"
                   "tiler is disabled at build time");
        pObj->createArgs.tilerEnable = FALSE;
    }
    #endif

    pObj->newDataProcessOnFrameFree = FALSE;
    pObj->outObj.numAllocPools = 0;
    for (outId = 0; outId < UTILS_BUF_MAX_ALLOC_POOLS; outId++)
    {
        pObj->outObj.reslutionClass[outId] =
            (EncDec_ResolutionClass) (UTILS_ENCDEC_RESOLUTION_CLASS_LAST + 1);
        pObj->outObj.numChPerResolutionClass[outId] = 0;
        pObj->outObj.outNumFrames[outId] = 0;
    }

#if DECLINK_UPDATE_CREATEPARAMS_LOCALLY
    /* ENABLE the define DECLINK_UPDATE_CREATEPARAMS_LOCALLY if App is not
     * configuring the create time paramters */
    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        if(pObj->createArgs.chCreateParams[chId].format ==  IVIDEO_H264HP)
            pObj->createArgs.chCreateParams[chId] = DECLINK_DEFAULTCHCREATEPARAMS_H264;
        else if(pObj->createArgs.chCreateParams[chId].format ==  IVIDEO_MPEG4ASP)
            pObj->createArgs.chCreateParams[chId] = DECLINK_DEFAULTCHCREATEPARAMS_MPEG4;
        else if(pObj->createArgs.chCreateParams[chId].format ==  IVIDEO_MJPEG)
            pObj->createArgs.chCreateParams[chId] = DECLINK_DEFAULTCHCREATEPARAMS_JPEG;
    }
#endif

    DecLink_initTPlayConfig(pObj);
    DecLink_codecCreateInitStats(pObj);
    DecLink_resetStatistics(pObj);

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        status = DecLink_codecMapCh2OutPollId(pObj, chId);
        UTILS_assert(status == DEC_LINK_S_SUCCESS);
    }

    DecLink_codecMapMaxNumBufsChPool(pObj);

    DecLink_codecMapCh2ProcessTskId(pObj);
    DecLink_codecCreateOutObj(pObj);
    DecLink_codecCreateReqObj(pObj);
    pObj->state = SYSTEM_LINK_STATE_START;

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pChObj = &pObj->chObj[chId];

        #ifdef SYSTEM_DEBUG_DEC
        Vps_printf(" %d: DECODE: Creating CH%d of %d x %d [%s] [%s],"
                   "target bitrate = %d Kbps ... \n",
            Utils_getCurTimeInMsec(),
            chId,
            pObj->inQueInfo.chInfo[chId].width,
            pObj->inQueInfo.chInfo[chId].height,
            gSystem_nameScanFormat[pObj->inQueInfo.chInfo[chId].scanFormat],
            gSystem_nameMemoryType[pObj->createArgs.tilerEnable],
            pObj->createArgs.chCreateParams[chId].defaultDynamicParams.
                                                  targetBitRate/1000
            );
        #endif

        pChObj->IFrameOnlyDecode = FALSE;
        pChObj->inBufQueCount = 0;
        pChObj->processReqestCount = 0;
        pChObj->getProcessedBufCount = 0;

        DecLink_codecCreateChObj(pObj, chId);
        DecLink_codecCreateDecObj(pObj, chId);
    }

    #ifdef SYSTEM_DEBUG_DEC
    Vps_printf(" %d: DECODE: All CH Create ... DONE !!!\n",
            Utils_getCurTimeInMsec()
        );
    #endif

    for (tskId = 0; tskId < NUM_HDVICP_RESOURCES; tskId++)
    {
        status = Utils_queCreate(&pObj->decProcessTsk[tskId].processQue,
                                 DEC_LINK_MAX_OUT_FRAMES,
                                 pObj->decProcessTsk[tskId].processQueMem,
                                 (UTILS_QUE_FLAG_BLOCK_QUE_GET |
                                  UTILS_QUE_FLAG_BLOCK_QUE_PUT));
        UTILS_assert(status == FVID2_SOK);
        DecLink_codecCreateProcessTsk(pObj, tskId);
    }

    DecLink_codecCreateDupObj(pObj);
    DecLink_codecCreatePrdObj(pObj);
    DecLink_codecStartPrdObj(pObj, DEC_LINK_PROCESS_DONE_PERIOD_MS);

    UTILS_MEMLOG_USED_END(pObj->memUsed);
    UTILS_MEMLOG_PRINT("DECLINK",
                       pObj->memUsed,
                       UTILS_ARRAYSIZE(pObj->memUsed));
    #ifdef SYSTEM_DEBUG_DEC
    Vps_printf(" %d: DECODE: Create ... DONE !!!\n",
            Utils_getCurTimeInMsec()
        );
    #endif


    return FVID2_SOK;
}

static Int32 DecLink_codecQueueBufsToChQue(DecLink_Obj * pObj)
{
    UInt32 bufId, freeBufNum;
    Bitstream_Buf *pBuf;
    System_LinkInQueParams *pInQueParams;
    Bitstream_BufList bufList;
    DecLink_ChObj *pChObj;
    Int32 status;
    UInt32 curTime;

    pInQueParams = &pObj->createArgs.inQueParams;

    System_getLinksFullBufs(pInQueParams->prevLinkId,
                            pInQueParams->prevLinkQueId, &bufList);

    if (bufList.numBufs)
    {
        pObj->inBufGetCount += bufList.numBufs;

        freeBufNum = 0;
        curTime = Utils_getCurTimeInMsec();

        for (bufId = 0; bufId < bufList.numBufs; bufId++)
        {
            pBuf = bufList.bufs[bufId];

            pChObj = &pObj->chObj[pBuf->channelNum];

            pChObj->inFrameRecvCount++;

            // pBuf->fid = pChObj->nextFid;
            if(pChObj->disableChn && pChObj->skipFrame == FALSE)
            {
                pChObj->skipFrame = TRUE;
            }
            else if((pChObj->disableChn == FALSE) && pChObj->skipFrame)
            {
                if(pBuf->isKeyFrame == TRUE)
                {
                    pChObj->skipFrame = FALSE;
                }
            }

            if (((pChObj->IFrameOnlyDecode) &&
                (!pBuf->isKeyFrame)) || pChObj->skipFrame)
            {
                pChObj->inBufSkipCount++;

                pChObj->inFrameUserSkipCount++;

                // Drop if not a I frame
                bufList.bufs[freeBufNum] = pBuf;
                freeBufNum++;
            }
            else
            {
                pChObj->totalInFrameCnt++;
                if (pChObj->totalInFrameCnt > DEC_LINK_STATS_START_THRESHOLD)
                {
                    pChObj->totalFrameIntervalTime +=
                        (curTime - pChObj->prevFrmRecvTime);
                }
                else
                {
                    pChObj->totalFrameIntervalTime = 0;
                    pChObj->totalProcessTime = 0;

                    DecLink_resetStatistics(pObj);
                }
                pChObj->prevFrmRecvTime = curTime;

                status = Utils_quePut(&pChObj->inQue, pBuf, BIOS_NO_WAIT);
                UTILS_assert(status == FVID2_SOK);

                pChObj->inBufQueCount++;
            }
        }

        if (freeBufNum)
        {
            bufList.numBufs = freeBufNum;
            System_putLinksEmptyBufs(pInQueParams->prevLinkId,
                                     pInQueParams->prevLinkQueId, &bufList);
            pObj->inBufPutCount += freeBufNum;
        }
    }

    return FVID2_SOK;
}

static Int32 DecLink_codecSubmitData(DecLink_Obj * pObj)
{
    DecLink_ReqObj *pReqObj;
    DecLink_ChObj *pChObj;
    UInt32 chCount,chIdIndex, numProcessCh;
    Bitstream_Buf *pInBuf;
    FVID2_Frame *pOutFrame;
    Int32 status = FVID2_EFAIL, numReqObjPerProcess;
    UInt32 tskId, i;
    static UInt32 startChID = 0;

    System_FrameInfo *pOutFrameInfo;
    UInt32 curTime = Utils_getCurTimeInMsec();

    numProcessCh = 0;
    chIdIndex    = startChID;
    for (chCount = 0; chCount < pObj->inQueInfo.numCh; chCount++,chIdIndex++)
    {
      numReqObjPerProcess = 0;
      if (chIdIndex >= pObj->inQueInfo.numCh)
          chIdIndex = 0;
      pChObj = &pObj->chObj[chIdIndex];
      if (Utils_queIsEmpty(&pObj->outObj.bufOutQue.
                           emptyQue[pChObj->allocPoolID]))
      {
          pObj->newDataProcessOnFrameFree = TRUE;
      }

      while(numReqObjPerProcess < pChObj->numReqObjPerProcess) {
        numReqObjPerProcess++;
        status =
            Utils_queGet(&pObj->reqQue, (Ptr *) & pReqObj, 1,
                         BIOS_NO_WAIT);

        if (UTILS_ISERROR(status)) {
            break;
        }
        pObj->reqQueCount++;
        UTILS_assert(DEC_LINK_MAX_REQ >= pObj->reqQueCount);

        tskId = pObj->ch2ProcessTskId[chIdIndex];
        
        if (pChObj->algObj.algCreateParams.fieldMergeDecodeEnable)
        {
           /* pReqObj->OutFrameList.numFrames should be set to 2 once         */
           /*  codec has support to consume 2 output pointers rather than     */
           /*  just one pointer with 2 contigous fields in field merged       */
           /*  interlaced decode use case.                                    */
            pReqObj->OutFrameList.numFrames = 1;
        }
        else
        {
            pReqObj->OutFrameList.numFrames = 1;
        }
        if ((status == FVID2_SOK) &&
            (pChObj->inBufQueCount) &&
            (Utils_queGetQueuedCount(&pObj->outObj.bufOutQue.emptyQue[pChObj->
                   allocPoolID]) >= pReqObj->OutFrameList.numFrames) &&
            !(Utils_queIsFull(&pObj->decProcessTsk[tskId].processQue)))
        {
            for (i=0; i<pReqObj->OutFrameList.numFrames; i++)
            {
                pOutFrame = NULL;
                status =
                    Utils_bufGetEmptyFrameExt(&pObj->outObj.bufOutQue,
                                              &pOutFrame,
                                              pObj->outObj.ch2poolMap[chIdIndex],
                                              BIOS_NO_WAIT);
                if (pOutFrame)
                {
                    declink_codec_init_outframe(pObj, chIdIndex, pOutFrame);
                    pReqObj->OutFrameList.frames[i] = pOutFrame;
                }
                else
                {
                    break;
                }
            }
            if ((status == FVID2_SOK) && (pOutFrame))
            {
                Utils_queGet(&pChObj->inQue, (Ptr *) & pInBuf, 1, BIOS_NO_WAIT);
                UTILS_assert(status == FVID2_SOK);
                pReqObj->InBuf = pInBuf;
                pChObj->inBufQueCount--;

                for (i=0; i<pReqObj->OutFrameList.numFrames; i++)
                {
                    pReqObj->OutFrameList.frames[i]->channelNum =
                                                     pInBuf->channelNum;
                    //pInBuf->timeStamp  = curTime;
                    pReqObj->OutFrameList.frames[i]->timeStamp=
                               pInBuf->timeStamp;


                    pOutFrameInfo = (System_FrameInfo *) pReqObj->OutFrameList.frames[i]->appData;
                    pOutFrameInfo->ts64  = (UInt32)pInBuf->upperTimeStamp;
                    pOutFrameInfo->ts64 <<= 32;
                    pOutFrameInfo->ts64  = pOutFrameInfo->ts64 | ((UInt32)pInBuf->lowerTimeStamp);
                }
                numProcessCh++;

                status =
                    Utils_quePut(&pObj->decProcessTsk[tskId].processQue,
                                 pReqObj, BIOS_NO_WAIT);
                UTILS_assert(status == FVID2_SOK);
                pChObj->processReqestCount++;
            }
            else
            {
                status = Utils_quePut(&pObj->reqQue, pReqObj, BIOS_NO_WAIT);
                startChID = chIdIndex;
                UTILS_assert(status == FVID2_SOK);
                pObj->reqQueCount--;
                status = FVID2_EFAIL;
                continue;
            }
        }
        else
        {
            status = Utils_quePut(&pObj->reqQue, pReqObj, BIOS_NO_WAIT);
            UTILS_assert(status == FVID2_SOK);
            pObj->reqQueCount--;
            startChID = chIdIndex;
            status = FVID2_EFAIL;
            if (Utils_queIsEmpty(&pObj->outObj.bufOutQue.
                                 emptyQue[pChObj->allocPoolID]))
            {
                pObj->newDataProcessOnFrameFree = TRUE;
            }
        }
      }
    }

    return status;
}

Int32 DecLink_codecProcessData(DecLink_Obj * pObj)
{
    Int32 status;

    pObj->newDataProcessOnFrameFree = FALSE;
    DecLink_codecQueueBufsToChQue(pObj);

    do
    {
        status = DecLink_codecSubmitData(pObj);
    } while (status == FVID2_SOK);

    return FVID2_SOK;
}

static Int32 DecLink_codecGetProcessedData(DecLink_Obj * pObj)
{
    Bitstream_BufList inBufList;
    FVID2_FrameList outFrameList;
    FVID2_FrameList outFrameSkipList;
    UInt32 chId, sendCmd;
    System_LinkInQueParams *pInQueParams;
    DecLink_ChObj *pChObj;
    DecLink_ReqObj *pReqObj;
    Int32 status, j;
    UInt32 curTime;

    sendCmd = FALSE;
    inBufList.numBufs = 0;
    inBufList.appData = NULL;
    outFrameList.numFrames = 0;
    outFrameSkipList.numFrames = 0;
    curTime = Utils_getCurTimeInMsec();

    while(!Utils_queIsEmpty(&pObj->processDoneQue)
          &&
          (inBufList.numBufs < (VIDBITSTREAM_MAX_BITSTREAM_BUFS - 1))
          &&
          (outFrameList.numFrames < (FVID2_MAX_FVID_FRAME_PTR - 1)))
    {
        status = Utils_queGet(&pObj->processDoneQue, (Ptr *) & pReqObj, 1,
                              BIOS_NO_WAIT);
        if (status != FVID2_SOK)
        {
            break;
        }

        UTILS_assert(pReqObj->InBuf != NULL);
        chId = pReqObj->InBuf->channelNum;
        pChObj = &pObj->chObj[chId];

        //if (pChObj->totalInFrameCnt > DEC_LINK_STATS_START_THRESHOLD)
        {
            if (curTime > pReqObj->InBuf->timeStamp)
            {
                pChObj->totalProcessTime +=
                     (curTime - pReqObj->InBuf->timeStamp);
            }
        }

        pChObj->getProcessedBufCount++;

        pChObj->outFrameCount++;

        inBufList.bufs[inBufList.numBufs] = pReqObj->InBuf;
        inBufList.numBufs++;

        for (j = 0; j < pReqObj->OutFrameList.numFrames; j++)
        {
            if (pReqObj->OutFrameList.frames[j])
            {
                UTILS_assert(pReqObj->InBuf->channelNum ==
                             pReqObj->OutFrameList.frames[j]->channelNum);
                UTILS_assert(pChObj->allocPoolID < UTILS_BUF_MAX_ALLOC_POOLS);


                pChObj->trickPlayObj.skipFrame = Utils_doSkipFrame(&(pChObj->trickPlayObj.frameSkipCtx));

                if (pChObj->trickPlayObj.skipFrame == TRUE)
                {
                    /* Skip the output frame */
                    outFrameSkipList.frames[outFrameSkipList.numFrames] =
                                        pReqObj->OutFrameList.frames[j];
                    outFrameSkipList.numFrames++;
                }
                else
                {
                    outFrameList.frames[outFrameList.numFrames] =
                                        pReqObj->OutFrameList.frames[j];
                    outFrameList.numFrames++;
                }
            }
        }
        status = Utils_quePut(&pObj->reqQue, pReqObj, BIOS_NO_WAIT);
        UTILS_assert(status == FVID2_SOK);
        pObj->reqQueCount--;
    }

    if (outFrameList.numFrames)
    {
        status = Utils_bufPutFullExt(&pObj->outObj.bufOutQue,
                                     &outFrameList);
        UTILS_assert(status == FVID2_SOK);
        sendCmd = TRUE;
    }

    if (outFrameSkipList.numFrames)
    {
        status = DecLink_codecFreeProcessedFrames(pObj, &outFrameSkipList);
        UTILS_assert(status == DEC_LINK_S_SUCCESS);
    }

    if (inBufList.numBufs)
    {
        /* Free input frames */
        pInQueParams = &pObj->createArgs.inQueParams;
        System_putLinksEmptyBufs(pInQueParams->prevLinkId,
                                 pInQueParams->prevLinkQueId, &inBufList);
        pObj->inBufPutCount += inBufList.numBufs;
    }

    /* Send-out the output bitbuffer */
    if (sendCmd == TRUE)
    {
        System_sendLinkCmd(pObj->createArgs.outQueParams.nextLink,
                           SYSTEM_CMD_NEW_DATA);
    }

    return FVID2_SOK;
}

Int32 DecLink_codecGetProcessedDataMsgHandler(DecLink_Obj * pObj)
{
    Int32 status;

    status = DecLink_codecGetProcessedData(pObj);
    UTILS_assert(status == FVID2_SOK);

    return DEC_LINK_S_SUCCESS;

}

static
Int32 DecLink_codecFreeInQueuedBufs(DecLink_Obj * pObj)
{
    System_LinkInQueParams *pInQueParams;
    Bitstream_BufList bufList;

    pInQueParams = &pObj->createArgs.inQueParams;
    System_getLinksFullBufs(pInQueParams->prevLinkId,
                            pInQueParams->prevLinkQueId, &bufList);
    if (bufList.numBufs)
    {
        pObj->inBufGetCount += bufList.numBufs;
        /* Free input frames */
        System_putLinksEmptyBufs(pInQueParams->prevLinkId,
                                 pInQueParams->prevLinkQueId, &bufList);
        pObj->inBufPutCount += bufList.numBufs;
    }
    return DEC_LINK_S_SUCCESS;
}

Int32 DecLink_codecStop(DecLink_Obj * pObj)
{
    Int32 rtnValue = FVID2_SOK;
    UInt32 tskId;

       #ifdef SYSTEM_DEBUG_DEC
       Vps_printf(" %d: DECODE: Stop in progress !!!\n",
            Utils_getCurTimeInMsec()
        );
       #endif

    for (tskId = 0; tskId < NUM_HDVICP_RESOURCES; tskId++)
    {
        Utils_queUnBlock(&pObj->decProcessTsk[tskId].processQue);
    }
    while (!Utils_queIsFull(&pObj->reqQue))
    {
        Utils_tskWaitCmd(&pObj->tsk, NULL, DEC_LINK_CMD_GET_PROCESSED_DATA);
        DecLink_codecGetProcessedDataMsgHandler(pObj);
    }

    DecLink_codecFreeInQueuedBufs(pObj);

       #ifdef SYSTEM_DEBUG_DEC
       Vps_printf(" %d: DECODE: Stop Done !!!\n",
            Utils_getCurTimeInMsec()
        );
       #endif

    return (rtnValue);
}

Int32 DecLink_codecDelete(DecLink_Obj * pObj)
{
    UInt32 outId, chId, tskId, i, buf_index;
    DecLink_ChObj *pChObj;
    DecLink_OutObj *pOutObj;
    Bool tilerUsed = FALSE;
    Int retVal = DEC_LINK_S_SUCCESS;

    #ifdef SYSTEM_DEBUG_DEC
    Vps_printf(" %d: DECODE: Delete in progress !!!\n",
            Utils_getCurTimeInMsec()
        );
    #endif

    pObj->state = SYSTEM_LINK_STATE_STOP;
    DecLink_codecStopPrdObj(pObj);
    for (tskId = 0; tskId < NUM_HDVICP_RESOURCES; tskId++)
    {
        DecLink_codecDeleteProcessTsk(pObj, tskId);
        Utils_queDelete(&pObj->decProcessTsk[tskId].processQue);
    }

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pChObj = &pObj->chObj[chId];

        switch (pChObj->algObj.algCreateParams.format)
        {
            case IVIDEO_H264BP:
            case IVIDEO_H264MP:
            case IVIDEO_H264HP:
                DecLinkH264_algDelete(&pChObj->algObj.u.h264AlgIfObj);
                break;

            case IVIDEO_MPEG4SP:
      case IVIDEO_MPEG4ASP:
                DecLinkMPEG4_algDelete(&pChObj->algObj.u.mpeg4AlgIfObj);
                break;				

            case IVIDEO_MJPEG:
                DecLinkJPEG_algDelete(&pChObj->algObj.u.jpegAlgIfObj);
                break;

            default:
                retVal = DEC_LINK_E_UNSUPPORTEDCODEC;
                break;
        }
        UTILS_assert(retVal == DEC_LINK_S_SUCCESS);

        Utils_queDelete(&pChObj->inQue);

        if (pChObj->totalInFrameCnt > DEC_LINK_STATS_START_THRESHOLD)
        {
            pChObj->totalInFrameCnt -= DEC_LINK_STATS_START_THRESHOLD;
        }
        #ifdef SYSTEM_DEBUG_DEC
        Vps_printf(" %d: DECODE: CH%d: "
                    "Processed Frames : %8d, "
                    "Total Process Time : %8d, "
                    "Total Frame Interval: %8d, "
                    "Dropped Frames: %8d, "
                    "FPS: %8d \n",
                    Utils_getCurTimeInMsec(),
                    chId,
                    pChObj->totalInFrameCnt,
                    pChObj->totalProcessTime,
                    pChObj->totalFrameIntervalTime,
                    pChObj->inBufSkipCount,
                    (pChObj->totalInFrameCnt) /
                    (pChObj->totalFrameIntervalTime/1000)
                 );
        #endif
    }

    Utils_queDelete(&pObj->processDoneQue);
    DecLink_codecDeleteDupObj(pObj);
    DecLink_codecDeletePrdObj(pObj);

    for (outId = 0; outId < DEC_LINK_MAX_OUT_QUE; outId++)
    {
        {
            pOutObj = &pObj->outObj;

            Utils_bufDeleteExt(&pOutObj->bufOutQue);
            buf_index = 0;
            for (i = 0; i < pOutObj->numAllocPools; i++)
            {
                if (pObj->createArgs.tilerEnable)
                {
                    tilerUsed = TRUE;
                }
                else
                {
                    Utils_memFrameFree(&pOutObj->outFormat[i],
                                       &pOutObj->outFrames[buf_index],
                                       pOutObj->outNumFrames[i]);
                    buf_index += pOutObj->outNumFrames[i];
                }
            }
        }
    }

    Utils_queDelete(&pObj->reqQue);

    if (tilerUsed)
    {
        SystemTiler_freeAll();
    }

    #ifdef SYSTEM_DEBUG_DEC
    Vps_printf(" %d: DECODE: Delete Done !!!\n",
            Utils_getCurTimeInMsec()
        );
    #endif

    return FVID2_SOK;
}

Int32 DecLink_resetStatistics(DecLink_Obj * pObj)
{
    UInt32 chId, tskId;
    DecLink_ChObj *pChObj;

  for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
  {
        pChObj = &pObj->chObj[chId];

        pChObj->inFrameRecvCount = 0;
        pChObj->inFrameUserSkipCount = 0;
        pChObj->outFrameCount = 0;
    }
    for ( tskId = 0; tskId < NUM_HDVICP_RESOURCES; tskId++)
    {
        pObj->batchStatistics[tskId].numBatchesSubmitted = 0;
        pObj->batchStatistics[tskId].aggregateBatchSize = 0;
        pObj->batchStatistics[tskId].averageBatchSize = 0;
        pObj->batchStatistics[tskId].maxAchievedBatchSize = 0;     
    }
    pObj->statsStartTime = Utils_getCurTimeInMsec();

    return 0;
}

Int32 DecLink_printStatistics (DecLink_Obj * pObj, Bool resetAfterPrint)
{
    UInt32 chId, ivaId;
    DecLink_ChObj *pChObj;
    UInt32 elaspedTime;

    elaspedTime = Utils_getCurTimeInMsec() - pObj->statsStartTime; // in msecs
    elaspedTime /= 1000; // convert to secs

    Vps_printf( " \n"
            " *** DECODE Statistics *** \n"
            " \n"
            " Elasped Time           : %d secs\n"
            " \n"
            " \n"
            " CH  | In Recv In User  Out \n"
            " Num | FPS     Skip FPS FPS \n"
            " -----------------------------------\n",
            elaspedTime
            );

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pChObj = &pObj->chObj[chId];

        Vps_printf( " %3d | %7d %8d %3d\n",
                chId,
            pChObj->inFrameRecvCount/elaspedTime,
            pChObj->inFrameUserSkipCount/elaspedTime,
            pChObj->outFrameCount/elaspedTime
             );
    }
    
    Vps_printf( " \n");
    
    Vps_printf("Multi Channel Decode Average Submit Batch Size \n");
    Vps_printf("Max Submit Batch Size : %d\n", DEC_LINK_GROUP_SUBM_MAX_SIZE);
    
    for (ivaId = 0; ivaId < NUM_HDVICP_RESOURCES; ivaId++)
    {
      Vps_printf ("IVAHD_%d Average Batch Size : %d\n", 
                  ivaId, pObj->batchStatistics[ivaId].averageBatchSize);
      Vps_printf ("IVAHD_%d Max achieved Batch Size : %d\n", 
                  ivaId, pObj->batchStatistics[ivaId].maxAchievedBatchSize);                  
    }    

    Vps_printf( " \n");

    if(resetAfterPrint)
    {
        DecLink_resetStatistics(pObj);
    }

   return FVID2_SOK;
}

static
Int32 DecLink_dupFrame(DecLink_Obj * pObj, FVID2_Frame * pOrgFrame,
                       FVID2_Frame ** ppDupFrame)
{
    Int status = DEC_LINK_S_SUCCESS;
    FVID2_Frame *pFrame;
    System_FrameInfo *pFrameInfo, *pOrgFrameInfo;

    status =
        Utils_queGet(&pObj->dupObj.dupQue, (Ptr *) & pFrame, 1, BIOS_NO_WAIT);
    UTILS_assert(status == FVID2_SOK);
    UTILS_assert(pFrame != NULL);
    pFrameInfo = (System_FrameInfo *) pFrame->appData;
    UTILS_assert(pFrameInfo != NULL);
    while (((System_FrameInfo *) pOrgFrame->appData)->pVdecOrgFrame != NULL)
    {
        pOrgFrame = ((System_FrameInfo *) pOrgFrame->appData)->pVdecOrgFrame;
    }
    pOrgFrameInfo = pOrgFrame->appData;
    memcpy(pFrame, pOrgFrame, sizeof(*pOrgFrame));
    pOrgFrameInfo = pOrgFrame->appData;
    memcpy(pFrameInfo, pOrgFrameInfo, sizeof(*pOrgFrameInfo));

    pFrame->appData = pFrameInfo;
    pFrameInfo->pVdecOrgFrame = pOrgFrame;
    UTILS_assert(pOrgFrameInfo->vdecRefCount <= DEC_LINK_MAX_DUP_PER_FRAME);
    pOrgFrameInfo->vdecRefCount++;
    *ppDupFrame = pFrame;

    return status;
}

Int32 DecLink_codecDisableChannel(DecLink_Obj * pObj,
                              DecLink_ChannelInfo* params)
{
    Int32 status = DEC_LINK_S_SUCCESS;
    DecLink_ChObj *pChObj;
    UInt key;

    key = Hwi_disable();
    pChObj = &pObj->chObj[params->chId];
    pChObj->disableChn = TRUE;
    Hwi_restore(key);

    return (status);
}

Int32 DecLink_codecEnableChannel(DecLink_Obj * pObj,
                              DecLink_ChannelInfo* params)
{
    Int32 status = DEC_LINK_S_SUCCESS;
    DecLink_ChObj *pChObj;
    UInt key;

    key = Hwi_disable();
    pChObj = &pObj->chObj[params->chId];
    pChObj->disableChn = FALSE;
    Hwi_restore(key);

    return (status);
}


Int32 DecLink_setTPlayConfig(DecLink_Obj * pObj,
                              DecLink_TPlayConfig* params)
{
    Int32 status = DEC_LINK_S_SUCCESS;
    DecLink_ChObj *pChObj;
    UInt key;

    key = Hwi_disable();
    pChObj = &pObj->chObj[params->chId];
    pChObj->trickPlayObj.frameSkipCtx.inputFrameRate = params->inputFps;
    pChObj->trickPlayObj.frameSkipCtx.outputFrameRate = params->targetFps;
    pChObj->trickPlayObj.frameSkipCtx.firstTime = TRUE;

    Vps_printf("\r\n DecLink_setTPlayConfig : Ch :%d InputputFrameRate :%d, trickPlay outputFrameRate: %d ",
         params->chId,
         pChObj->trickPlayObj.frameSkipCtx.inputFrameRate,
         pChObj->trickPlayObj.frameSkipCtx.outputFrameRate);

    Hwi_restore(key);

    return (status);
}

static Int32 DecLink_PrepareBatch  (DecLink_Obj *pObj, UInt32 tskId, 
                                   DecLink_ReqObj *pReqObj, 
                                   DecLink_ReqBatch *pReqObjBatch)
{
  Int32 channelId, newObjChannelId, codecClassSwitch = 0;
  Int32 status = FVID2_SOK;
  UInt32 contentType;
#ifdef SYSTEM_DEBUG_MULTI_CHANNEL_DEC
  Int32 i;
#endif

  Bool batchPreperationDone = FALSE;
  DecLink_ReqObj *pNewReqObj;
  DecLink_ChObj *pChObj;
  Int32 outputFrameWidth, maxBatchSize;
  

  
  /*Reset the submitted flag at the start of batch preperation*/
  pReqObjBatch->channelSubmittedFlag = 0x0;
  pReqObjBatch->codecSubmittedFlag = 0x0;
  pReqObjBatch->numReqObjsInBatch = 0;

  pReqObjBatch->pReqObj[pReqObjBatch->numReqObjsInBatch++] = pReqObj;

  channelId = pReqObj->InBuf->channelNum;
  
  contentType =
           Utils_encdecMapFVID2XDMContentType(pObj->inQueInfo.chInfo[channelId].
                                              scanFormat);  
  pChObj = &pObj->chObj[channelId];
  outputFrameWidth = pObj->inQueInfo.chInfo[channelId].width;
  pChObj->numBufsInCodec += pReqObj->OutFrameList.numFrames;
  
  /*Since this is the first ReqList in the Batch, the channel submit and codec 
     submit bits wont have been set a-priori.*/
  pReqObjBatch->channelSubmittedFlag = pReqObjBatch->channelSubmittedFlag | 
                                        (0x1 << channelId);
  
  maxBatchSize = DEC_LINK_GROUP_SUBM_MAX_SIZE;
  
  
  if ((UTILS_ENCDEC_RESOLUTION_CLASS_1080P_WIDTH >= outputFrameWidth) &&
      (UTILS_ENCDEC_RESOLUTION_CLASS_720P_WIDTH < outputFrameWidth))
  {
    /*If the element has width between 1920 and 1280, 
      only 1 channels possible per Batch*/
    maxBatchSize = 1;
  }
  else if ((UTILS_ENCDEC_RESOLUTION_CLASS_720P_WIDTH >= outputFrameWidth) &&
           (UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH < outputFrameWidth))
  {
    /*If the element has width between 1280 and (including )480, Batch can have 
      max 14 channels*/
    maxBatchSize = MIN (14,DEC_LINK_GROUP_SUBM_MAX_SIZE);
  }
  else if (UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH >= outputFrameWidth)
  {
    /*If the element has width between 1280 and (including )1920, Batch can have 
      only 2 channels*/
    maxBatchSize = MIN (16,DEC_LINK_GROUP_SUBM_MAX_SIZE);
  }
  else
  {
    UTILS_assert(FALSE);
  }
  
  
  /*Set the flag for which codec class this REqObject belongs to.*/
  switch (pChObj->algObj.algCreateParams.format)
  {
      case IVIDEO_H264BP:
      case IVIDEO_H264MP:
      case IVIDEO_H264HP:
          pReqObjBatch->codecSubmittedFlag = pReqObjBatch->codecSubmittedFlag |
                                              (0x1 << 
                                               DEC_LINK_GROUP_CODEC_CLASS_H264);
          break;
      case IVIDEO_MJPEG:
          pReqObjBatch->codecSubmittedFlag = pReqObjBatch->codecSubmittedFlag |
                                              (0x1 << 
                                               DEC_LINK_GROUP_CODEC_CLASS_JPEG);
          break;
      case IVIDEO_MPEG4ASP:
      case IVIDEO_MPEG4SP:
          pReqObjBatch->codecSubmittedFlag = pReqObjBatch->codecSubmittedFlag |
                                              (0x1 << 
                                               DEC_LINK_GROUP_CODEC_CLASS_MPEG4);
          break;
      default:
          UTILS_assert(FALSE);
  }
                                        
  while (FALSE == batchPreperationDone)  
  {
    if (pReqObjBatch->numReqObjsInBatch >= 
        MIN (DEC_LINK_GROUP_SUBM_MAX_SIZE, maxBatchSize))
    {
      /*The number of Request Lists has exceeded the maximum batch size 
        supported be the codec.*/
      #ifdef SYSTEM_DEBUG_MULTI_CHANNEL_DEC    
      Vps_printf ("DEC : IVAHDID : %d Number of Req Objs exceeded limit: %d\n", tskId,
                  maxBatchSize);
      #endif        
      batchPreperationDone = TRUE;
      continue;
    }
    
    if (Utils_queIsEmpty (&pObj->decProcessTsk[tskId].processQue))
    {
      /*There are no more request Objects to be dequeued. Batch generation done*/
      #ifdef SYSTEM_DEBUG_MULTI_CHANNEL_DEC      
      Vps_printf ("DEC : IVAHDID : %d Incoming Queue is empty. Batch generation complete!!",
                  tskId);
      #endif
      batchPreperationDone = TRUE;
      continue;

    }
    
    /*Peek at the next Request Obj in the Process Queue*/    
    status = Utils_quePeek(&pObj->decProcessTsk[tskId].processQue,
                           (Ptr *) &pNewReqObj);
    
    if (status != FVID2_SOK)
      return status;                           
    
    
    newObjChannelId = pNewReqObj->InBuf->channelNum;
    outputFrameWidth = pObj->inQueInfo.chInfo[newObjChannelId].width;
    
    if ((UTILS_ENCDEC_RESOLUTION_CLASS_1080P_WIDTH >= outputFrameWidth) &&
        (UTILS_ENCDEC_RESOLUTION_CLASS_720P_WIDTH < outputFrameWidth))
    {
      /*If the element has width between 1920 and 1280, 
        only 1 channels possible per Batch. As we already have an element in, 
        break out*/
      batchPreperationDone = TRUE;
      continue;
    }
    else if ((UTILS_ENCDEC_RESOLUTION_CLASS_720P_WIDTH >= outputFrameWidth) &&
             (UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH < outputFrameWidth))
    {
      /*If the element has width between 1280 and (including )480, Batch can have 
        max 14 channels*/
      if (pReqObjBatch->numReqObjsInBatch >= 
          MIN (14,DEC_LINK_GROUP_SUBM_MAX_SIZE))
      {
        batchPreperationDone = TRUE;
        continue;      
      }
      else
      {
        maxBatchSize = MIN (maxBatchSize, 14);  
      }
    }
    else if (UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH >= outputFrameWidth)
    {
      /*If the element has width between 1280 and (including )1920, Batch can have 
        only 2 channels*/
      if (pReqObjBatch->numReqObjsInBatch >= 
          MIN (16,DEC_LINK_GROUP_SUBM_MAX_SIZE))
      {
        batchPreperationDone = TRUE;
        continue;      
      }
      else
      {
        maxBatchSize = MIN (maxBatchSize, 16);  
      }
    }
    else
    {
      UTILS_assert(FALSE);
    }
    
    /*If the new element's Content type doesnt match that of the first ReqObj,
      stop the Batch preperation. Each batch can have either all progressive or
      all interlaced channels*/ 
    if (contentType != Utils_encdecMapFVID2XDMContentType(pObj->inQueInfo.chInfo
                                                          [newObjChannelId].
                                                          scanFormat))
    {
      #ifdef SYSTEM_DEBUG_MULTI_CHANNEL_DEC      
      Vps_printf ("DEC : IVAHDID : %d Interlaced or Progressive switch happened!!\n", 
                  tskId);
      #endif           
      batchPreperationDone = TRUE;
      continue;     
    }
    
    /*Check if the channel has already been inserted in the batch*/      
    if (pReqObjBatch->channelSubmittedFlag & (0x1 << newObjChannelId))
    {
      /*Codec doesnt support multiple entries of the same channel in the same
        multi process call. So the batch generation ends here.*/
      #ifdef SYSTEM_DEBUG_MULTI_CHANNEL_DEC      
      Vps_printf ("DEC : IVAHDID : %d Channel repeated within Batch!!\n", tskId);
      #endif           
      batchPreperationDone = TRUE;
      continue;    
    }
    else
    {
      /*This is a new channel so set the bit for this channel*/
      pReqObjBatch->channelSubmittedFlag = pReqObjBatch->channelSubmittedFlag |
                                           (0x1 << newObjChannelId);
    }

    /*Check if there is a codec switch. If yes, batch generation is completed.*/
    pChObj = &pObj->chObj[newObjChannelId];
    
    /*Check the flag for which codec class this Request Object belongs to.*/
    switch (pChObj->algObj.algCreateParams.format)
    {
        case IVIDEO_H264BP:
        case IVIDEO_H264MP:
        case IVIDEO_H264HP:
            codecClassSwitch = pReqObjBatch->codecSubmittedFlag &
                                                (0x1 << 
                                                 DEC_LINK_GROUP_CODEC_CLASS_H264);
            break;
        case IVIDEO_MJPEG:
            codecClassSwitch = pReqObjBatch->codecSubmittedFlag &
                                                (0x1 << 
                                                 DEC_LINK_GROUP_CODEC_CLASS_JPEG);
            break;
        case IVIDEO_MPEG4ASP:
        case IVIDEO_MPEG4SP:
            codecClassSwitch = pReqObjBatch->codecSubmittedFlag &
                                                (0x1 << 
                                                 DEC_LINK_GROUP_CODEC_CLASS_MPEG4);        
             break;
        default:
            UTILS_assert(FALSE);
    }    
    
    if (! codecClassSwitch)
    {
      /*A codec switch from JPEG to H264 or vice-versa has happened and this 
        Request object cannot be part of the batch to be submitted. 
        Batch generation done.*/
      #ifdef SYSTEM_DEBUG_MULTI_CHANNEL_DEC      
      Vps_printf ("DEC : IVAHDID : %d Codec Switch occured!!. Batch generation complete!!",
                  tskId);
      #endif         
      batchPreperationDone = TRUE;
      continue;      
    }
    
    /*Now that the Request Obj is eligible to be part of the Batch, include it.
     */
    status = Utils_queGet(&pObj->decProcessTsk[tskId].processQue,
                          (Ptr *) &pNewReqObj, 1, BIOS_WAIT_FOREVER);     
    
    pReqObjBatch->pReqObj[pReqObjBatch->numReqObjsInBatch++] = pNewReqObj;
    pChObj->numBufsInCodec += pNewReqObj->OutFrameList.numFrames;
      
  }
  
  if (FVID2_SOK == status)
  {
    /*Print Debug details of the Batch created*/
    #ifdef SYSTEM_DEBUG_MULTI_CHANNEL_DEC
    Vps_printf("DEC : IVAHDID : %d Batch creation ... DONE !!!\n", tskId);
    Vps_printf("DEC : IVAHDID : %d Number of Req Objs in Batch : %d\n", 
               tskId, pReqObjBatch->numReqObjsInBatch);
    Vps_printf ("DEC : IVAHDID : %d Channels included in Batch:\n", tskId);
    for ( i = 0; i < pReqObjBatch->numReqObjsInBatch; i++)
    {
      Vps_printf ("DEC : IVAHDID : %d %d\n", tskId, 
                  pReqObjBatch->pReqObj[i]->InBuf->channelNum);
    }

    #endif    
  }

  return status;
}
Int32 DecLink_printBufferStatus (DecLink_Obj * pObj)
{
    Uint8 str[256];
    
    Vps_rprintf(
        " \n"
        " *** Decode Statistics *** \n"
        "\n %d: DEC: Rcvd from prev = %d, Returned to prev = %d\r\n",
         Utils_getCurTimeInMsec(), pObj->inBufGetCount, pObj->inBufPutCount);

    sprintf ((char *)str, "DEC Out ");
    Utils_bufExtPrintStatus(str, &pObj->outObj.bufOutQue);
    return 0;
}

static Int32 decLink_map_displayDelay2CodecParam(Int32 displayDelay)
{
    Int32 VdecMaxDisplayDelay;

    if ((displayDelay >= IVIDDEC3_DISPLAY_DELAY_AUTO)
        &&
        (displayDelay <= IVIDDEC3_DISPLAY_DELAY_16))
    {
        VdecMaxDisplayDelay =  displayDelay;
    }
    else
    {
       Vps_printf("DECLINK: Invalid param passed for MaxDisplayDelay param [%d] "
                  "Forcing to default value [%d]\n",
                  displayDelay,
                  IVIDDEC3_DECODE_ORDER);
       VdecMaxDisplayDelay =  IVIDDEC3_DECODE_ORDER;
    }

    return VdecMaxDisplayDelay;
}

