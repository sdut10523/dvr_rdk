
#include <demo.h>

char gDemo_audioSettingsMenu[] = {
    "\r\n ==================="
    "\r\n Audio Settings Menu"
    "\r\n ==================="
    "\r\n"
    "\r\n 1: Set   Audio Storage Path"
    "\r\n 2: Start Audio Capture"
    "\r\n 3: Stop  Audio Capture"
    "\r\n 4: Start Audio Playback"
    "\r\n 5: Stop  Audio Playback"
    "\r\n 6: Set Audio Capture Configuration"
    "\r\n"
    "\r\n p: Previous Menu"
    "\r\n"
    "\r\n Enter Choice: "
};


char gDemo_audioParametersSettingsMenu[] = {
    "\r\n ==========================="
    "\r\n Audio Parameters Settings Menu"
    "\r\n ==========================="
    "\r\n"
    "\r\n 1: Set Sampling Frequency"
    "\r\n 2: Set Audio Volume"
    "\r\n 3: Mute Audio Volume"
    "\r\n"
    "\r\n p: Previous Menu"
    "\r\n"
    "\r\n Enter Choice: "
};

int Demo_audioCaptureStop()
{
    if(!gDemo_info.audioEnable)
        return -1;

    printf(" \n");

    if(gDemo_info.isAudioPathSet)
    {
        if(gDemo_info.audioCaptureActive)
        {
            Audio_captureStop();
            printf(" Audio Capture is STOPPED !!! \n");

            gDemo_info.audioCaptureActive = FALSE;
        }
        else
        {
            printf(" Audio Capture is ALREADY STOPPED !!! \n");
        }
    }
    else
    {
        printf(" Audio Capture storage path NOT set !!! \n");
    }

    printf(" \n");

    return 0;
}

int Demo_audioPlaybackStop()
{
    if(!gDemo_info.audioEnable)
        return -1;

    printf(" \n");

    if(gDemo_info.isAudioPathSet)
    {
        if(gDemo_info.audioPlaybackActive)
        {
            Audio_playStop();
            printf(" Audio Playback is STOPPED !!! \n");

            gDemo_info.audioPlaybackActive = FALSE;
        }
        else
        {
            printf(" Audio Playback is ALREADY STOPPED !!! \n");
        }
    }
    else
    {
        printf(" Audio Playback storage path NOT set !!! \n");
    }

    printf(" \n");

    return 0;
}

int Demo_audioCaptureStart(UInt32 chId)
{
    if(!gDemo_info.audioEnable)
        return -1;

    printf(" \n");

    if(gDemo_info.isAudioPathSet)
    {
        gDemo_info.audioCaptureChNum = chId;

        if (gDemo_info.audioCaptureActive)
        {
            Demo_audioCaptureStop();
        }

        if(gDemo_info.audioCaptureChNum == gDemo_info.audioPlaybackChNum && gDemo_info.audioPlaybackActive)
        {
            printf (" Audio playback active on same channel. Stopping Playback ... \n");
            Demo_audioPlaybackStop();
        }
        Audio_captureStart(gDemo_info.audioCaptureChNum);

        gDemo_info.audioCaptureActive = TRUE;
    }
    else
    {
        printf(" Audio Capture storage path NOT set !!! \n");
    }

    printf(" \n");

    return 0;
}

int Demo_audioPlaybackStart(UInt32 chId, UInt32 playbackDevId)
{
    if(!gDemo_info.audioEnable)
        return -1;

    printf(" \n");

    if(gDemo_info.isAudioPathSet)
    {
        gDemo_info.audioPlaybackChNum = chId;

        if (gDemo_info.audioPlaybackActive)
        {
            Demo_audioPlaybackStop();
        }

        if(gDemo_info.audioCaptureChNum == gDemo_info.audioPlaybackChNum && gDemo_info.audioCaptureActive)
        {
            printf (" Audio capture active on same channel. Stopping Capture ... \n");
            Demo_audioCaptureStop();
        }

        Audio_playStart(gDemo_info.audioPlaybackChNum,playbackDevId);
        gDemo_info.audioPlaybackActive = TRUE;
    }
    else
    {
        printf(" Audio Playback storage path NOT set !!! \n");
    }

    printf(" \n");


    return 0;
}

