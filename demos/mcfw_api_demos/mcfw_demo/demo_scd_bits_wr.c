/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2011 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/



#include <demo_scd_bits_wr.h>

#define FILE_WRITE_STOPPED  (0)
#define FILE_WRITE_RUNNING  (1)

Int32 Scd_resetStatistics()
{
    UInt32 chId;
    Scd_ChInfo *pChInfo;

    for(chId=0; chId<VENC_CHN_MAX; chId++)
    {
        pChInfo = &gScd_ctrl.chInfo[chId];

        pChInfo->totalDataSize = 0;
        pChInfo->numKeyFrames = 0;
        pChInfo->numFrames = 0;
        pChInfo->maxWidth = 0;
        pChInfo->minWidth = 0xFFF;
        pChInfo->maxHeight= 0;
        pChInfo->minHeight= 0xFFF;
        pChInfo->maxLatency= 0;
        pChInfo->minLatency= 0xFFF;

    }

    gScd_ctrl.statsStartTime = OSA_getCurTimeInMsec();

    return 0;
}

Int32 Scd_printStatistics(Bool resetStats)
{
    UInt32 chId;
    Scd_ChInfo *pChInfo;
    float elaspedTime;

    elaspedTime = OSA_getCurTimeInMsec() - gScd_ctrl.statsStartTime;

    elaspedTime /= 1000.0; // in secs

    printf( "\n"
            "\n *** SCD Bitstream Received Statistics *** "
            "\n"
            "\n Elased time = %6.1f secs"
            "\n"
            "\n CH | Bitrate (Kbps) | FPS | Key-frame FPS | Width (max/min) | Height (max/min) | Latency (max/min)"
            "\n --------------------------------------------------------------------------------------------------",
            elaspedTime
            );

    for(chId=0; chId<VENC_CHN_MAX;chId++)
    {
        pChInfo = &gScd_ctrl.chInfo[chId];

        if(pChInfo->numFrames)
        {
            printf("\n %2d | %14.2f | %3.1f | %13.1f | %5d / %6d | %6d / %6d  | %6d / %6d",
                chId,
                (pChInfo->totalDataSize*8.0/elaspedTime)/1024.0,
                pChInfo->numFrames*1.0/elaspedTime,
                pChInfo->numKeyFrames*1.0/elaspedTime,
                pChInfo->maxWidth,
                pChInfo->minWidth,
                pChInfo->maxHeight,
                pChInfo->minHeight,
                pChInfo->maxLatency,
                pChInfo->minLatency

                );
        }
    }

    printf("\n");

    if(resetStats)
        Scd_resetStatistics();

    return 0;
}

Void Scd_bitsWriteCbFxn(Ptr pPrm)
{
    OSA_semSignal(&gScd_ctrl.wrSem);
}

