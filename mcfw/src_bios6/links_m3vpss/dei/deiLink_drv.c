/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2009 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

#include <xdc/std.h>
#include <mcfw/interfaces/link_api/system_tiler.h>
#include "deiLink_priv.h"

// #define DEI_LINK_QUEUE_REQ

#ifdef TI_814X_BUILD
static Int32 DeiLink_drvReleaseContextField(DeiLink_Obj * pObj);
#endif                                                     /* TI_814X_BUILD */

Int32 DeiLink_drvFvidCb(FVID2_Handle handle, Ptr appData, Ptr reserved)
{
    DeiLink_Obj *pObj = (DeiLink_Obj *) appData;

#ifdef DEI_LINK_QUEUE_REQ
    Utils_tskSendCmd(&pObj->tsk, DEI_LINK_CMD_GET_PROCESSED_DATA);
#else
    Semaphore_post(pObj->complete);
#endif
    return FVID2_SOK;
}

Int32 DeiLink_drvFvidErrCb(FVID2_Handle handle,
                           Ptr appData, Ptr errList, Ptr reserved)
{
    return FVID2_SOK;
}

Int32 DeiLink_drvCreateReqObj(DeiLink_Obj * pObj)
{
    Int32 status;
    UInt32 reqId;

    memset(pObj->reqObj, 0, sizeof(pObj->reqObj));

    status = Utils_queCreate(&pObj->reqQue,
                             DEI_LINK_MAX_REQ,
                             pObj->reqQueMem, UTILS_QUE_FLAG_NO_BLOCK_QUE);
    UTILS_assert(status == FVID2_SOK);

    pObj->reqQueCount = 0;
    pObj->isReqPend = FALSE;

    for (reqId = 0; reqId < DEI_LINK_MAX_REQ; reqId++)
    {
        status =
            Utils_quePut(&pObj->reqQue, &pObj->reqObj[reqId], BIOS_NO_WAIT);
        UTILS_assert(status == FVID2_SOK);
    }

    pObj->reqNumOutLists = 1;

    switch (pObj->drvInstId)
    {

#ifdef TI_816X_BUILD
        case VPS_M2M_INST_MAIN_DEIH_SC1_SC3_WB0_VIP0:
        case VPS_M2M_INST_AUX_DEI_SC2_SC4_WB1_VIP1:
            pObj->reqNumOutLists = 2;
            break;
#endif                                                     /* TI_816X_BUILD */

#ifdef TI_814X_BUILD
        case VPS_M2M_INST_MAIN_DEI_SC1_SC3_WB0_VIP0:
        case VPS_M2M_INST_AUX_SC2_SC4_WB1_VIP1:
            pObj->reqNumOutLists = 2;
            break;
#endif                                                     /* TI_814X_BUILD */
    }

    return FVID2_SOK;
}

Int32 DeiLink_drvCreateOutObj(DeiLink_Obj * pObj)
{
    DeiLink_OutObj *pOutObj;
    Int32 status = FVID2_SOK;
    UInt32 frameId, outId, chId;
    FVID2_Format *pFormat;
    UInt32 maxOutWidth = 0u, maxOutHeight = 0u;
    System_LinkChInfo *pInChInfo;
    System_LinkChInfo *pOutChInfo;

    memset(&pObj->outFrameDrop, 0, sizeof(pObj->outFrameDrop));

    for (outId = 0u; outId < DEI_LINK_MAX_OUT_QUE; outId++)
    {
        if (pObj->createArgs.enableOut[outId])
        {

            maxOutWidth = pObj->inQueInfo.chInfo[0].width;
            pInChInfo = &pObj->inQueInfo.chInfo[0u];
            if (pInChInfo->scanFormat == FVID2_SF_INTERLACED && !pObj->createArgs.enableDeiForceBypass)
            {
                /* if input is interlaced and DEI is NOT bypassed */
                maxOutHeight = pObj->inQueInfo.chInfo[0].height * 2;
            }
            else
            {
                maxOutHeight = pObj->inQueInfo.chInfo[0].height;
            }

            if (pObj->createArgs.scaleMode == DEI_SCALE_MODE_RATIO)
            {
                maxOutHeight = (maxOutHeight * pObj->createArgs.outScaleFactor[outId].ratio.heightRatio.numerator) / pObj->createArgs.outScaleFactor[outId].ratio.heightRatio.denominator;
                maxOutWidth = (maxOutWidth * pObj->createArgs.outScaleFactor[outId].ratio.widthRatio.numerator) / pObj->createArgs.outScaleFactor[outId].ratio.widthRatio.denominator;
            }
            else
            {
                maxOutHeight = pObj->createArgs.outScaleFactor[outId].absoluteResolution.outHeight;
                maxOutWidth = pObj->createArgs.outScaleFactor[outId].absoluteResolution.outWidth;
            }
            maxOutWidth = VpsUtils_floor(maxOutWidth, VPS_BUFFER_ALIGNMENT);

            pOutObj = &pObj->outObj[outId];

            pOutObj->outNumFrames =
            pObj->inQueInfo.numCh * DEI_LINK_MAX_OUT_FRAMES_PER_CH;


            UTILS_assert(pObj->inQueInfo.numCh <= DEI_LINK_MAX_CH);
            UTILS_assert(pOutObj->outNumFrames <= DEI_LINK_MAX_OUT_FRAMES);

            status = Utils_bufCreate(&pOutObj->bufOutQue, TRUE, FALSE);
            UTILS_assert(status == FVID2_SOK);

            pFormat = &pOutObj->outFormat;

            pFormat->channelNum = 0;
            pFormat->width = maxOutWidth;
            pFormat->height = maxOutHeight;
            pFormat->fieldMerged[0] = FALSE;
            pFormat->fieldMerged[1] = FALSE;
            pFormat->fieldMerged[2] = FALSE;

            pFormat->pitch[0] =
                VpsUtils_align(pFormat->width, VPS_BUFFER_ALIGNMENT);

            switch (outId)
            {
                case DEI_LINK_OUT_QUE_DEI_SC:
                    pFormat->dataFormat = FVID2_DF_YUV422I_YUYV;
                    pFormat->pitch[0] *= 2;
                    pFormat->pitch[1] = pFormat->pitch[0];
                    break;
                case DEI_LINK_OUT_QUE_VIP_SC:
                    if (pObj->createArgs.setVipScYuv422Format)
                    {
                        pFormat->dataFormat = FVID2_DF_YUV422I_YUYV;
                        pFormat->pitch[0] *= 2;
                    }
                    else
                    {
                        pFormat->dataFormat = FVID2_DF_YUV420SP_UV;
                    }
                    pFormat->pitch[1] = pFormat->pitch[0];
                    if (pObj->createArgs.tilerEnable &&
                        pFormat->dataFormat == FVID2_DF_YUV420SP_UV)
                    {
                        pFormat->pitch[0] = VPSUTILS_TILER_CNT_8BIT_PITCH;
                        pFormat->pitch[1] = VPSUTILS_TILER_CNT_16BIT_PITCH;
                    }
                    break;
                case DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT:
                    pFormat->dataFormat = FVID2_DF_YUV420SP_UV;
                    pFormat->pitch[0] *= 1;
                    pFormat->pitch[1] = pFormat->pitch[0];
                    break;
            }
            pFormat->pitch[2] = 0;

            pFormat->scanFormat = FVID2_SF_PROGRESSIVE;
            pFormat->bpp = FVID2_BPP_BITS16;
            pFormat->reserved = NULL;

            if(DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT == outId)
            {
                #ifdef SYSTEM_DEBUG_MEMORY_ALLOC
                Vps_printf("DEI:ALLOCINFO:OUTID[%d]/FMT[%d]/PITCH[%d]/HEIGHT[%d]/NUMBUFS[%d]",
                           outId,
                           pOutObj->outFormat.dataFormat,
                           pOutObj->outFormat.pitch[0],
                           pOutObj->outFormat.height,
                           pOutObj->outNumFrames);
                #endif
                status = Utils_memFrameAlloc(&pOutObj->outFormat,
                                             pOutObj->outFrames,
                                             pOutObj->outNumFrames);
            }
            else
            {
                if (pObj->createArgs.tilerEnable &&
                   (outId == DEI_LINK_OUT_QUE_VIP_SC) &&
                    pFormat->dataFormat == FVID2_DF_YUV420SP_UV)
                {
                    status = Utils_tilerFrameAlloc(&pOutObj->outFormat,
                                                   pOutObj->outFrames,
                                                   pOutObj->outNumFrames);
                }
                else
                {
                    #ifdef SYSTEM_DEBUG_MEMORY_ALLOC
                    Vps_printf("DEI:ALLOCINFO:OUTID[%d]/FMT[%d]/PITCH[%d]/HEIGHT[%d]/NUMBUFS[%d]",
                             outId,
                             pOutObj->outFormat.dataFormat,
                             pOutObj->outFormat.pitch[0],
                             pOutObj->outFormat.height,
                             pOutObj->outNumFrames);
                    #endif
                    status = Utils_memFrameAlloc(&pOutObj->outFormat,
                                                 pOutObj->outFrames,
                                                 pOutObj->outNumFrames);
                }
            }
            UTILS_assert(status == FVID2_SOK);

            for (frameId = 0; frameId < pOutObj->outNumFrames; frameId++)
            {
                status =
                    Utils_bufPutEmptyFrame(&pOutObj->bufOutQue,
                                           &pOutObj->outFrames[frameId]);
                UTILS_assert(status == FVID2_SOK);

                pOutObj->frameInfo[frameId].rtChInfoUpdate = FALSE;
                pOutObj->outFrames[frameId].appData = &pOutObj->frameInfo[frameId];

#ifdef SYSTEM_VERBOSE_PRINTS
                Vps_rprintf(" %d: DEI: OUT%d: %2d: 0x%08x, %d x %d, %08x B\n",
                            Utils_getCurTimeInMsec(),
                            outId,
                            frameId, pOutObj->outFrames[frameId].addr[0][0],
                            pOutObj->outFormat.pitch[0] / 2,
                        pOutObj->outFormat.height,
                        pOutObj->outFormat.height *
                        pOutObj->outFormat.pitch[0]);
#endif

            }
        }
#ifdef SYSTEM_VERBOSE_PRINTS

        if (outId == 0)
        {
            Vps_rprintf(" %d: DEI: 0x%08x %08x B\n",
                        Utils_getCurTimeInMsec(),
                        pOutObj->outFrames[0].addr[0][0],
                        pOutObj->outFormat.height *
                        pOutObj->outFormat.pitch[0] * frameId);
        }
#endif
    }

    pObj->info.numQue = DEI_LINK_MAX_OUT_QUE;
    for (outId = 0u; outId < DEI_LINK_MAX_OUT_QUE; outId++)
    {
        pObj->info.queInfo[outId].numCh = 0;
        if (TRUE == pObj->createArgs.enableOut[outId])
        {
            pObj->info.queInfo[outId].numCh = pObj->inQueInfo.numCh;
        }
    }

    for (chId = 0u; chId < pObj->inQueInfo.numCh; chId++)
    {
        pInChInfo = &pObj->inQueInfo.chInfo[chId];

        for (outId = 0u; outId < DEI_LINK_MAX_OUT_QUE; outId++)
        {
            if (TRUE == pObj->createArgs.enableOut[outId])
            {
                pOutChInfo = &pObj->info.queInfo[outId].chInfo[chId];

                pFormat = &pObj->outObj[outId].outFormat;

                pOutChInfo->memType = VPS_VPDMA_MT_NONTILEDMEM;
                if (outId == DEI_LINK_OUT_QUE_VIP_SC &&
                    pObj->createArgs.tilerEnable &&
                    pFormat->dataFormat == FVID2_DF_YUV420SP_UV)
                {
                    pOutChInfo->memType =
                        (Vps_VpdmaMemoryType) VPS_VPDMA_MT_TILEDMEM;
                }
                pOutChInfo->dataFormat = (FVID2_DataFormat) pFormat->dataFormat;

                /* Out Width / Height can be derived from pFormat calculated in previous for loop;
                             *  still this is per channel, hence use pInChInfo and recalculate
                             */
                pOutChInfo->width = pInChInfo->width;
                if (pInChInfo->scanFormat == FVID2_SF_INTERLACED && !pObj->createArgs.enableDeiForceBypass)
                {
                    pOutChInfo->height = pInChInfo->height * 2u;
                }
                else
                {
                    pOutChInfo->height = pInChInfo->height;
                }

                if (pObj->createArgs.scaleMode == DEI_SCALE_MODE_RATIO)
                {
                    pOutChInfo->height = (pOutChInfo->height * pObj->createArgs.outScaleFactor[outId].ratio.heightRatio.numerator) / pObj->createArgs.outScaleFactor[outId].ratio.heightRatio.denominator;
                    pOutChInfo->width = (pOutChInfo->width * pObj->createArgs.outScaleFactor[outId].ratio.widthRatio.numerator) / pObj->createArgs.outScaleFactor[outId].ratio.widthRatio.denominator;
                }
                else
                {
                    pOutChInfo->height = pObj->createArgs.outScaleFactor[outId].absoluteResolution.outHeight;
                    pOutChInfo->width = pObj->createArgs.outScaleFactor[outId].absoluteResolution.outWidth;
                }
                pOutChInfo->width = VpsUtils_floor(pOutChInfo->width, VPS_BUFFER_ALIGNMENT);

                pOutChInfo->startX = 0;
                pOutChInfo->startY = 0;
                pOutChInfo->pitch[0] = pFormat->pitch[0];
                pOutChInfo->pitch[1] = pFormat->pitch[1];
                pOutChInfo->pitch[2] = pFormat->pitch[2];
                pOutChInfo->scanFormat = (FVID2_ScanFormat) pFormat->scanFormat;
            }
        }
    }

    return (status);
}

