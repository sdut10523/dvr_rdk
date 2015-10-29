
#include <demo.h>

char gDemo_mainMenu[] = {
    "\r\n ========="
    "\r\n Main Menu"
    "\r\n ========="
    "\r\n"
#ifdef TI_814X_BUILD
    "\r\n 1: 4CH VCAP + VENC + VDEC + VDIS  - Progressive SD Encode + Decode"
    "\r\n 2: 8CH <D1+CIF> VCAP + VENC + VDEC + VDIS  - Progressive SD Encode + Decode"
    "\r\n 3: 16CH <D1+CIF> VCAP + VENC + VDEC + VDIS  - Progressive SD Encode + Decode"
    "\r\n 4: CUSTOM DEMO - 2Ch D1 Encode"
    "\r\n 5: CUSTOM DEMO - 1Ch D1 + 4CIF Encode"
    "\r\n 6: CUSTOM DEMO - 1Ch D1 Decode"
    "\r\n 7:               VDEC + VDIS  - SD/HD Decode ONLY"
#else
    "\r\n 1: VCAP + VENC + VDEC + VDIS  - Progressive SD Encode + Decode"
#if 0
    "\r\n 2: VCAP + VENC + VDEC + VDIS  - Interlaced  SD Encode + Decode"
#endif
    "\r\n 3: VCAP + VENC        + VDIS  - SD Encode ONLY"
    "\r\n 4: VCAP + VENC        + VDIS  - HD Encode ONLY"
    "\r\n 5:               VDEC + VDIS  - SD/HD Decode ONLY"
    "\r\n 6: VCAP               + VDIS  - NO Encode or Decode"
    "\r\n 7: CUSTOM DEMO - 2Ch D1 Encode"
    "\r\n 8: CUSTOM DEMO - 1Ch D1 + 4CIF Encode"
    "\r\n 9: CUSTOM DEMO - 1Ch D1 Decode"
#if 0
    "\r\n a: 4CH VCAP + VENC + VDEC + VDIS  - Progressive SD Encode + Decode"
#endif
#endif
    "\r\n"
    "\r\n e: Exit"
    "\r\n"
    "\r\n Enter Choice: "
};

char gDemo_runMenu[] = {
    "\r\n ============="
    "\r\n Run-Time Menu"
    "\r\n ============="
    "\r\n"
    "\r\n 1: Capture Settings"
    "\r\n 2: Encode  Settings"
    "\r\n 3: Decode  Settings"
    "\r\n 4: Display Settings"
    "\r\n 5: Audio   Settings"
#ifdef TI_814X_BUILD
    "\r\n c: Change modes (8CH usecase ONLY!!!!)"
#endif
    "\r\n"
    "\r\n i: Print detailed system information"
//    "\r\n b: Print link buffers statistics"
    "\r\n"
    "\r\n e: Stop Demo"
    "\r\n"
    "\r\n Enter Choice: "
};

Demo_Info gDemo_info;

int main()
{
    Bool done = FALSE;
    char ch;
    Bool first=FALSE;
    while(!done)
    {
        printf(gDemo_mainMenu);
        if(first){
        	ch = '6';
        	first=FALSE;
        }
        else{
        	ch = Demo_getChar();
        }


        switch(ch)
        {
#ifdef TI_814X_BUILD
            case '1':
                Demo_run(DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE);
                break;
            case '2':
                Demo_run(DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH);
                break;
            case '3':
                Demo_run(DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_NON_D1);
                break;
            case '4':
                Demo_run(DEMO_CUSTOM_0);
                break;
            case '5':
                Demo_run(DEMO_CUSTOM_1);
                break;
            case '6':
                Demo_run(DEMO_CUSTOM_2);
                break;
            case '7':
                Demo_run(DEMO_VDEC_VDIS);
                break;
#else
            case '1':
                Demo_run(DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE);
                break;
            case '2':
                done = TRUE;
                // this demo is not support now
                // Demo_run(DEMO_VCAP_VENC_VDEC_VDIS_INTERLACED);
                break;
            case '3':
                Demo_run(DEMO_VCAP_VENC_VDIS);
                break;
            case '4':
                Demo_run(DEMO_VCAP_VENC_VDIS_HD);
                break;
            case '5':
                Demo_run(DEMO_VDEC_VDIS);
                break;
            case '6':
                Demo_run(DEMO_VCAP_VDIS);
                break;
            case '7':
                Demo_run(DEMO_CUSTOM_0);
                break;
            case '8':
                Demo_run(DEMO_CUSTOM_1);
                break;
            case '9':
                Demo_run(DEMO_CUSTOM_2);
                break;
            case 'a':
                // this demo is not support now
                // Demo_run(DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_4CH);
                break;
#endif
            case 'e':
                done = TRUE;
                break;
        }
    }

    return 0;
}

