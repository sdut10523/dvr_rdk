/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2009 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

#include <xdc/std.h>
#include <ti/xdais/xdas.h>
#include <ti/xdais/ialg.h>
#include <ti/sdo/fc/rman/rman.h>
#include <ti/xdais/dm/ividenc2.h>
#include <ti/xdais/dm/ividdec3.h>
#include <ih264enc.h>
#include <ih264vdec.h>
#include <mcfw/src_bios6/links_m3video/system/systemLink_priv_m3video.h>
#include <mcfw/src_bios6/links_m3video/codec_utils/utils_encdec.h>
#include <mcfw/src_bios6/links_m3video/codec_utils/utils_encdec_prf.h>
#include <mcfw/src_bios6/links_m3video/codec_utils/hdvicp2_config.h>
#include "iresman_tiledmemory.h"
#include "iresman_hdvicp2.h"

#define UTILS_ENCDEC_MAX_IVACH    (128)

typedef struct Utils_encdecAlg2IVAMap {
    Ptr algHandle;
    UInt32 ivaID;
} Utils_encdecAlg2IVAMap;

#ifdef TI_816X_BUILD
static SystemVideo_Ivahd2ChMap_Tbl UtilsEncDec_IvaChMapTbl =
{
    .isPopulated = 1,
    .ivaMap[0] = 
    {
        .EncNumCh  = 10,
        .EncChList = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0 , 0, 0},
        .DecNumCh  = 0, 
        .DecChList = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    },
    .ivaMap[1] = 
    {
        .EncNumCh  = 16, 
        .EncChList = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
        .DecNumCh  = 12,
        .DecChList = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0 , 0, 0},
    },
    .ivaMap[2] = 
    {
        .EncNumCh  = 6, 
        .EncChList = {10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .DecNumCh  = 4, 
        .DecChList = {12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    },
};
#else
#ifdef TI_814X_BUILD
static SystemVideo_Ivahd2ChMap_Tbl UtilsEncDec_IvaChMapTbl =
{
    .isPopulated = 1,
    .ivaMap[0] = 
    {
        .EncNumCh  = 16,
        .EncChList = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 , 14, 15},
        .DecNumCh  = 16, 
        .DecChList = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 , 14, 15},
    },

};

#endif
#endif

Utils_encdecAlg2IVAMap UTILS_ENCDECIVAMAP[UTILS_ENCDEC_MAX_IVACH];
Int initDone = FALSE;

HDVICP_logTbl g_HDVICP_logTbl[UTILS_ENCDEC_MAXNUMOFHDVICP2_RESOUCES];
EncDec_AlgorithmActivityLog AlgorithmActivityLog[UTILS_ENCDEC_MAXNUMOFHDVICP2_RESOUCES];

Int Utils_encdecSetCh2IvahdMap(SystemVideo_Ivahd2ChMap_Tbl* Tbl)
{
    Int retVal = UTILS_ENCDEC_S_SUCCESS;
    
    if (Tbl->isPopulated == TRUE)
    {
        UtilsEncDec_IvaChMapTbl = *Tbl;
    }
    else
    {
        UTILS_warn("ENCDECUTIL: ERROR! CH2IVA TABLE MAP COPY FAILED ");
        retVal = UTILS_ENCDEC_E_FAIL;
    }
    return retVal;
}

EncDec_ResolutionClass Utils_encdecGetResolutionClass(UInt32 width,
                                                    UInt32 height)
{
    EncDec_ResolutionClass resClass = UTILS_ENCDEC_RESOLUTION_CLASS_1080P;

    UTILS_assert((width <= UTILS_ENCDEC_RESOLUTION_CLASS_1080P_WIDTH)
                 && (height <= UTILS_ENCDEC_RESOLUTION_CLASS_1080P_HEIGHT));
    if ((width > UTILS_ENCDEC_RESOLUTION_CLASS_720P_WIDTH) ||
        (height > UTILS_ENCDEC_RESOLUTION_CLASS_720P_HEIGHT))
    {
        resClass = UTILS_ENCDEC_RESOLUTION_CLASS_1080P;
    }
    else
    {
        if ((width > UTILS_ENCDEC_RESOLUTION_CLASS_D1_WIDTH) ||
            (height > UTILS_ENCDEC_RESOLUTION_CLASS_D1_HEIGHT))
        {
            resClass = UTILS_ENCDEC_RESOLUTION_CLASS_720P;
        }
        else
        {
            if ((width > UTILS_ENCDEC_RESOLUTION_CLASS_CIF_WIDTH) ||
                (height > UTILS_ENCDEC_RESOLUTION_CLASS_CIF_HEIGHT))
            {
                resClass = UTILS_ENCDEC_RESOLUTION_CLASS_D1;
            }
            else
            {
                resClass = UTILS_ENCDEC_RESOLUTION_CLASS_CIF;
            }
        }
    }
    return resClass;
}