Int32 DeiLink_drvCreateChObj(DeiLink_Obj * pObj, UInt32 chId)
{
    DeiLink_ChObj *pChObj;
    System_LinkChInfo *pInChInfo;
    System_LinkChInfo *pOutChInfo;
    Vps_M2mDeiChParams *pDrvChParams;
    FVID2_Format *pFormat;
    Int32 status = FVID2_SOK;
    Bool deiBypass;
    UInt32 outId;

    pChObj = &pObj->chObj[chId];

    status = Utils_bufCreate(&pChObj->inQue, FALSE, FALSE);
    UTILS_assert(status == FVID2_SOK);

    pChObj->nextFid = 0;

    pInChInfo = &pObj->inQueInfo.chInfo[chId];

    if (pInChInfo->scanFormat == FVID2_SF_INTERLACED && !pObj->createArgs.enableDeiForceBypass)
    {
        deiBypass = FALSE;
    }
    else
    {
        deiBypass = TRUE;
    }

    pDrvChParams = &pObj->drvChArgs[chId];

    pFormat = &pDrvChParams->inFmt;

    pFormat->channelNum = chId;
    pFormat->width = pInChInfo->width;
    pFormat->height = pInChInfo->height;

    pFormat->fieldMerged[0] = FALSE;
    if (pInChInfo->scanFormat == FVID2_SF_INTERLACED && !pObj->createArgs.enableDeiForceBypass)
        pFormat->fieldMerged[0] = TRUE;
    pFormat->fieldMerged[1] = pFormat->fieldMerged[0];
    pFormat->fieldMerged[2] = pFormat->fieldMerged[0];
    pFormat->pitch[0] = pInChInfo->pitch[0];
    pFormat->pitch[1] = pInChInfo->pitch[1];
    pFormat->pitch[2] = pInChInfo->pitch[2];
    pFormat->dataFormat = pInChInfo->dataFormat;
    pFormat->scanFormat = pInChInfo->scanFormat;
    if (pObj->createArgs.enableDeiForceBypass)
        pFormat->scanFormat = FVID2_SF_PROGRESSIVE;
    pFormat->bpp = FVID2_BPP_BITS16;
    pFormat->reserved = NULL;

    if(pObj->createArgs.enableLineSkipSc && pInChInfo->memType == VPS_VPDMA_MT_NONTILEDMEM)
    {
        /* half the height and double the pitch possible only when input is non-tiled */
        pFormat->pitch[0] *= 2;
        pFormat->pitch[1] *= 2;
        pFormat->height /= 2;
    }

    pDrvChParams->inMemType = pInChInfo->memType;
    pDrvChParams->outMemTypeDei = VPS_VPDMA_MT_NONTILEDMEM;
    pDrvChParams->outMemTypeVip = VPS_VPDMA_MT_NONTILEDMEM;
    pDrvChParams->drnEnable = FALSE;
    pDrvChParams->comprEnable = pObj->createArgs.comprEnable;

    pChObj->curFrameNum = 0;
    pChObj->frameSkipCount[DEI_LINK_OUT_QUE_DEI_SC] = 0;
    pChObj->frameSkipCount[DEI_LINK_OUT_QUE_VIP_SC] = 0;
    pChObj->frameSkipCount[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT] = 0;

    for (outId=0; outId<DEI_LINK_MAX_OUT_QUE; outId++)
    {
        pChObj->frameSkipCtx[outId].firstTime = TRUE;
        pChObj->frameSkipCtx[outId].inputFrameRate = pObj->createArgs.inputFrameRate[outId];
        pChObj->frameSkipCtx[outId].outputFrameRate = pObj->createArgs.outputFrameRate[outId];
    }

    for (outId = 0u; outId < DEI_LINK_MAX_OUT_QUE; outId++)
    {
        pChObj->enableOut[outId] = FALSE;
        if (TRUE == pObj->createArgs.enableOut[outId])
        {
            pChObj->enableOut[outId] = TRUE;

            pOutChInfo = &pObj->info.queInfo[outId].chInfo[chId];

            pFormat = &pChObj->outFormat[outId];

            if (pInChInfo->width < pOutChInfo->width
                || pInChInfo->height < pOutChInfo->height)
            {
                pObj->loadUpsampleCoeffs = TRUE;
            }

            pFormat->channelNum = chId;
            pFormat->width = pOutChInfo->width;
            pFormat->height = pOutChInfo->height;
            pFormat->fieldMerged[0] = FALSE;
            pFormat->fieldMerged[1] = FALSE;
            pFormat->fieldMerged[2] = FALSE;
            pFormat->pitch[0] = pOutChInfo->pitch[0];
            pFormat->pitch[1] = pOutChInfo->pitch[1];
            pFormat->pitch[2] = pOutChInfo->pitch[2];
            pFormat->dataFormat = pOutChInfo->dataFormat;
            pFormat->scanFormat = pOutChInfo->scanFormat;
            pFormat->bpp = FVID2_BPP_BITS16;
            pFormat->reserved = NULL;

            /* Initilaize the rtparm ouput resolution from outObj */
            pChObj->chRtOutInfoUpdate[outId] = FALSE;
            pChObj->chRtOutInfoUpdateDropCnt = 0;

            pChObj->vipRtOutFrmPrm[outId].width =
                    pObj->outObj[outId].outFormat.width;
            pChObj->vipRtOutFrmPrm[outId].height =
                    pObj->outObj[outId].outFormat.height;
            pChObj->vipRtOutFrmPrm[outId].pitch[0] =
                    pObj->outObj[outId].outFormat.pitch[0];
            pChObj->vipRtOutFrmPrm[outId].pitch[1] =
                    pObj->outObj[outId].outFormat.pitch[1];
            pChObj->vipRtOutFrmPrm[outId].pitch[2] =
                    pObj->outObj[outId].outFormat.pitch[2];
            pChObj->vipRtOutFrmPrm[outId].memType = pOutChInfo->memType;

            pChObj->vipRtOutFrmPrm[outId].dataFormat = pOutChInfo->dataFormat;

            pChObj->deiRtOutFrmPrm[outId].width =
                    pObj->outObj[outId].outFormat.width;
            pChObj->deiRtOutFrmPrm[outId].height =
                    pObj->outObj[outId].outFormat.height;
            pChObj->deiRtOutFrmPrm[outId].pitch[0] =
                    pObj->outObj[outId].outFormat.pitch[0];
            pChObj->deiRtOutFrmPrm[outId].pitch[1] =
                    pObj->outObj[outId].outFormat.pitch[1];
            pChObj->deiRtOutFrmPrm[outId].pitch[2] =
                    pObj->outObj[outId].outFormat.pitch[2];
            pChObj->deiRtOutFrmPrm[outId].memType = pOutChInfo->memType;

            pChObj->deiRtOutFrmPrm[outId].dataFormat = pOutChInfo->dataFormat;

            if (outId == DEI_LINK_OUT_QUE_DEI_SC)
            {
                pDrvChParams->outMemTypeDei = pOutChInfo->memType;
            }
            if (outId == DEI_LINK_OUT_QUE_VIP_SC)
            {
                pDrvChParams->outMemTypeVip = pOutChInfo->memType;
            }
            if (outId == DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT)
            {
                pDrvChParams->outMemTypeVip = pOutChInfo->memType;
            }

        }

        pChObj->scCfg[outId].bypass = FALSE;
        pChObj->scCfg[outId].nonLinear = FALSE;
        pChObj->scCfg[outId].stripSize = 0;
        pChObj->scCfg[outId].vsType = VPS_SC_VST_POLYPHASE;

        pChObj->scCropConfig[outId].cropStartX = pInChInfo->startX;
        pChObj->scCropConfig[outId].cropWidth = pInChInfo->width;
        if (pInChInfo->scanFormat == FVID2_SF_INTERLACED && !pObj->createArgs.enableDeiForceBypass)
        {
            pChObj->scCropConfig[outId].cropStartY = pInChInfo->startY * 2;
            pChObj->scCropConfig[outId].cropHeight = pInChInfo->height * 2;
        }
        else
        {
            pChObj->scCropConfig[outId].cropStartY = pInChInfo->startY;
            pChObj->scCropConfig[outId].cropHeight = pInChInfo->height;
        }

        if(pObj->createArgs.enableLineSkipSc && pInChInfo->memType == VPS_VPDMA_MT_NONTILEDMEM)
        {
            /* half the height and crop start Y possible only when input is non-tiled */
            pChObj->scCropConfig[outId].cropStartY /= 2;
            pChObj->scCropConfig[outId].cropHeight /= 2;
        }
    }

    pChObj->deiHqCfg.bypass = deiBypass;
    pChObj->deiHqCfg.inpMode = VPS_DEIHQ_EDIMODE_EDI_SMALL_WINDOW;
    pChObj->deiHqCfg.tempInpEnable = TRUE;
    pChObj->deiHqCfg.tempInpChromaEnable = TRUE;
    pChObj->deiHqCfg.spatMaxBypass = FALSE;
    pChObj->deiHqCfg.tempMaxBypass = FALSE;
    pChObj->deiHqCfg.fldMode = VPS_DEIHQ_FLDMODE_4FLD;
    pChObj->deiHqCfg.lcModeEnable = TRUE;
    pChObj->deiHqCfg.mvstmEnable = FALSE;
    pChObj->deiHqCfg.tnrEnable = FALSE;
    pChObj->deiHqCfg.snrEnable = FALSE;
    pChObj->deiHqCfg.sktEnable = FALSE;
    pChObj->deiHqCfg.chromaEdiEnable = FALSE;

    pChObj->deiCfg.bypass = deiBypass;
    pChObj->deiCfg.inpMode = VPS_DEIHQ_EDIMODE_EDI_LARGE_WINDOW;
    pChObj->deiCfg.tempInpEnable = TRUE;
    pChObj->deiCfg.tempInpChromaEnable = TRUE;
    pChObj->deiCfg.spatMaxBypass = FALSE;
    pChObj->deiCfg.tempMaxBypass = FALSE;

    pChObj->deiRtCfg.resetDei = FALSE;
    pChObj->deiRtCfg.fldRepeat = FALSE;

    return FVID2_SOK;
}