int Demo_audioSettings(int demoId)
{
    Bool done = FALSE;
    char ch;
    char audioPath[256];
    int chId;
    int playbackDevId;

    if(gDemo_info.maxVcapChannels<=0 || !gDemo_info.audioEnable)
    {
        printf(" \n");
        printf(" WARNING: Audio NOT enabled, this menu is NOT valid !!!\n");
        return -1;
    }

    while(!done)
    {
        printf(gDemo_audioSettingsMenu);

        ch = Demo_getChar();

        switch(ch)
        {
            case '1':
                Demo_getFileWritePath(audioPath, "/dev/shm");

                if (Audio_setStoragePath(audioPath) == AUDIO_STATUS_OK)
                {
                    gDemo_info.isAudioPathSet = TRUE;
                }
                else
                {
                    gDemo_info.isAudioPathSet = FALSE;
                }

                break;
            case '2':
                chId = Demo_getChId("AUDIO CAPTURE", gDemo_info.maxVcapChannels);

                Demo_audioCaptureStart(chId);

                break;
            case '3':
                Demo_audioCaptureStop();
                break;
            case '4':
                chId = Demo_getChId("AUDIO PLAYBACK", gDemo_info.maxVcapChannels);
                playbackDevId = Demo_getIntValue("Playback Device 0-AIC3x 1-HDMI Out", 0, 1, 0);
                
                Demo_audioPlaybackStart(chId,playbackDevId);
                break;
            case '5':
                Demo_audioPlaybackStop();
                break;
            case '6':
                Demo_audioCaptureSetParams(TRUE);
                break;
            case 'p':
                done = TRUE;
                break;
        }
    }

    return 0;
}

int Demo_audioEnable(Bool enable)
{
    if(!gDemo_info.audioEnable)
        return -1;

    if(enable)
    {
        Audio_captureCreate();
        Audio_playCreate();
        Demo_audioCaptureSetParams(FALSE);
        gDemo_info.isAudioPathSet = FALSE;
        gDemo_info.audioCaptureActive = FALSE;
        gDemo_info.audioPlaybackActive = FALSE;
        gDemo_info.audioPlaybackChNum = 0;
        gDemo_info.audioCaptureChNum = 0;
    }
    else
    {
        Demo_audioCaptureStop();
        Demo_audioPlaybackStop();

        Audio_captureDelete();
        Audio_playDelete();
    }

    return 0;
}


int Demo_audioCaptureSetParams(Bool set_params)
{
    char ch;
    Bool done = FALSE;
    Uint32 samplingHz;
    Int32 audioVolume;
    if(!gDemo_info.audioEnable)
        return -1;
    if (FALSE == set_params)
    {
        Audio_playSetSamplingFreq(AUDIO_SAMPLE_RATE_DEFAULT,AUDIO_VOLUME_DEFAULT);
        return 0;
    }
    
    Audio_playGetSamplingFreq(&samplingHz,&audioVolume);

    while(!done)
    {
        printf(gDemo_audioParametersSettingsMenu);

        ch = Demo_getChar();

        switch(ch)
        {
            case '1':

                if(Audio_captureIsStart() == 0)
                {
                    printf("\r\n Enter the Frequency [8000 or 16000  Khz]:");
                    scanf("%d",&samplingHz);
                }
                else
                {
                    printf("\r\n Sampling Frquecny can not be changed when Audio Capture is Running ");
                    printf("\r\n Stop Audio Capture");
                }

                break;
            case '2':

                printf("\r\n Enter the Audio Volume[0..8] :");
                scanf("%d",&audioVolume);

                break;
            case '3':

                audioVolume = 0;
                printf("\r\n Audio capture is muted. Increase audio capture volume to unmute");
               
                break;
            case 'p':
                done = TRUE;
                break;
        }
    }

    if ((samplingHz == 8000 || samplingHz == 16000)&&((audioVolume >= 0 || audioVolume <= 8)))
    {
        Vcap_setAudioModeParam(samplingHz,audioVolume);
    }
    
    


    return 0;
}

