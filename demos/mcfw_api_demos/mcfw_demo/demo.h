/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2011 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/



#ifndef _DEMO_H_
#define _DEMO_H_

#include <osa.h>
#include <osa_thr.h>
#include <osa_sem.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>  // For stat().
#include <sys/stat.h>   // For stat().
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "ti_vdis_common_def.h"

#include "ti_vsys.h"
#include "ti_vcap.h"
#include "ti_venc.h"
#include "ti_vdec.h"
#include "ti_vdis.h"
#include "ti_vdis_timings.h"

#include "demos/graphic/graphic.h"
#include "demos/audio_sample/audio.h"
#include "demos/audio_sample/alg_uLawCodec.h"
#include "demos/display_process/display_process.h"

#include <demos/mcfw_api_demos/mcfw_demo/demo_swms.h>


/* To select if FBDEV interface is used for Graphics */
#ifdef TI_814X_BUILD
#define USE_FBDEV   1
#else
#define USE_FBDEV   0
#endif

#define DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE    (0)
#define DEMO_VCAP_VENC_VDEC_VDIS_INTERLACED     (1)
#define DEMO_VCAP_VENC_VDIS                     (2)
#define DEMO_VCAP_VENC_VDIS_HD                  (3)
#define DEMO_VDEC_VDIS                          (4)
#define DEMO_VCAP_VDIS                          (5)
#define DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_NON_D1    (6)
#define DEMO_CUSTOM_0                           (7)
#define DEMO_CUSTOM_1                           (8)
#define DEMO_CUSTOM_2                           (9)
#define DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_4CH (10)
#define DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH (11)
#define DEMO_MAX                                (12)

#define MAX_INPUT_STR_SIZE                      (128)
#define DEMO_SW_MS_INVALID_ID                   (0xFF)


#define SCD_MAX_TILES                           (9)
#define SCD_MAX_CHANNELS                        (16)

/* The below part is temporary for OSD specific items */
#define DEMO_OSD_NUM_WINDOWS     (3)
#define DEMO_OSD_WIN_MAX_WIDTH   (320)
#define DEMO_OSD_WIN_MAX_HEIGHT  (64)
#define DEMO_OSD_WIN_WIDTH       (208)//(160)
#define DEMO_OSD_WIN_HEIGHT      (32)
#define DEMO_OSD_WIN0_STARTX     (16)
#define DEMO_OSD_WIN0_STARTY     (16)

#define DEMO_OSD_WIN_PITCH       (DEMO_OSD_WIN_WIDTH)
#define DEMO_OSD_TRANSPARENCY    (0)
#define DEMO_OSD_GLOBAL_ALPHA    (0x80)
#define DEMO_OSD_ENABLE_WIN      (1)


typedef enum {
    DEMO_TYPE_PROGRESSIVE,
    DEMO_TYPE_INTERLACED
}demoType;

typedef struct {

    UInt32 maxVencChannels;
    UInt32 maxVcapChannels;
    UInt32 maxVdisChannels;
    UInt32 maxVdecChannels;
    UInt32 VsysNumChs;
    UInt32 VsysCifOnly;

    Bool   audioCaptureActive;
    Bool   audioPlaybackActive;
    Int8   audioCaptureChNum;
    Int8   audioPlaybackChNum;
    Bool   isAudioPathSet;
    Bool   audioEnable;
	Bool   osdEnable;
    Bool   scdTileConfigInitFlag;
    demoType Type;

} Demo_Info;

extern Demo_Info gDemo_info;
extern AlgLink_OsdChWinParams g_osdChParam[];


Void  VcapVdis_start();
Void  VcapVdis_stop();

Void  VcapVenc_start(Bool hdDemo);
Void  VcapVenc_stop();
Int32 VcapVenc_printStatistics(Bool resetStats);
Int32 Scd_printStatistics(Bool resetStats);


Void  VdecVdis_start();
Void  VdecVdis_stop();

Void  VcapVencVdecVdis_start( Bool doProgressiveVenc, Bool enableSecondaryOut, int demoId );
Void  VcapVencVdecVdis_stop();
Int32 VcapVencVdecVdis_printStatistics(Bool resetStats, Bool allChs);

Void VcapVencVdecVdisCustom_start(Bool enableDecode, Bool enableCapture, UInt32 numVipInst);
Void VcapVencVdecVdisCustom_stop();

void  Demo_generateBitsHdrFile(char *filename);

Int32 Demo_swMsGetOutSize(UInt32 outRes, UInt32 * width, UInt32 * height);

int Demo_audioCaptureStop();
int Demo_audioCaptureSetParams(Bool set_params);
int Demo_audioSetParams();
int Demo_audioPlaybackStop();
int Demo_audioCaptureStart(UInt32 chId);
int Demo_audioPlaybackStart(UInt32 chId, UInt32 playbackDevId);
int Demo_audioSettings(int demoId);
int Demo_audioEnable(Bool enable);

int Demo_captureSettings(int demoId);
int Demo_captureGetVideoSourceStatus();
int Demo_captureGetTamperStatus(Ptr pPrm);
int Demo_captureGetMotionStatus(Ptr pPrm);
VSYS_VIDEO_STANDARD_E Demo_captureGetSignalStandard();

int Demo_encodeSettings(int demoId);

int Demo_decodeSettings(int demoId);

int Demo_displaySwitchChn(int devId, int startChId);
int Demo_displaySwitchSDChan(int devId, int startChId);
int Demo_displayChnEnable(int chId, Bool enable);
int Demo_displayGetLayoutId(int demoId);
int Demo_displaySetResolution(UInt32 displayId, UInt32 resolution);
int Demo_displaySettings(int demoId);

Int32 Demo_osdInit(UInt32 numCh, UInt8 *osdFormat);
Void  Demo_osdDeinit();

int  Demo_printInfo(int demoId);
int Demo_printBuffersInfo();
int  Demo_startStop(int demoId, Bool startDemo);
int  Demo_run(int demoId);
char Demo_getChar();
int  Demo_getChId(char *string, int maxChId);
int  Demo_getIntValue(char *string, int minVal, int maxVal, int defaultVal);
Bool Demo_getFileWriteEnable();
Bool Demo_isPathValid( const char* absolutePath );
int  Demo_getFileWritePath(char *path, char *defaultPath);
Void VdecVdis_setTplayConfig(VDIS_CHN vdispChnId, VDIS_AVSYNC speed);

#ifdef TI_814X_BUILD
int Demo_change8ChMode(int demoId);
#endif

#endif /* TI_MULTICH_DEMO_H */