Int32 DeiLink_drvAllocCtxMem(DeiLink_Obj * pObj)
{
    Int32 retVal = FVID2_SOK;
    Vps_DeiCtxInfo deiCtxInfo;
    Vps_DeiCtxBuf deiCtxBuf;
    UInt32 chCnt, bCnt;

    for (chCnt = 0u; chCnt < pObj->drvCreateArgs.numCh; chCnt++)
    {
        /* Get the number of buffers to allocate */
        deiCtxInfo.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_GET_DEI_CTX_INFO, &deiCtxInfo, NULL);
        UTILS_assert(FVID2_SOK == retVal);

        /* Allocate the buffers as requested by the driver */
        for (bCnt = 0u; bCnt < deiCtxInfo.numFld; bCnt++)
        {
            #ifdef SYSTEM_DEBUG_MEMORY_ALLOC
            Vps_printf("DEI:ALLOCINFO:CTXBUF:FLDBUF:ChId[%d]/BufCnt[%d]/Size[%d]",
                       chCnt,
                       bCnt,
                       deiCtxInfo.fldBufSize);
            #endif
            deiCtxBuf.fldBuf[bCnt] = Utils_memAlloc(deiCtxInfo.fldBufSize,
                                                    VPS_BUFFER_ALIGNMENT);
            UTILS_assert(NULL != deiCtxBuf.fldBuf[bCnt]);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMv; bCnt++)
        {
            #ifdef SYSTEM_DEBUG_MEMORY_ALLOC
            Vps_printf("DEI:ALLOCINFO:CTXBUF:MVBUF:ChId[%d]/BufCnt[%d]/Size[%d]",
                       chCnt,
                       bCnt,
                       deiCtxInfo.mvBufSize);
            #endif
            deiCtxBuf.mvBuf[bCnt] = Utils_memAlloc(deiCtxInfo.mvBufSize,
                                                   VPS_BUFFER_ALIGNMENT);
            UTILS_assert(NULL != deiCtxBuf.mvBuf[bCnt]);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMvstm; bCnt++)
        {
            #ifdef SYSTEM_DEBUG_MEMORY_ALLOC
            Vps_printf("DEI:ALLOCINFO:CTXBUF:MVSTMBUF:ChId[%d]/BufCnt[%d]/Size[%d]",
                       chCnt,
                       bCnt,
                       deiCtxInfo.mvstmBufSize);
            #endif
            deiCtxBuf.mvstmBuf[bCnt] = Utils_memAlloc(deiCtxInfo.mvstmBufSize,
                                                      VPS_BUFFER_ALIGNMENT);
            UTILS_assert(NULL != deiCtxBuf.mvstmBuf[bCnt]);
        }

        /* Provided the allocated buffer to driver */
        deiCtxBuf.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_SET_DEI_CTX_BUF, &deiCtxBuf, NULL);
        UTILS_assert(FVID2_SOK == retVal);
    }

    return (retVal);

}

Int32 DeiLink_drvFreeCtxMem(DeiLink_Obj * pObj)
{
    Int32 retVal = FVID2_SOK;
    Vps_DeiCtxInfo deiCtxInfo;
    Vps_DeiCtxBuf deiCtxBuf;
    UInt32 chCnt, bCnt;

    for (chCnt = 0u; chCnt < pObj->drvCreateArgs.numCh; chCnt++)
    {
        /* Get the number of buffers to allocate */
        deiCtxInfo.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_GET_DEI_CTX_INFO, &deiCtxInfo, NULL);
        UTILS_assert(FVID2_SOK == retVal);

        /* Get the allocated buffer back from the driver */
        deiCtxBuf.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_GET_DEI_CTX_BUF, &deiCtxBuf, NULL);
        UTILS_assert(FVID2_SOK == retVal);

        /* Free the buffers */
        for (bCnt = 0u; bCnt < deiCtxInfo.numFld; bCnt++)
        {
            Utils_memFree(deiCtxBuf.fldBuf[bCnt], deiCtxInfo.fldBufSize);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMv; bCnt++)
        {
            Utils_memFree(deiCtxBuf.mvBuf[bCnt], deiCtxInfo.mvBufSize);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMvstm; bCnt++)
        {
            Utils_memFree(deiCtxBuf.mvstmBuf[bCnt], deiCtxInfo.mvstmBufSize);
        }
    }

    return (retVal);
}

Int32 DeiLink_drvSetScCoeffs(DeiLink_Obj * pObj, Bool loadAll)
{
    Int32 retVal = FVID2_SOK;
    Vps_ScCoeffParams coeffPrms;

    if (pObj->loadUpsampleCoeffs)
    {
        Vps_rprintf(" %d: DEI     : Loading Up-scaling Co-effs\n",
                    Utils_getCurTimeInMsec());

        coeffPrms.hScalingSet = VPS_SC_US_SET;
        coeffPrms.vScalingSet = VPS_SC_US_SET;
    }
    else
    {
        Vps_rprintf(" %d: DEI     : Loading Down-scaling Co-effs\n",
                    Utils_getCurTimeInMsec());

        coeffPrms.hScalingSet = VPS_SC_DS_SET_0;
        coeffPrms.vScalingSet = VPS_SC_DS_SET_0;
    }
    coeffPrms.coeffPtr = NULL;
    coeffPrms.scalarId = VPS_M2M_DEI_SCALAR_ID_DEI_SC;

    if (loadAll)
    {
        /* Program DEI scalar coefficient - Always used */
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_SET_COEFFS, &coeffPrms, NULL);
        UTILS_assert(FVID2_SOK == retVal);
    }

    /* Program the second scalar coefficient if needed */