Int32 Demo_eventHandler(UInt32 eventId, Ptr pPrm, Ptr appData)
{
    if(eventId==VSYS_EVENT_VIDEO_DETECT)
    {
        printf(" \n");
        printf(" DEMO: Received event VSYS_EVENT_VIDEO_DETECT [0x%04x]\n", eventId);

        Demo_captureGetVideoSourceStatus();
    }

    if(eventId==VSYS_EVENT_TAMPER_DETECT)
    {
        Demo_captureGetTamperStatus(pPrm);
    }

    if(eventId==VSYS_EVENT_MOTION_DETECT)
    {
        Demo_captureGetMotionStatus(pPrm);
    }


    return 0;
}

int Demo_run(int demoId)
{
    int status;
    Bool done = FALSE;
    char ch;

    gDemo_info.scdTileConfigInitFlag = FALSE;
    status = Demo_startStop(demoId, TRUE);
    if(status<0)
    {
        printf(" WARNING: This demo is NOT curently supported !!!\n");
        return status;
    }
    ///////////////////////////////
    int chId;
    for(chId=4;chId<16;chId++){
        /* primary stream */
        Vcap_disableChn(chId, 0);

        /* secondary stream */
        Vcap_disableChn(chId, 1);
    }
    int displayId=0;
    Demo_displaySetResolution(displayId,VSYS_STD_XGA_60);

    int layoutId=DEMO_LAYOUT_MODE_4CH;
    int devId=VDIS_DEV_HDMI;
    int startChId=0;
    VDIS_MOSAIC_S vdMosaicParam;
    Demo_swMsGenerateLayout(devId, startChId, gDemo_info.maxVdisChannels,
             layoutId, &vdMosaicParam, FALSE,
             gDemo_info.Type, Vdis_getResolution(devId));
    Vdis_setMosaicParams(devId,&vdMosaicParam);
    chId=0;
    Vcap_setDynamicParamChn(chId, &g_osdChParam[chId], VCAP_OSDWINPRM);
    ////////////////////////////////
    while(!done)
    {
        printf(gDemo_runMenu);


        ch = Demo_getChar();

        switch(ch)
        {
            case '1':
                Demo_captureSettings(demoId);
                break;
            case '2':
                Demo_encodeSettings(demoId);
                break;
            case '3':
                Demo_decodeSettings(demoId);
                break;
            case '4':
                Demo_displaySettings(demoId);
                break;
            #ifndef SYSTEM_DISABLE_AUDIO
            case '5':
                Demo_audioSettings(demoId);
                break;
            #endif
            #ifdef TI_814X_BUILD
            case 'c':
                Demo_change8ChMode(demoId);
                break;
            #endif
            case 'i':
                Demo_printInfo(demoId);
                break;
            case 'b':
                Demo_printBuffersInfo();
                break;
            case 'e':
                done = TRUE;
                break;
        }
    }

    Demo_startStop(demoId, FALSE);

    return 0;
}