void *Scd_bitsWriteMain(void *pPrm)
{
    Int32 status, frameId;
    VCODEC_BITSBUF_LIST_S bitsBuf;
    VCODEC_BITSBUF_S *pBuf;
    Scd_ChInfo *pChInfo;
    UInt32 latency;

    FILE *fp = NULL;
    UInt32 fileWriteState = FILE_WRITE_STOPPED, writeDataSize;

    if(gScd_ctrl.fileWriteEnable)
    {
        fp = fopen(gScd_ctrl.fileWriteName, "wb");
        if(fp!=NULL)
        {
            fileWriteState = FILE_WRITE_RUNNING;
            printf(" Opened file [%s] for writing CH%d\n", gScd_ctrl.fileWriteName, gScd_ctrl.fileWriteChn);
        }
        else
        {
            printf(" ERROR: File open [%s] for writing CH%d FAILED !!!!\n", gScd_ctrl.fileWriteName, gScd_ctrl.fileWriteChn);
        }
    }

    while(!gScd_ctrl.exitWrThr)
    {
        status = OSA_semWait(&gScd_ctrl.wrSem, OSA_TIMEOUT_FOREVER);
        if(status!=OSA_SOK)
            break;
        status = Vcap_getBitstreamBuffer(&bitsBuf, TIMEOUT_NO_WAIT);
	
        if(status==ERROR_NONE && bitsBuf.numBufs)
        {

            for(frameId=0; frameId<bitsBuf.numBufs; frameId++)
            {
		 pBuf = &bitsBuf.bitsBuf[frameId];
                if(pBuf->chnId<VENC_CHN_MAX)
                {
                    pChInfo = &gScd_ctrl.chInfo[pBuf->chnId];

                    pChInfo->totalDataSize += pBuf->filledBufSize;
                    pChInfo->numFrames++;
                    if(pBuf->frameType==VCODEC_FRAME_TYPE_I_FRAME)
                    {
                        pChInfo->numKeyFrames++;
                    }

                    latency = pBuf->encodeTimestamp - pBuf->timestamp;

                    if(latency > pChInfo->maxLatency)
                        pChInfo->maxLatency = latency;

                    if(latency < pChInfo->minLatency)
                        pChInfo->minLatency = latency;

                    if(pBuf->frameWidth > pChInfo->maxWidth)
                        pChInfo->maxWidth = pBuf->frameWidth;

                    if(pBuf->frameWidth < pChInfo->minWidth)
                        pChInfo->minWidth = pBuf->frameWidth;

                    if(pBuf->frameHeight > pChInfo->maxHeight)
                        pChInfo->maxHeight = pBuf->frameHeight;

                    if(pBuf->frameHeight < pChInfo->minHeight)
                        pChInfo->minHeight = pBuf->frameHeight;

                }

                if(gScd_ctrl.fileWriteEnable);
                {
                    if(pBuf->chnId == gScd_ctrl.chId && fileWriteState == FILE_WRITE_RUNNING)
                    {
                        writeDataSize = fwrite(pBuf->bufVirtAddr, 1, pBuf->filledBufSize, fp);
                        if(writeDataSize!=pBuf->filledBufSize)
                        {
                            fileWriteState = FILE_WRITE_STOPPED;
                            fclose(fp);
                            printf(" Closing file [%s] for CH%d\n", gScd_ctrl.fileWriteName, gScd_ctrl.fileWriteChn);
                        }
                    }
                }
            }

            Vcap_releaseBitstreamBuffer(&bitsBuf);
        }
    }

    gScd_ctrl.isWrThrStopDone = TRUE;

    if(gScd_ctrl.fileWriteEnable)
    {
        if(fileWriteState==FILE_WRITE_RUNNING)
        {
            fclose(fp);
            printf(" Closing file [%s] for CH%d\n", gScd_ctrl.fileWriteName, gScd_ctrl.fileWriteChn);
        }
    }

    return NULL;
}

Int32 Scd_bitsWriteCreate()
{
    VCAP_CALLBACK_S callback;

    Int32 status;

    gScd_ctrl.fileWriteChn = 0;
    //printf("\nEnable SCD block metadata Write\n\n");
   /*Disable SCD block metadata Write*/
    gScd_ctrl.fileWriteEnable =FALSE;//Demo_getFileWriteEnable();

    if(gScd_ctrl.fileWriteEnable)
    {
        char path[256];

        Demo_getFileWritePath(path, "/dev/shm");

        gScd_ctrl.fileWriteChn = Demo_getChId("File Write", gDemo_info.maxVcapChannels);

        sprintf(gScd_ctrl.fileWriteName, "%s/VID_CH%02d.bin", path, gScd_ctrl.fileWriteChn);

        gScd_ctrl.chId = gScd_ctrl.fileWriteChn + gDemo_info.maxVcapChannels;
    }

    gScd_ctrl.exitWrThr = FALSE;
    gScd_ctrl.isWrThrStopDone = FALSE;

    callback.newDataAvailableCb = Scd_bitsWriteCbFxn;

    /* Register call back with encoder */
    Vcap_registerBitsCallback(&callback,
                         (Ptr)&gScd_ctrl);

    status = OSA_semCreate(&gScd_ctrl.wrSem, 1, 0);
    OSA_assert(status==OSA_SOK);

    status = OSA_thrCreate(
        &gScd_ctrl.wrThrHndl,
        Scd_bitsWriteMain,
        OSA_THR_PRI_DEFAULT,
        0,
        &gScd_ctrl
        );

    OSA_assert(status==OSA_SOK);

    OSA_waitMsecs(100); // allow for print to complete
    return OSA_SOK;
}

Void Scd_bitsWriteStop()
{
    gScd_ctrl.exitWrThr = TRUE;
}
Int32 Scd_bitsWriteDelete()
{

    gScd_ctrl.exitWrThr = TRUE;
    OSA_semSignal(&gScd_ctrl.wrSem);


    while(!gScd_ctrl.isWrThrStopDone)
    {
        OSA_waitMsecs(10);
    }

    OSA_thrDelete(&gScd_ctrl.wrThrHndl);
    OSA_semDelete(&gScd_ctrl.wrSem);

    return OSA_SOK;
}