#ifdef TI_816X_BUILD
    if ((VPS_M2M_INST_MAIN_DEIH_SC3_VIP0 == pObj->drvInstId) ||
        (VPS_M2M_INST_AUX_DEI_SC4_VIP1 == pObj->drvInstId) ||
        (VPS_M2M_INST_MAIN_DEIH_SC1_SC3_WB0_VIP0 == pObj->drvInstId) ||
        (VPS_M2M_INST_AUX_DEI_SC2_SC4_WB1_VIP1 == pObj->drvInstId))
#endif                                                     /* TI_816X_BUILD */

#ifdef TI_814X_BUILD
        if ((VPS_M2M_INST_MAIN_DEI_SC3_VIP0 == pObj->drvInstId) ||
            (VPS_M2M_INST_MAIN_DEI_SC1_SC3_WB0_VIP0 == pObj->drvInstId) ||
            (VPS_M2M_INST_AUX_SC2_SC4_WB1_VIP1 == pObj->drvInstId)
          )
#endif                                                     /* TI_814X_BUILD */
        {
            /* Program VIP scalar coefficients */
            coeffPrms.scalarId = VPS_M2M_DEI_SCALAR_ID_VIP_SC;
            retVal = FVID2_control(pObj->fvidHandle,
                                   IOCTL_VPS_SET_COEFFS, &coeffPrms, NULL);
            UTILS_assert(FVID2_SOK == retVal);
        }

    Vps_rprintf(" %d: DEI     : Co-effs Loading ... DONE !!!\n",
                Utils_getCurTimeInMsec());

    return (retVal);
}

Int32 DeiLink_drvCreateFvidObj(DeiLink_Obj * pObj)
{
    Vps_M2mDeiChParams *pChParams;
    DeiLink_ChObj *pChObj;
    UInt32 chId;
    FVID2_CbParams cbParams;

    pObj->drvCreateArgs.mode = VPS_M2M_CONFIG_PER_CHANNEL;
    pObj->drvCreateArgs.numCh = pObj->inQueInfo.numCh;
    pObj->drvCreateArgs.deiHqCtxMode = VPS_DEIHQ_CTXMODE_DRIVER_ALL;
    pObj->drvCreateArgs.chParams = (const Vps_M2mDeiChParams *) pObj->drvChArgs;
    pObj->drvCreateArgs.isVipScReq = FALSE;

    if ((TRUE == pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC]) ||
        (TRUE == pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT]))
    {
        pObj->drvCreateArgs.isVipScReq = TRUE;
    }

    for (chId = 0u; chId < pObj->drvCreateArgs.numCh; chId++)
    {
        pChParams = &pObj->drvChArgs[chId];
        pChObj = &pObj->chObj[chId];

        pChParams->outFmtDei = NULL;
        pChParams->outFmtVip = NULL;
        pChParams->inFmtFldN_1 = NULL;
        pChParams->deiHqCfg = NULL;
        pChParams->deiCfg = NULL;
        pChParams->scCfg = NULL;
        pChParams->deiCropCfg = NULL;
        pChParams->vipScCfg = NULL;
        pChParams->vipCropCfg = NULL;
        pChParams->subFrameParams = NULL;
#ifdef TI_816X_BUILD
        if (pObj->linkId == SYSTEM_LINK_ID_DEI_HQ_0
            || pObj->linkId == SYSTEM_LINK_ID_DEI_HQ_1)
        {
            pChParams->deiHqCfg = &pChObj->deiHqCfg;

            pObj->drvInstId = VPS_M2M_INST_MAIN_DEIH_SC1_WB0;
            pObj->vipInstId = SYSTEM_VIP_0;

            if ((pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC]) ||
                (pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT]))
            {
                pObj->drvInstId = VPS_M2M_INST_MAIN_DEIH_SC1_SC3_WB0_VIP0;
            }
        }

        if (pObj->linkId == SYSTEM_LINK_ID_DEI_0
            || pObj->linkId == SYSTEM_LINK_ID_DEI_1)
        {
            pChParams->deiCfg = &pChObj->deiCfg;

            pObj->drvInstId = VPS_M2M_INST_AUX_DEI_SC2_WB1;
            pObj->vipInstId = SYSTEM_VIP_1;

            if ((pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC])||
                (pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT]))
            {
                pObj->drvInstId = VPS_M2M_INST_AUX_DEI_SC2_SC4_WB1_VIP1;
            }
        }
#endif                                                     /* TI_816X_BUILD */

#ifdef TI_814X_BUILD
        if (pObj->linkId == SYSTEM_LINK_ID_DEI_0
            || pObj->linkId == SYSTEM_LINK_ID_DEI_1)
        {
            pChParams->deiCfg = &pChObj->deiCfg;

            pObj->drvInstId = VPS_M2M_INST_MAIN_DEI_SC1_WB0;
            pObj->vipInstId = SYSTEM_VIP_0;

            if (pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC] || pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT])
            {
                pObj->drvInstId = VPS_M2M_INST_MAIN_DEI_SC1_SC3_WB0_VIP0;
            }
        }

        /* SYSTEM_LINK_ID_DEI_HQ_0 in 814x points to SC2 + VIP1-SC in non DEI mode */
        if (pObj->linkId == SYSTEM_LINK_ID_DEI_HQ_0
            || pObj->linkId == SYSTEM_LINK_ID_DEI_HQ_1)
        {
            pChParams->deiCfg = NULL;
            /* Force DEI byPass to TRUE even if usecase doesnt do as DEI is not available in this mode */
            pObj->createArgs.enableDeiForceBypass = TRUE;

            pObj->drvInstId = VPS_M2M_INST_AUX_SC2_WB1;
            pObj->vipInstId = SYSTEM_VIP_1;

            if ((pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC])||
                (pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT]))
            {
                pObj->drvInstId = VPS_M2M_INST_AUX_SC2_SC4_WB1_VIP1;
            }
        }
#endif                                                     /* TI_814X_BUILD */

        if (pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_DEI_SC])
        {
            pChParams->scCfg = &pChObj->scCfg[DEI_LINK_OUT_QUE_DEI_SC];
            pChParams->deiCropCfg =
                &pChObj->scCropConfig[DEI_LINK_OUT_QUE_DEI_SC];
            pChParams->outFmtDei = &pChObj->outFormat[DEI_LINK_OUT_QUE_DEI_SC];
        }
        if (pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC] || pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT])
        {
            pChParams->vipScCfg = &pChObj->scCfg[DEI_LINK_OUT_QUE_VIP_SC];
            pChParams->vipCropCfg =
                &pChObj->scCropConfig[DEI_LINK_OUT_QUE_VIP_SC];
            pChParams->outFmtVip = &pChObj->outFormat[DEI_LINK_OUT_QUE_VIP_SC];
        }
    }

    memset(&cbParams, 0, sizeof(cbParams));

    cbParams.cbFxn = DeiLink_drvFvidCb;
    cbParams.errCbFxn = DeiLink_drvFvidErrCb;
    cbParams.errList = &pObj->errProcessList;
    cbParams.appData = pObj;

    pObj->fvidHandle = FVID2_create(FVID2_VPS_M2M_DEI_DRV,
                                    pObj->drvInstId,
                                    &pObj->drvCreateArgs,
                                    &pObj->drvCreateStatus, &cbParams);
    UTILS_assert(pObj->fvidHandle != NULL);

    return FVID2_SOK;
}

Int32 DeiLink_drvCreate(DeiLink_Obj * pObj, DeiLink_CreateParams * pPrm)
{
    UInt32 chId;
    Semaphore_Params semParams;
    Int32 status;
    Vps_PlatformCpuRev cpuRev;

#ifdef SYSTEM_DEBUG_DEI
    Vps_printf(" %d: DEI    : Create in progress !!!\n", Utils_getCurTimeInMsec());
#endif

    UTILS_MEMLOG_USED_START();
    memcpy(&pObj->createArgs, pPrm, sizeof(*pPrm));

    status = System_linkGetInfo(pPrm->inQueParams.prevLinkId, &pObj->inTskInfo);
    UTILS_assert(status == FVID2_SOK);
    UTILS_assert(pPrm->inQueParams.prevLinkQueId < pObj->inTskInfo.numQue);

    memcpy(&pObj->inQueInfo,
           &pObj->inTskInfo.queInfo[pPrm->inQueParams.prevLinkQueId],
           sizeof(pObj->inQueInfo));

#ifndef SYSTEM_USE_TILER
    if (pObj->createArgs.tilerEnable)
    {
        Vps_printf("DEILINK:!!WARNING.FORCIBLY DISABLING TILER since tiler is disabled at build time");
        pObj->createArgs.tilerEnable = FALSE;
    }
#endif

    cpuRev = Vps_platformGetCpuRev();

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        if (cpuRev < VPS_PLATFORM_CPU_REV_2_0)
        {
            if((pObj->inQueInfo.chInfo[chId].width > DEI_DRV_MAX_WIDTH_LIMIT_BEFORE_CPU_REV_2_0) && 
                 ((pObj->inQueInfo.chInfo[chId].dataFormat != FVID2_DF_YUV420SP_UV) ||
                   (pObj->inQueInfo.chInfo[chId].dataFormat != FVID2_DF_YUV420SP_VU) ||
                    (pObj->inQueInfo.chInfo[chId].dataFormat != FVID2_DF_YUV420P)))
            {

                Vps_printf(" %u: Warning: This CPU Revision [%s] does not support current set width %d\n",\
                    Utils_getCurTimeInMsec(), gVpss_cpuVer[cpuRev],\
                                              pObj->inQueInfo.chInfo[chId].width);
                Vps_printf(" %u: Warning: Limiting Input width to 960\n",Utils_getCurTimeInMsec());
                {
                    pObj->inQueInfo.chInfo[chId].width = DEI_DRV_MAX_WIDTH_LIMIT_BEFORE_CPU_REV_2_0;
                    pObj->inQueInfo.chInfo[chId].pitch[0] = 2*DEI_DRV_MAX_WIDTH_LIMIT_BEFORE_CPU_REV_2_0;
                    pObj->inQueInfo.chInfo[chId].pitch[1] = 2*DEI_DRV_MAX_WIDTH_LIMIT_BEFORE_CPU_REV_2_0;
                    pObj->inQueInfo.chInfo[chId].pitch[2] = 2*DEI_DRV_MAX_WIDTH_LIMIT_BEFORE_CPU_REV_2_0;
                }
            }
        }
    }

    pObj->inFrameGetCount = 0;
    pObj->inFramePutCount = 0;
    pObj->outFrameGetCount[0] = 0;
    pObj->outFrameGetCount[1] = 0;
    pObj->outFrameGetCount[2] = 0;
    pObj->outFramePutCount[0] = 0;
    pObj->outFramePutCount[1] = 0;
    pObj->outFramePutCount[2] = 0;
    pObj->processFrameReqPendCount = 0;
    pObj->processFrameReqPendSubmitCount = 0;
    pObj->processFrameCount = 0;
    pObj->getProcessFrameCount = 0;
    pObj->processFrameReqCount = 0;
    pObj->getProcessFrameReqCount = 0;
    pObj->totalTime = 0;
    pObj->curTime = 0;
    pObj->givenInFrames = 0x0;
    pObj->returnedInFrames = 0x0;
    pObj->loadUpsampleCoeffs = FALSE;

    DeiLink_resetStatistics(pObj);

    /* Create semaphores */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    pObj->complete = Semaphore_create(0u, &semParams, NULL);
    UTILS_assert(pObj->complete != NULL);

    DeiLink_drvCreateOutObj(pObj);

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
        DeiLink_drvCreateChObj(pObj, chId);

    DeiLink_drvCreateFvidObj(pObj);