Int Utils_encdecGetCodecLevel(UInt32 codingFormat,
                              UInt32 maxWidth,
                              UInt32 maxHeight,
                              UInt32 maxFrameRate,
                              UInt32 maxBitRate, Int32 * pLevel,
                              Bool   isEnc)
{
    Int retVal = UTILS_ENCDEC_S_SUCCESS;

    (Void) maxWidth;
    (Void) maxHeight;
    (Void) maxFrameRate;
    (Void) maxBitRate;

    switch (codingFormat)
    {
        case IVIDEO_H264BP:
        case IVIDEO_H264MP:
        case IVIDEO_H264HP:
            switch (Utils_encdecGetResolutionClass(maxWidth, maxHeight))
            {
                case UTILS_ENCDEC_RESOLUTION_CLASS_1080P: 
                case UTILS_ENCDEC_RESOLUTION_CLASS_CIF:
                    if (isEnc)
                        *pLevel = IH264_LEVEL_41;
                    else
                        *pLevel = IH264VDEC_LEVEL41;
                    break;
                case UTILS_ENCDEC_RESOLUTION_CLASS_720P:
                case UTILS_ENCDEC_RESOLUTION_CLASS_D1:
                    if (isEnc)
                        *pLevel = IH264_LEVEL_31;
                    else
                        *pLevel = IH264VDEC_LEVEL31;
                    break;
                default:
                    if (isEnc)
                        *pLevel = IH264_LEVEL_41;
                    else
                        *pLevel = IH264VDEC_LEVEL41;
                    break;
            }
            break;
        default:
            *pLevel = IVIDENC2_DEFAULTLEVEL;
            retVal = UTILS_ENCDEC_E_UNKNOWNCODINGTFORMAT;
            break;
    }
    return retVal;
}

Int Utils_encdecInit()
{
    IRES_Status iresStatus;
    Int i;

    if (FALSE == initDone)
    {
        HDVICP2_Init();
        iresStatus = RMAN_init();
        UTILS_assert(iresStatus == IRES_OK);

        iresStatus = IRESMAN_TiledMemoryResourceRegister();
        UTILS_assert(iresStatus == IRES_OK);

        for (i = 0; i < UTILS_ENCDEC_MAX_IVACH; i++)
        {
            UTILS_ENCDECIVAMAP[i].algHandle = NULL;
        }
        IRESMAN_HDVICP2_EarlyAcquireInit();
        memset (AlgorithmActivityLog, 0, sizeof (AlgorithmActivityLog) * 
                UTILS_ENCDEC_MAXNUMOFHDVICP2_RESOUCES);
        initDone = TRUE;
    }
    return 0;
}

Int Utils_encdecDeInit()
{
    IRES_Status iresStatus;
    Int i;

    if (TRUE == initDone)
    {
        iresStatus = IRESMAN_TiledMemoryResourceUnregister();
        UTILS_assert(iresStatus == IRES_OK);

        for (i = 0; i < UTILS_ENCDEC_MAX_IVACH; i++)
        {
            UTILS_ENCDECIVAMAP[i].algHandle = NULL;
        }

        IRESMAN_HDVICP2_EarlyAcquireDeInit();

        iresStatus = RMAN_exit();
        UTILS_assert(iresStatus == IRES_OK);
        initDone = FALSE;
    }
    Utils_encdecHdvicpPrfInit();

    return 0;
}

Int Utils_encdecGetEncoderIVAID(UInt32 chId)
{
    UInt32 ivaId, i, j;

    ivaId = 0;
    for (j=0; j<NUM_HDVICP_RESOURCES; j++)
    {
        for (i=0; i<UtilsEncDec_IvaChMapTbl.ivaMap[j].EncNumCh; i++)
        {
            if (chId == UtilsEncDec_IvaChMapTbl.ivaMap[j].EncChList[i])
            {
                ivaId = j;
                break;
            }
        }
    }
    if (UtilsEncDec_IvaChMapTbl.isPopulated != TRUE)
    {
        UTILS_warn("ENCDECUTIL: ERROR! ENCODER CH2IVA TABLE MAP APPLY FAILED ");
    }

    return (ivaId);
}