int Demo_startStop(int demoId, Bool startDemo)
{
    if(startDemo)
    {
        gDemo_info.maxVcapChannels = 0;
        gDemo_info.maxVdisChannels = 0;
        gDemo_info.maxVencChannels = 0;
        gDemo_info.maxVdecChannels = 0;

        gDemo_info.audioEnable = FALSE;
        gDemo_info.isAudioPathSet = FALSE;
        gDemo_info.audioCaptureActive = FALSE;
        gDemo_info.audioPlaybackActive = FALSE;
        gDemo_info.audioPlaybackChNum = 0;
        gDemo_info.audioCaptureChNum = 0;
		gDemo_info.osdEnable = FALSE;
    }

    switch(demoId)
    {
#ifdef TI_816X_BUILD
        case DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE:
            if(startDemo)
            {
        #ifndef SYSTEM_DISABLE_AUDIO
                gDemo_info.audioEnable = TRUE;
        #endif
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVencVdecVdis_start(TRUE, TRUE, demoId);
            }
            else
            {
                VcapVencVdecVdis_stop();
            }
            break;
        case DEMO_VCAP_VENC_VDEC_VDIS_INTERLACED:
            if(startDemo)
            {
        #ifndef SYSTEM_DISABLE_AUDIO
                gDemo_info.audioEnable = TRUE;
        #endif
                gDemo_info.Type = DEMO_TYPE_INTERLACED;
                VcapVencVdecVdis_start(FALSE, TRUE, demoId);
            }
            else
            {
                VcapVencVdecVdis_stop();
            }
            break;
        case DEMO_VCAP_VENC_VDIS:
            if(startDemo)
            {
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVenc_start(FALSE);
            }
            else
            {
                VcapVenc_stop();
            }
            break;
        case DEMO_VCAP_VENC_VDIS_HD:
            if(startDemo)
            {
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVenc_start(TRUE);
            }
            else
            {
                 VcapVenc_stop();
            }
            break;
        case DEMO_VCAP_VDIS:
            if(startDemo)
            {
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVdis_start();
            }
            else
            {
                VcapVdis_stop();
            }
            break;

        case DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_4CH:
            if(startDemo)
            {
    #ifndef SYSTEM_DISABLE_AUDIO
                gDemo_info.audioEnable = TRUE;
    #endif
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVencVdecVdis_start(TRUE, TRUE, demoId);
            }
            else
            {
                VcapVencVdecVdis_stop();
            }
            break;
#else   /* TI_814X_BUILD */
        case DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE:
            if(startDemo)
            {
#ifndef SYSTEM_DISABLE_AUDIO
                gDemo_info.audioEnable = TRUE;
#endif
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVencVdecVdis_start(TRUE, TRUE, demoId);
            }
            else
            {
                VcapVencVdecVdis_stop();
            }
            break;
        case DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH:
            if(startDemo)
            {
                #ifndef SYSTEM_DISABLE_AUDIO
                gDemo_info.audioEnable = TRUE;
                #endif
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVencVdecVdis_start(TRUE, TRUE, demoId);
            }
            else
            {
                VcapVencVdecVdis_stop();
            }
            break;
        case DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_NON_D1:
            if(startDemo)
            {
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VcapVencVdecVdis_start(TRUE, TRUE, demoId);
            }
            else
            {
                VcapVencVdecVdis_stop();
            }
            break;
#endif
        case DEMO_VDEC_VDIS:
            if(startDemo)
            {
                gDemo_info.Type = DEMO_TYPE_PROGRESSIVE;
                VdecVdis_start();
            }
            else
            {
                VdecVdis_stop();
            }
            break;
        case DEMO_CUSTOM_0:
            if(startDemo)
                VcapVencVdecVdisCustom_start(FALSE, TRUE, 1);
            else
                VcapVencVdecVdisCustom_stop();

            break;
        case DEMO_CUSTOM_1:
            if(startDemo)
                VcapVencVdecVdisCustom_start(FALSE, TRUE, 2);
            else
                VcapVencVdecVdisCustom_stop();

            break;
        case DEMO_CUSTOM_2:
            if(startDemo)
//////////////////////////////
                VcapVencVdecVdisCustom_start(TRUE, FALSE, 2);  /////2
/////////////////////////
            else
                VcapVencVdecVdisCustom_stop();

            break;
        default:
            return -1;
    }

    if(startDemo)
    {
        Vsys_registerEventHandler(Demo_eventHandler, NULL);
    }

    return 0;
}

int Demo_printBuffersInfo()
{
    Vsys_printBufferStatistics();
    return 0;
}