#ifdef TI_814X_BUILD    /* CtxMem allocation should not be done for 814x in bypass mode */
    if (pObj->createArgs.enableDeiForceBypass == FALSE)
#endif
        DeiLink_drvAllocCtxMem(pObj);
    DeiLink_drvSetScCoeffs(pObj, TRUE);
    DeiLink_drvCreateReqObj(pObj);
    UTILS_MEMLOG_USED_END(pObj->memUsed);
    UTILS_MEMLOG_PRINT("DEI",
                       pObj->memUsed,
                       UTILS_ARRAYSIZE(pObj->memUsed));

#ifdef SYSTEM_DEBUG_DEI
    Vps_printf(" %d: DEI    : Create Done !!!\n", Utils_getCurTimeInMsec());
#endif

    return FVID2_SOK;
}

Int32 DeiLink_drvQueueFramesToChQue(DeiLink_Obj * pObj)
{
    UInt32 frameId, freeFrameNum;
    FVID2_Frame *pFrame;
    System_LinkInQueParams *pInQueParams;
    FVID2_FrameList frameList;
    DeiLink_ChObj *pChObj;
    Int32 status;

    pInQueParams = &pObj->createArgs.inQueParams;

    System_getLinksFullFrames(pInQueParams->prevLinkId,
                              pInQueParams->prevLinkQueId, &frameList);

    if (frameList.numFrames)
    {
#ifdef SYSTEM_DEBUG_DEI_RT
        Vps_printf(" %d: DEI    : Received %d IN frames !!!\n",
                   Utils_getCurTimeInMsec(), frameList.numFrames);
#endif

        pObj->inFrameGetCount += frameList.numFrames;

        freeFrameNum = 0;

        for (frameId = 0; frameId < frameList.numFrames; frameId++)
        {
            pFrame = frameList.frames[frameId];

            pChObj = &pObj->chObj[pFrame->channelNum];

            pChObj->inFrameRecvCount++;

            // pChObj->nextFid = pFrame->fid;

            /* in bypass mode only pick even fields */
            if(pObj->createArgs.enableDeiForceBypass)
                pChObj->nextFid = 0;

            if (pChObj->nextFid == pFrame->fid)
            {
                // frame is of the expected FID use it, else drop it
                status = Utils_bufPutFullFrame(&pChObj->inQue, pFrame);
                UTILS_assert(status == FVID2_SOK);

                pChObj->nextFid ^= 1;                      // toggle to next
                                                           // required FID
                pChObj->deiRtCfg.fldRepeat = FALSE;
            }
            else
            {
                if(pObj->createArgs.enableDeiForceBypass)
                {
                    pChObj->inFrameRejectCount++;

                    // frame is not of expected FID, so release frame
                    frameList.frames[freeFrameNum] = pFrame;
                    freeFrameNum++;
                }
                else
                {
                    // frame is of the wrong FID,  still use it with FldRepeat set to TRUE. Not toggling nextFid
                    status = Utils_bufPutFullFrame(&pChObj->inQue, pFrame);
                    UTILS_assert(status == FVID2_SOK);

                    pChObj->deiRtCfg.fldRepeat = TRUE;
                }
            }
        }

        if (freeFrameNum)
        {
            frameList.numFrames = freeFrameNum;

#ifdef SYSTEM_DEBUG_DEI_RT
            Vps_printf(" %d: DEI    : Skipped %d IN frames !!!\n",
                       Utils_getCurTimeInMsec(), frameList.numFrames);
#endif

            pObj->inFramePutCount += freeFrameNum;

            System_putLinksEmptyFrames(pInQueParams->prevLinkId,
                                       pInQueParams->prevLinkQueId, &frameList);
        }
    }

    return FVID2_SOK;
}