Int Utils_encdecGetDecoderIVAID(UInt32 chId)
{
    UInt32 ivaId, i, j;

    ivaId = 0;
    for (j=0; j<NUM_HDVICP_RESOURCES; j++)
    {
        for (i=0; i<UtilsEncDec_IvaChMapTbl.ivaMap[j].DecNumCh; i++)
        {
            if (chId == UtilsEncDec_IvaChMapTbl.ivaMap[j].DecChList[i])
            {
                ivaId = j;
                break;
            }
        }
    }
    if (UtilsEncDec_IvaChMapTbl.isPopulated != TRUE)
    {
        UTILS_warn("ENCDECUTIL: ERROR! DECODER CH2IVA TABLE MAP APPLY FAILED ");
    }

    return (ivaId);
}

Int Utils_encdecRegisterAlgHandle(UInt32 ivaChID, Ptr algHandle)
{
    UInt32 ivaID;
    UInt32 chId;

    UTILS_assert(ivaChID < UTILS_ENCDEC_MAX_IVACH);

    if (ivaChID < UTILS_ENCDEC_DECODER_IVACH_BASE)
    {
        chId = ivaChID;
        ivaID = Utils_encdecGetEncoderIVAID (chId);
    }
    else
    {
        chId = ivaChID - UTILS_ENCDEC_DECODER_IVACH_BASE;
        ivaID = Utils_encdecGetDecoderIVAID (chId);
    }
    
    UTILS_ENCDECIVAMAP[ivaChID].ivaID = ivaID;
    UTILS_ENCDECIVAMAP[ivaChID].algHandle = algHandle;

    return (UTILS_ENCDEC_S_SUCCESS);

}

/**
********************************************************************************
 *  @func   Utils_encdecGetIVAID()
 *  @brief  Framework Componnet callable APIs, It serves as interface between
 *          codec and Iva scheduler.
 *
 * @param [in] algHandle      : codec handle
 * @param [in] hdvicpHandle   : hdvicp handle from framework component
 * @param [out] id            : returned ivahd id  pointer
 *
 *  @return
 *  0 = Successful
 *
 *
********************************************************************************
*/
/* =========================================================================== */

Int32 Utils_encdecGetIVAID(Int32 * id, Ptr algHandle, Ptr hdvicpHandle)
{
    Int i;

    *id = 0;
    for (i = 0; i < UTILS_ENCDEC_MAX_IVACH; i++)
    {
        if (algHandle == UTILS_ENCDECIVAMAP[i].algHandle)
        {
            *id = UTILS_ENCDECIVAMAP[i].ivaID;
        }
    }
    return (UTILS_ENCDEC_S_SUCCESS);
}

/**
********************************************************************************
 *  @func   Utils_encdecReleaseIVAID()
 *  @brief  Framework Componnet callable APIs, It serves as interface between
 *          codec and Iva scheduler, currently a placeholder for enhancement
 *
 * @param [in] algHandle      : codec handle
 * @param [in] hdvicpHandle   : hdvicp handle from framework component
 * @param [out] id            : ivahd id
 *
 *  @return
 *  0 = Successful
 *
 *  Other_value = Failed (Error code is returned)
 *
********************************************************************************
*/
/* =========================================================================== */

Int32 Utils_encdecReleaseIVAID(Int32 id, Ptr algHandle, Ptr hdvicpHandle)
{
    return (UTILS_ENCDEC_S_SUCCESS);
}

/* Yield function to be used when yielding context and reacquiring it to run
 * again */
Void Utils_encdecDummyContextRelease(IRES_YieldResourceType resource,
                                     IRES_YieldContextHandle algYieldContext,
                                     IRES_YieldArgs yieldArgs)
{
    (Void) resource;
    (Void) algYieldContext;
    (Void) yieldArgs;
    /* Do nothing */

}

Void Utils_encdecDummyContextAcquire(IRES_YieldResourceType resource,
                                     IRES_YieldContextHandle algYieldContext,
                                     IRES_YieldArgs yieldArgs)
{
    (Void) resource;
    (Void) algYieldContext;
    (Void) yieldArgs;
    /* Do nothing */
}

UInt32 Utils_encdecMapIVACh2IVAResId(UInt32 ivaChID)
{
    UTILS_assert(ivaChID < UTILS_ENCDEC_MAX_IVACH);
    return (UTILS_ENCDECIVAMAP[ivaChID].ivaID);
}