int Demo_printInfo(int demoId)
{

    Demo_captureGetVideoSourceStatus();
    Vsys_printDetailedStatistics();

    switch(demoId)
    {
        case DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE:
            VcapVencVdecVdis_printStatistics(TRUE, TRUE);
            Scd_printStatistics(TRUE);
            break;

        case DEMO_VCAP_VENC_VDEC_VDIS_INTERLACED:
            break;

        case DEMO_CUSTOM_0:
        case DEMO_CUSTOM_1:
        case DEMO_CUSTOM_2:
        case DEMO_VCAP_VENC_VDIS:
        case DEMO_VCAP_VENC_VDIS_HD:
            VcapVenc_printStatistics(TRUE);
            break;

        case DEMO_VDEC_VDIS:
            break;

        case DEMO_VCAP_VDIS:
            break;
#if TI_814X_BUILD
        case DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH:
            VcapVencVdecVdis_printStatistics(TRUE, TRUE);
            if(demoId == DEMO_VCAP_VENC_VDEC_VDIS_PROGRESSIVE_8CH)
               Scd_printStatistics(TRUE);
            break;
#endif
    }

    if(gDemo_info.audioEnable && gDemo_info.isAudioPathSet)
    {
        #ifndef SYSTEM_DISABLE_AUDIO
        Audio_capturePrintStats();
        Audio_playPrintStats();
        Audio_G711codecPrintStats();
        Audio_pramsPrint();
        #endif
    }

    return 0;
}



char Demo_getChar()
{
    char buffer[MAX_INPUT_STR_SIZE];

    fflush(stdin);
    fgets(buffer, MAX_INPUT_STR_SIZE, stdin);

    return(buffer[0]);
}

int Demo_getChId(char *string, int maxChId)
{
    char inputStr[MAX_INPUT_STR_SIZE];
    int chId;

    printf(" \n");
    printf(" Select %s CH ID [0 .. %d] : ", string, maxChId-1);

    fflush(stdin);
    fgets(inputStr, MAX_INPUT_STR_SIZE, stdin);

    chId = atoi(inputStr);

    if(chId < 0 || chId >= maxChId )
    {
        chId = 0;

        printf(" \n");
        printf(" WARNING: Invalid CH ID specified, defaulting to CH ID = %d \n", chId);
    }
    else
    {
        printf(" \n");
        printf(" Selected CH ID = %d \n", chId);
    }

    printf(" \n");

    return chId;
}

int Demo_getIntValue(char *string, int minVal, int maxVal, int defaultVal)
{
    char inputStr[MAX_INPUT_STR_SIZE];
    int value;

    printf(" \n");
    printf(" Enter %s [Valid values, %d .. %d] : ", string, minVal, maxVal);

    fflush(stdin);
    fgets(inputStr, MAX_INPUT_STR_SIZE, stdin);

    value = atoi(inputStr);

    if(value < minVal || value > maxVal )
    {
        value = defaultVal;
        printf(" \n");
        printf(" WARNING: Invalid value specified, defaulting to value of = %d \n", value);
    }
    else
    {
        printf(" \n");
        printf(" Entered value = %d \n", value);
    }

    printf(" \n");

    return value;
}

Bool Demo_getFileWriteEnable()
{
    char inputStr[MAX_INPUT_STR_SIZE];
    Bool enable;

    printf(" \n");
    printf(" Enable file write (YES - y / NO - n) : ");

    inputStr[0] = 0;

    fflush(stdin);
    fgets(inputStr, MAX_INPUT_STR_SIZE, stdin);

    enable = FALSE;

    if(inputStr[0]=='y' || inputStr[0]=='Y' )
    {
        enable = TRUE;
    }

    printf(" \n");
    if(enable)
        printf(" File write ENABLED !!!\n");
    else
        printf(" File write DISABLED !!!\n");
    printf(" \n");
    return enable;
}

Bool Demo_isPathValid( const char* absolutePath )
{

    if(access( absolutePath, F_OK ) == 0 ){

        struct stat status;
        stat( absolutePath, &status );

        return (status.st_mode & S_IFDIR) != 0;
    }
    return FALSE;
}

int Demo_getFileWritePath(char *path, char *defaultPath)
{
    int status=0;

    printf(" \n");
    printf(" Enter file write path : ");

    fflush(stdin);
    fgets(path, MAX_INPUT_STR_SIZE, stdin);

    printf(" \n");

    /* remove \n from the path name */
    path[ strlen(path)-1 ] = 0;

    if(!Demo_isPathValid(path))
    {
        printf(" WARNING: Invalid path [%s], trying default path [%s] ...\n", path, defaultPath);

        strcpy(path, defaultPath);

        if(!Demo_isPathValid(path))
        {
            printf(" WARNING: Invalid default path [%s], file write will FAIL !!! \n", path);

            status = -1;
        }
    }

    if(status==0)
    {
        printf(" Selected file write path [%s] \n", path);
    }

    printf(" \n");

    return 0;


}