Int32 DeiLink_drvMakeFrameLists(DeiLink_Obj * pObj,
                                FVID2_FrameList * inFrameList,
                                 FVID2_FrameList
                                outFrameList[DEI_LINK_MAX_DRIVER_OUT_QUE])
{
    DeiLink_ChObj *pChObj;
    UInt32 chId, outId, frameId;
    FVID2_Frame *pInFrame, *pOutFrame;
    Int32 status;
    Bool doFrameDrop;
    Bool repeatFld = FALSE;
    System_FrameInfo *pFrameInfo;

    System_FrameInfo *pInFrameInfo;
    frameId = 0;

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pChObj = &pObj->chObj[chId];

        Utils_bufGetFullFrame(&pChObj->inQue, &pInFrame, BIOS_NO_WAIT);
        memset(&pChObj->deiRtPrm, 0, sizeof(pChObj->deiRtPrm));

        if (pInFrame)
        {
            inFrameList->frames[frameId] = pInFrame;

            pInFrameInfo = (System_FrameInfo *) pInFrame->appData;

            pChObj->inFrameProcessCount++;

            repeatFld = pChObj->deiRtCfg.fldRepeat;

            /* Process for DEI-SC Queue */
            outId = DEI_LINK_OUT_QUE_DEI_SC;
            pOutFrame = NULL;

            if ((pObj->createArgs.enableOut[outId]) &&
                (pChObj->enableOut[outId]) &&
                (Utils_queGetQueuedCount (&pObj->outObj[outId]
                                          .bufOutQue.emptyQue)))
            {
                doFrameDrop = Utils_doSkipFrame(&(pChObj->frameSkipCtx[outId]));

                if( doFrameDrop == TRUE)
                {
                    pOutFrame = &pObj->outFrameDrop;

                    pChObj->frameSkipCount[outId]++;
                    pChObj->outFrameUserSkipCount[outId]++;
                }
                else
                {
                    status =
                      Utils_bufGetEmptyFrame(&pObj->outObj[outId].
                                          bufOutQue, &pOutFrame,
                                          BIOS_WAIT_FOREVER);
                    UTILS_assert(status == FVID2_SOK);
                    UTILS_assert(pOutFrame != NULL);

                    pObj->outFrameGetCount[outId]++;
                    pChObj->outFrameCount[outId]++;

                    pFrameInfo = (System_FrameInfo *) pOutFrame->appData;
                    UTILS_assert(pFrameInfo != NULL);
                    if (pChObj->chRtOutInfoUpdate[outId] == TRUE)
                    {
                        pInFrame->perFrameCfg = &pChObj->deiRtPrm;
                        pChObj->deiRtPrm.deiOutFrmPrms =
                                         &pChObj->deiRtOutFrmPrm[outId];
                        pChObj->deiRtPrm.deiScCropCfg =
                                         &pChObj->scCropConfig[outId];
                        pChObj->chRtOutInfoUpdate[outId] = FALSE;
                    }
                    if(repeatFld)
                    {
                        pInFrame->perFrameCfg = &pChObj->deiRtPrm;
                        pChObj->deiRtPrm.deiRtCfg = &pChObj->deiRtCfg;
                    }
                        
                    
                    pFrameInfo->rtChInfo.width =
                                pChObj->deiRtOutFrmPrm[outId].width;
                    pFrameInfo->rtChInfo.height =
                                pChObj->deiRtOutFrmPrm[outId].height;
                    pFrameInfo->rtChInfo.pitch[0] =
                                pChObj->deiRtOutFrmPrm[outId].pitch[0];
                    pFrameInfo->rtChInfo.pitch[1] =
                                pChObj->deiRtOutFrmPrm[outId].pitch[1];
                    pFrameInfo->rtChInfoUpdate = TRUE;
                    pFrameInfo->ts64  = pInFrameInfo->ts64;
                }

                pOutFrame->channelNum = pInFrame->channelNum;
                pOutFrame->timeStamp  = pInFrame->timeStamp;
                pOutFrame->fid = pInFrame->fid;
            }
            else
            {
                pChObj->outFrameSkipCount[outId]++;
            }

            if (pOutFrame == NULL)
            {
                pOutFrame = &pObj->outFrameDrop;
                pOutFrame->channelNum = pInFrame->channelNum;
                pOutFrame->timeStamp  = pInFrame->timeStamp;
            }

            outFrameList[DEI_LINK_OUT_QUE_DEI_SC].frames[frameId] = pOutFrame;


            /****************************************************************/
            /* Process for VIP-SC 0 & 1 Queues */
            outId = DEI_LINK_OUT_QUE_VIP_SC;
            pOutFrame = NULL;

            if (pChObj->enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT])
            {
                if(pInFrame->fid == 1)
                    outId = DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT;
                else
                    outId = DEI_LINK_OUT_QUE_VIP_SC;

                pChObj->chRtOutInfoUpdate[outId] = TRUE;
            }

            if ((pObj->createArgs.enableOut[outId]) &&
                (pChObj->enableOut[outId]) &&
                (Utils_queGetQueuedCount (&pObj->outObj[outId]
                                          .bufOutQue.emptyQue)))
            {
                doFrameDrop = Utils_doSkipFrame(&(pChObj->frameSkipCtx[outId]));
                if (pChObj->enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT])
                {
                    if (doFrameDrop)
                    {
                        pChObj->chRtOutInfoUpdate[outId] = FALSE;
                        pChObj->chRtOutInfoUpdateDropCnt++;
                    } 
                    else
                    {
                        if (pChObj->chRtOutInfoUpdateDropCnt & 1)
                        {
                            pChObj->chRtOutInfoUpdate[outId] = FALSE;
                        }
                        pChObj->chRtOutInfoUpdateDropCnt = 0;
                    }
                }
                if (doFrameDrop == TRUE)
                {
                    pOutFrame = &pObj->outFrameDrop;

                    pChObj->frameSkipCount[outId]++;
                    pChObj->outFrameUserSkipCount[outId]++;
                }
                else
                {
                    status =
                      Utils_bufGetEmptyFrame(&pObj->outObj[outId].
                                         bufOutQue, &pOutFrame,
                                         BIOS_WAIT_FOREVER);
                    UTILS_assert(status == FVID2_SOK);
                    UTILS_assert(pOutFrame != NULL);

                    pObj->outFrameGetCount[outId]++;
                    pChObj->outFrameCount[outId]++;
				                    
                    pFrameInfo = (System_FrameInfo *) pOutFrame->appData;
                    UTILS_assert(pFrameInfo != NULL);
                    if (pChObj->chRtOutInfoUpdate[outId] == TRUE)
                    {
                        pInFrame->perFrameCfg = &pChObj->deiRtPrm;
                        pChObj->deiRtPrm.vipOutFrmPrms =
                                         &pChObj->vipRtOutFrmPrm[outId];
                        pChObj->deiRtPrm.vipScCropCfg =
                                         &pChObj->scCropConfig[outId];
                        pChObj->chRtOutInfoUpdate[outId] = FALSE;
                    }
                    if(repeatFld)
                    {
                        pInFrame->perFrameCfg = &pChObj->deiRtPrm;
                        pChObj->deiRtPrm.deiRtCfg = &pChObj->deiRtCfg;
                    }                    
                    
                    pFrameInfo->rtChInfo.width =
                                pChObj->vipRtOutFrmPrm[outId].width;
                    pFrameInfo->rtChInfo.height =
                                pChObj->vipRtOutFrmPrm[outId].height;
                    pFrameInfo->rtChInfo.pitch[0] =
                                pChObj->vipRtOutFrmPrm[outId].pitch[0];
                    pFrameInfo->rtChInfo.pitch[1] =
                                pChObj->vipRtOutFrmPrm[outId].pitch[1];
                    pFrameInfo->rtChInfoUpdate = TRUE;
                    pFrameInfo->ts64  = pInFrameInfo->ts64;
                }

                pOutFrame->channelNum = pInFrame->channelNum;
                pOutFrame->timeStamp  = pInFrame->timeStamp;
                pOutFrame->fid = pInFrame->fid;
            }
            else
            {
                pChObj->outFrameSkipCount[outId]++;
            }

            if (pOutFrame == NULL)
            {
                pOutFrame = &pObj->outFrameDrop;
                pOutFrame->channelNum = pInFrame->channelNum;
                pOutFrame->timeStamp  = pInFrame->timeStamp;
            }

            outFrameList[DEI_LINK_OUT_QUE_VIP_SC].frames[frameId] = pOutFrame;
            frameId++;

            pChObj->curFrameNum++;

        }
    }

    inFrameList->numFrames = frameId;
    for (outId = 0; outId < DEI_LINK_MAX_DRIVER_OUT_QUE; outId++)
        outFrameList[outId].numFrames = frameId;

    return FVID2_SOK;
}

#ifdef TI_816X_BUILD
Int32 DeiLink_drvReleaseFrames(DeiLink_Obj * pObj,
                               FVID2_FrameList * inFrameList,
                               FVID2_FrameList
                               outFrameList[DEI_LINK_MAX_DRIVER_OUT_QUE])
{
    UInt32 frameId, outId, outputId, sendCmd[DEI_LINK_MAX_OUT_QUE], latency;
    FVID2_Frame *pFrame;
    System_LinkInQueParams *pInQueParams;
    Int32 status;
    DeiLink_ChObj *pChObj;

    pInQueParams = &pObj->createArgs.inQueParams;

    for (outId = 0; outId < DEI_LINK_MAX_OUT_QUE; outId++)
        sendCmd[outId] = FALSE;

    for (frameId = 0; frameId < inFrameList->numFrames; frameId++)
    {
        for (outputId = 0; outputId < DEI_LINK_MAX_DRIVER_OUT_QUE; outputId++)
        {
            if(pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT] == TRUE)
            {
                if(inFrameList->frames[frameId]->fid == 1 && outputId == DEI_LINK_OUT_QUE_VIP_SC)
                  outId = DEI_LINK_MAX_DRIVER_OUT_QUE;
                else
                  outId = outputId;
            }
            else
                outId = outputId;

            if (pObj->createArgs.enableOut[outId])
            {
                pFrame = outFrameList[outputId].frames[frameId];
                if (pFrame && pFrame != &pObj->outFrameDrop)
                {
                    pChObj = &pObj->chObj[pFrame->channelNum];

                    // HACK !!!! - fid should be always 0 after DEI <explicitly setting here to avoid drop in SWMS based on fid. To be fixed in vpss driver */
                    pFrame->fid = 0;

                    latency = Utils_getCurTimeInMsec() - pFrame->timeStamp;

                    if(latency>pChObj->maxLatency)
                        pChObj->maxLatency = latency;
                    if(latency<pChObj->minLatency)
                        pChObj->minLatency = latency;

                    status =
                        Utils_bufPutFullFrame(&pObj->outObj[outId].bufOutQue,
                                              pFrame);
                    UTILS_assert(status == FVID2_SOK);

                    sendCmd[outId] = TRUE;

                    pObj->outFramePutCount[outId]++;
                }
            }
        }
    }

#ifdef SYSTEM_DEBUG_DEI_RT
    Vps_printf(" %d: DEI    : Released %d IN frames !!!\n", Utils_getCurTimeInMsec(),
               inFrameList->numFrames);
#endif

    pObj->inFramePutCount += inFrameList->numFrames;

    System_putLinksEmptyFrames(pInQueParams->prevLinkId,
                               pInQueParams->prevLinkQueId, inFrameList);

    for (outId = 0; outId < DEI_LINK_MAX_OUT_QUE; outId++)
    {
        if (sendCmd[outId])
        {
            System_sendLinkCmd(pObj->createArgs.outQueParams[outId].nextLink,
                               SYSTEM_CMD_NEW_DATA);
        }
    }

    return FVID2_SOK;
}
#endif                                                     /* TI_816X_BUILD */

#ifdef TI_814X_BUILD
Int32 DeiLink_drvReleaseFrames(DeiLink_Obj * pObj,
                               FVID2_FrameList * inFrameList,
                               FVID2_FrameList
                               outFrameList[DEI_LINK_MAX_OUT_QUE])
{
    UInt32 frameId, outId, outputId, sendCmd[DEI_LINK_MAX_OUT_QUE], latency;
    FVID2_Frame *pFrame;
    System_LinkInQueParams *pInQueParams;
    Int32 status;
    Int32 actualFrameIdx;
    DeiLink_ChObj *pChObj;

    pInQueParams = &pObj->createArgs.inQueParams;

    for (outputId = 0; outputId < DEI_LINK_MAX_OUT_QUE; outputId++)
        sendCmd[outputId] = FALSE;

    for (outputId = 0; outputId < DEI_LINK_MAX_DRIVER_OUT_QUE; outputId++)
    {
        if (pObj->createArgs.enableOut[outputId])
        {
            for (frameId = 0; frameId < outFrameList[outputId].numFrames;
                 frameId++)
            {
                pFrame = outFrameList[outputId].frames[frameId];

                if (pObj->createArgs.enableOut[DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT])
                {
                    if(pFrame->fid == 1 && outputId == DEI_LINK_OUT_QUE_VIP_SC)
                      outId = DEI_LINK_MAX_DRIVER_OUT_QUE;
                    else
                      outId = outputId;
                }
                else
                    outId = outputId;

                if ((pFrame) && (pFrame != &pObj->outFrameDrop))
                {
                    // HACK !!!! - fid should be always 0 after DEI <explicitly setting here to avoid drop in SWMS based on fid. To be fixed in vpss driver */
                    pFrame->fid = 0;

                    pChObj = &pObj->chObj[pFrame->channelNum];

                    latency = Utils_getCurTimeInMsec() - pFrame->timeStamp;

                    if(latency>pChObj->maxLatency)
                        pChObj->maxLatency = latency;
                    if(latency<pChObj->minLatency)
                        pChObj->minLatency = latency;

                    status =
                        Utils_bufPutFullFrame(&pObj->outObj[outId].bufOutQue,
                                              pFrame);
                    UTILS_assert(status == FVID2_SOK);

                    sendCmd[outId] = TRUE;

                    pObj->outFramePutCount[outId]++;
                }
            }
        }
    }

    if (inFrameList->numFrames > 0x0)
    {
#ifdef SYSTEM_DEBUG_DEI_RT
        Vps_printf(" %d: DEI    : Released %d IN frames !!!\n",
                   Utils_getCurTimeInMsec(), inFrameList->numFrames);
#endif

        /* There could be holes in the frame list, as the driver could have
         * decided to hold back couple of frames as context, ensure the frame
         * list is compacted */
        actualFrameIdx = -1;
        for (frameId = 0x0; frameId < inFrameList->numFrames; frameId++)
        {
            if (NULL != inFrameList->frames[frameId])
            {
                actualFrameIdx++;
                inFrameList->frames[actualFrameIdx] =
                    inFrameList->frames[frameId];
            }
            else
            {
                /* Do nothing */
                continue;
            }
        }

        if (actualFrameIdx != -1)
        {
            inFrameList->numFrames = actualFrameIdx + 0x01u;
            pObj->inFramePutCount += inFrameList->numFrames;
            pObj->returnedInFrames += inFrameList->numFrames;

            System_putLinksEmptyFrames(pInQueParams->prevLinkId,
                                       pInQueParams->prevLinkQueId,
                                       inFrameList);
        }
    }

    for (outId = 0; outId < DEI_LINK_MAX_OUT_QUE; outId++)
    {
        if (sendCmd[outId])
        {
            System_sendLinkCmd(pObj->createArgs.outQueParams[outId].nextLink,
                               SYSTEM_CMD_NEW_DATA);
        }
    }

    return FVID2_SOK;
}
#endif                                                     /* TI_814X_BUILD */

Int32 DeiLink_drvSubmitData(DeiLink_Obj * pObj)
{
    Int32 status;
    DeiLink_ReqObj *pReqObj;

    status = Utils_queGet(&pObj->reqQue, (Ptr *) & pReqObj, 1, BIOS_NO_WAIT);
    if (status != FVID2_SOK)
    {
#ifdef SYSTEM_DEBUG_DEI_RT
        Vps_printf(" %d: DEI    : Pending request !!!\n", Utils_getCurTimeInMsec());
#endif

        pObj->processFrameReqPendCount++;

        pObj->isReqPend = TRUE;

        // cannot process more frames to process
        return FVID2_EFAIL;
    }

    if (pObj->processFrameReqPendCount == pObj->processFrameReqPendSubmitCount)
        pObj->isReqPend = FALSE;

    pReqObj->processList.inFrameList[0] = &pReqObj->inFrameList;
    pReqObj->processList.outFrameList[0] = &pReqObj->outFrameList[0];
    pReqObj->processList.outFrameList[1] = &pReqObj->outFrameList[1];
    pReqObj->processList.numInLists = 1;
    pReqObj->processList.numOutLists = pObj->reqNumOutLists;

    pReqObj->inFrameList.appData = pReqObj;

    DeiLink_drvMakeFrameLists(pObj, &pReqObj->inFrameList,
                              pReqObj->outFrameList);

#ifdef SYSTEM_DEBUG_DEI_RT
    Vps_printf(" %d: DEI    : Submitting %d frames !!!\n", Utils_getCurTimeInMsec(),
               pReqObj->inFrameList.numFrames);
#endif

    if (pReqObj->inFrameList.numFrames)
    {
        pObj->reqQueCount++;

        pObj->processFrameCount += pReqObj->inFrameList.numFrames;
        pObj->processFrameReqCount++;

        pObj->givenInFrames += pReqObj->inFrameList.numFrames;

        System_lockVip(pObj->vipInstId);

        if (System_clearVipResetFlag(pObj->vipInstId))
        {
            /* VIP was reset since last frame processing, so we need to
             * reload VIP-SC co-effs */
            DeiLink_drvSetScCoeffs(pObj, FALSE);
        }

        pObj->curTime = Utils_getCurTimeInMsec();
        status = FVID2_processFrames(pObj->fvidHandle, &pReqObj->processList);
        UTILS_assert(status == FVID2_SOK);

#ifndef DEI_LINK_QUEUE_REQ
        Semaphore_pend(pObj->complete, BIOS_WAIT_FOREVER);

        DeiLink_drvGetProcessedData(pObj);;
#endif

        System_unlockVip(pObj->vipInstId);
    }
    else
    {

        status = Utils_quePut(&pObj->reqQue, pReqObj, BIOS_NO_WAIT);
        UTILS_assert(status == FVID2_SOK);

        // no more frames to process
        status = FVID2_EFAIL;
    }

    return status;
}

Int32 DeiLink_drvProcessData(DeiLink_Obj * pObj)
{
    Int32 status;

    DeiLink_drvQueueFramesToChQue(pObj);

    do
    {
        status = DeiLink_drvSubmitData(pObj);
    } while (status == FVID2_SOK);

    return FVID2_SOK;
}

Int32 DeiLink_drvGetProcessedData(DeiLink_Obj * pObj)
{
    DeiLink_ReqObj *pReqObj;
    Int32 status;
    FVID2_ProcessList processList;

    status =
        FVID2_getProcessedFrames(pObj->fvidHandle, &processList, BIOS_NO_WAIT);
    UTILS_assert(status == FVID2_SOK);

    pObj->curTime = Utils_getCurTimeInMsec() - pObj->curTime;
    pObj->totalTime += pObj->curTime;

#ifdef SYSTEM_DEBUG_DEI_RT
    Vps_printf(" %d: DEI    : Completed %d frames !!!\n", Utils_getCurTimeInMsec(),
               processList.outFrameList[0]->numFrames);
#endif

    pObj->getProcessFrameCount += processList.outFrameList[0]->numFrames;
    pObj->getProcessFrameReqCount++;

    pReqObj = (DeiLink_ReqObj *) processList.inFrameList[0]->appData;

    DeiLink_drvReleaseFrames(pObj, &pReqObj->inFrameList,
                             pReqObj->outFrameList);

    status = Utils_quePut(&pObj->reqQue, pReqObj, BIOS_NO_WAIT);
    UTILS_assert(status == FVID2_SOK);

    pObj->reqQueCount--;

    if (pObj->isReqPend)
    {
#ifdef SYSTEM_DEBUG_DEI_RT
        Vps_printf(" %d: DEI    : Submitting pending request !!!\n",
                   Utils_getCurTimeInMsec());
#endif

        pObj->processFrameReqPendSubmitCount++;

        DeiLink_drvSubmitData(pObj);
    }
    return FVID2_SOK;
}

Int32 DeiLink_drvStop(DeiLink_Obj * pObj)
{
    Int32 rtnValue = FVID2_SOK;

#ifdef SYSTEM_DEBUG_DEI
    Vps_printf(" %d: DEI    : Stop in progress, %d requests pending !!!\n",
               Utils_getCurTimeInMsec(), pObj->reqQueCount);
#endif

#ifdef TI_814X_BUILD
    rtnValue = FVID2_stop(pObj->fvidHandle, NULL);
    if (rtnValue != FVID2_SOK)
    {
#ifdef SYSTEM_DEBUG_DEI
        Vps_printf(" %d: DEI    : Stop Fails !!!\n", Utils_getCurTimeInMsec());
#endif
    }
#endif                                                     /* TI_814X_BUILD */

    while (pObj->reqQueCount)
    {
        Utils_tskWaitCmd(&pObj->tsk, NULL, DEI_LINK_CMD_GET_PROCESSED_DATA);
        DeiLink_drvGetProcessedData(pObj);
    }
    /* Even though all the requests are addressed, the driver would have held
     * back couple of input fields as context fields, get them */
#ifdef TI_814X_BUILD
    if ((pObj->returnedInFrames - pObj->givenInFrames) != 0x0)
    {
        DeiLink_drvReleaseContextField(pObj);
    }
#endif                                                     /* TI_814X_BUILD */
#ifdef SYSTEM_DEBUG_DEI
    Vps_printf(" %d: DEI    : Stop Done !!!\n", Utils_getCurTimeInMsec());
#endif

    return (rtnValue);
}

Int32 DeiLink_drvDelete(DeiLink_Obj * pObj)
{
    UInt32 outId, chId;
    DeiLink_ChObj *pChObj;
    DeiLink_OutObj *pOutObj;
    Bool tilerUsed = FALSE;

#ifdef SYSTEM_DEBUG_DEI
    Vps_printf(" %d: DEI    : Fields = %d (fps = %d), !!!\n",
                Utils_getCurTimeInMsec(),
                pObj->getProcessFrameCount,
                pObj->getProcessFrameCount * 100 / (pObj->totalTime / 10)
                );
#endif

#ifdef SYSTEM_DEBUG_DEI
    Vps_printf(" %d: DEI    : Delete in progress !!!\n", Utils_getCurTimeInMsec());
#endif

#ifdef TI_814X_BUILD    /* CtxMem allocation should not be done for 814x in bypass mode */
    if (pObj->createArgs.enableDeiForceBypass == FALSE)
#endif
        DeiLink_drvFreeCtxMem(pObj);

    FVID2_delete(pObj->fvidHandle, NULL);

    /* Delete semaphores */
    Semaphore_delete(&pObj->complete);

    for (outId = 0; outId < DEI_LINK_MAX_OUT_QUE; outId++)
    {
        if (pObj->createArgs.enableOut[outId])
        {
            pOutObj = &pObj->outObj[outId];

            Utils_bufDelete(&pOutObj->bufOutQue);

            if (pObj->createArgs.tilerEnable && (outId == DEI_LINK_OUT_QUE_VIP_SC)
                && pOutObj->outFormat.dataFormat == FVID2_DF_YUV420SP_UV)
            {
                tilerUsed = TRUE;
            }
            else
            {
                Utils_memFrameFree(&pOutObj->outFormat, pOutObj->outFrames,
                                   pOutObj->outNumFrames);
            }
        }
    }

    if (tilerUsed)
    {
        SystemTiler_freeAll();
    }

    for (chId = 0; chId < DEI_LINK_MAX_CH; chId++)
    {
        pChObj = &pObj->chObj[chId];

        Utils_bufDelete(&pChObj->inQue);
    }

    Utils_queDelete(&pObj->reqQue);

#ifdef SYSTEM_DEBUG_DEI
    Vps_printf(" %d: DEI    : Delete Done !!!\n", Utils_getCurTimeInMsec());
#endif

    return FVID2_SOK;
}

Int32 DeiLink_SetFrameRate(DeiLink_Obj * pObj, DeiLink_ChFpsParams * params)
{
    Int32 status = FVID2_SOK;
    DeiLink_ChObj *pChObj;

    if (params->chId < DEI_LINK_MAX_CH)
    {
        pChObj = &pObj->chObj[params->chId];

        /*
             * Stream 0 maps to Queue 0 - DEI_LINK_OUT_QUE_DEI_SC <D1>
             * Stream 1 maps to Queue 1 - DEI_LINK_OUT_QUE_VIP_SC <D1>
             * Stream 2 maps to Queue 2 - DEI_LINK_OUT_QUE_VIP_SC_SECONDARY_OUT <CIF>
             */
        pChObj->frameSkipCtx[params->streamId].firstTime = TRUE;
        pChObj->frameSkipCtx[params->streamId].inputFrameRate = params->inputFrameRate;
        pChObj->frameSkipCtx[params->streamId].outputFrameRate = params->outputFrameRate;
    }
    return (status);
}

Int32 DeiLink_drvSetChannelInfo(DeiLink_Obj * pObj, DeiLink_ChannelInfo *channelInfo)
{
	Int32 status = FVID2_SOK;
    DeiLink_ChObj *pChObj;

	UTILS_assert(channelInfo->channelId < (DEI_LINK_MAX_CH * 2));
	UTILS_assert(channelInfo->streamId  < (DEI_LINK_MAX_OUT_QUE));

    pChObj = &pObj->chObj[channelInfo->channelId];
    pChObj->enableOut[channelInfo->streamId] = channelInfo->enable;

    return status;

}

Int32 DeiLink_drvGetChDynamicOutputRes(DeiLink_Obj * pObj, 
                                       DeiLink_chDynamicSetOutRes * params)
{
    Int32 status = FVID2_SOK;
    DeiLink_ChObj *pChObj;
    UInt32 outId, chId;

    if (params->chId < DEI_LINK_MAX_CH)
    {
        chId = params->chId;
        outId = params->queId;
        pChObj = &pObj->chObj[chId];
        if (outId == DEI_LINK_OUT_QUE_DEI_SC)
        {
            params->width = pChObj->deiRtOutFrmPrm[outId].width;
            params->height = pChObj->deiRtOutFrmPrm[outId].height;
            params->pitch[0] = pChObj->deiRtOutFrmPrm[outId].pitch[0];
            params->pitch[1] = pChObj->deiRtOutFrmPrm[outId].pitch[1];
        }
        else
        {
            params->width = pChObj->vipRtOutFrmPrm[outId].width;
            params->height = pChObj->vipRtOutFrmPrm[outId].height;
            params->pitch[0] = pChObj->vipRtOutFrmPrm[outId].pitch[0];
            params->pitch[1] = pChObj->vipRtOutFrmPrm[outId].pitch[1];
        }
    }
    return (status);
}

Int32 DeiLink_drvSetChDynamicOutputRes(DeiLink_Obj * pObj,
                                       DeiLink_chDynamicSetOutRes * params)
{
    Int32 status = FVID2_SOK;
    DeiLink_ChObj *pChObj;
    UInt32 outId, chId;

    if (params->chId < DEI_LINK_MAX_CH)
    {
        chId = params->chId;
        outId = params->queId;
        pChObj = &pObj->chObj[chId];

        pChObj->chRtOutInfoUpdate[outId] = TRUE;
        pChObj->chRtOutInfoUpdateDropCnt = 0;

        pChObj->vipRtOutFrmPrm[outId].width    = params->width;
        pChObj->vipRtOutFrmPrm[outId].height   = params->height;
        pChObj->vipRtOutFrmPrm[outId].pitch[0] = params->pitch[0];
        pChObj->vipRtOutFrmPrm[outId].pitch[1] = params->pitch[1];

        pChObj->deiRtOutFrmPrm[outId].width    = params->width;
        pChObj->deiRtOutFrmPrm[outId].height   = params->height;
        pChObj->deiRtOutFrmPrm[outId].pitch[0] = params->pitch[0];
        pChObj->deiRtOutFrmPrm[outId].pitch[1] = params->pitch[1];
    }


    return (status);
}

Int32 DeiLink_resetStatistics(DeiLink_Obj *pObj)
{
    UInt32 chId, outId;
    DeiLink_ChObj *pChObj;

    for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
    {
        pChObj = &pObj->chObj[chId];

        pChObj->inFrameRecvCount = 0;
        pChObj->inFrameRejectCount = 0;
        pChObj->inFrameProcessCount = 0;

        for (outId = 0; outId < DEI_LINK_MAX_OUT_QUE; outId++)
        {
            pChObj->outFrameUserSkipCount[outId] = 0;
            pChObj->outFrameSkipCount[outId] = 0;
            pChObj->outFrameCount[outId] = 0;
        }

        pChObj->minLatency = 0xFF;
        pChObj->maxLatency = 0;
    }

    pObj->statsStartTime = Utils_getCurTimeInMsec();

    return 0;
}

Int32 DeiLink_printStatistics (DeiLink_Obj *pObj, Bool resetAfterPrint)
{
    UInt32 chId;
    DeiLink_ChObj *pChObj;
    UInt32 elaspedTime;

    elaspedTime = Utils_getCurTimeInMsec() - pObj->statsStartTime; // in msecs
    elaspedTime /= 1000; // convert to secs

    Vps_printf( " \n"
            " *** [%s] DEI Statistics *** \n"
            " \n"
            " Elasped Time           : %d secs\n"
            " Total Fields Processed : %d \n"
            " Total Fields FPS       : %d FPS\n"
            " \n"
            " \n"
            " CH  | In Recv In Reject In Process Out[0] Out[1] Out[2] Skip Out[0] Skip Out[1] Skip Out[2] User Out[0] User Out[1] User Out[2] Latency   \n"
            " Num | FPS     FPS       FPS        FPS    FPS    FPS        FPS         FPS         FPS       Skip FPS    Skip FPS    Skip FPS  Min / Max \n"
            " ------------------------------------------------------------------------------------------------------------------------------------------\n",
            pObj->name,
            elaspedTime,
                    pObj->getProcessFrameCount,
            pObj->getProcessFrameCount * 100 / (pObj->totalTime / 10)
                    );

	for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
	{
		pChObj = &pObj->chObj[chId];

        Vps_printf( " %3d | %7d %9d %10d %6d %6d %6d %11d %11d %11d %11d %11d %11d %3d / %3d\n",
            chId,
            pChObj->inFrameRecvCount/elaspedTime,
            pChObj->inFrameRejectCount/elaspedTime,
            pChObj->inFrameProcessCount/elaspedTime,
            pChObj->outFrameCount[0]/elaspedTime,
            pChObj->outFrameCount[1]/elaspedTime,
            pChObj->outFrameCount[2]/elaspedTime,
            pChObj->outFrameSkipCount[0]/elaspedTime,
            pChObj->outFrameSkipCount[1]/elaspedTime,
            pChObj->outFrameSkipCount[2]/elaspedTime,
            pChObj->outFrameUserSkipCount[0]/elaspedTime,
            pChObj->outFrameUserSkipCount[1]/elaspedTime,
            pChObj->outFrameUserSkipCount[2]/elaspedTime,
            pChObj->minLatency,
            pChObj->maxLatency
            );
    }

    Vps_printf( " \n");

    if(resetAfterPrint)
    {
        DeiLink_resetStatistics(pObj);
    }
    return FVID2_SOK;
}


#ifdef TI_814X_BUILD
static Int32 DeiLink_drvReleaseContextField(DeiLink_Obj * pObj)
{
    Int32 status, index;
    FVID2_ProcessList processList;
    FVID2_FrameList outFrameList[DEI_LINK_MAX_OUT_QUE];

    for (index = 0x0u; index < DEI_LINK_MAX_OUT_QUE; index++)
    {
        outFrameList[index].numFrames = 0x0u;
    }

    status =
        FVID2_getProcessedFrames(pObj->fvidHandle, &processList, BIOS_NO_WAIT);
    UTILS_assert(status == FVID2_SOK);
    UTILS_assert(processList.numOutLists == 0x0u);

    DeiLink_drvReleaseFrames(pObj, processList.inFrameList[0], outFrameList);

    return FVID2_SOK;
}
#endif                                                     /* TI_814X_BUILD */

Int32 DeiLink_printBufferStatus(DeiLink_Obj * pObj)
{
    Uint8 i, str[256];
    
    Vps_rprintf(
        " \n"
        " *** [%s] DEI Statistics *** \n"
        "%d: DEI: Rcvd from prev = %d, Returned to prev = %d\r\n",
         pObj->name, Utils_getCurTimeInMsec(), pObj->inFrameGetCount, pObj->inFramePutCount);

    for (i=0; i<DEI_LINK_MAX_OUT_QUE; i++)
    {
        sprintf ((char *)str, "DEI Out [%d]", i);
        Utils_bufPrintStatus(str, &pObj->outObj[i].bufOutQue);
    }
    return 0;
}

