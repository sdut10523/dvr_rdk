
#include <demo.h>

char gDemo_encodeSettingsMenu[] = {
    "\r\n ===================="
    "\r\n Encode Settings Menu"
    "\r\n ===================="
    "\r\n"
    "\r\n 1: Disable channel"
    "\r\n 2: Enable  channel"
    "\r\n 3: Change bit-rate"
    "\r\n 4: Change frame-rate"
    "\r\n 5: Change I-frame interval (GOP ratio)"
    "\r\n 6: Force I-frame"
    "\r\n 7: Change QP I-frame/JPEG Quality Factor"
    "\r\n 8: Change QP P-frame"
    "\r\n 9: Change RateControl Algorithm(CBR/VBR)"
    "\r\n 0: Take a JPEG Snapshot"
    "\r\n a: Bitrate Switching Interval: VBRDuration"
    "\r\n b: VBR Sensitivity"
    "\r\n"
    "\r\n i: Print Encode Parameters"
    "\r\n"
    "\r\n p: Previous Menu"
    "\r\n"
    "\r\n Enter Choice: "
};

int Demo_encodeSettings(int demoId)
{
    Bool done = FALSE;
    char ch;
    int chId, value;
    VENC_CHN_DYNAMIC_PARAM_S params;
    VENC_PARAMS_S getVEncContext;

    if(gDemo_info.maxVencChannels<=0)
    {
        printf(" \n");
        printf(" WARNING: Encode NOT enabled, this menu is NOT valid !!!\n");
        return -1;
    }
    Venc_getContext(&getVEncContext);

    while(!done)
    {
        printf(gDemo_encodeSettingsMenu);

        ch = Demo_getChar();

        switch(ch)
        {
            case '1':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);

                Venc_disableChn(chId);

                /* disable playback channel on display as well, since playback is sync with encode IN this demo */
                /* playback channel ID are after capture channel IDs */
                chId += gDemo_info.maxVcapChannels;

                Demo_displayChnEnable(chId, FALSE);
                break;

            case '2':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);

                /* primary stream */
                Venc_enableChn(chId);

                /* enable playback channel on display as well, since playback is sync with encode IN this demo */
                /* playback channel ID are after capture channel IDs */
                chId += gDemo_info.maxVcapChannels;

                Demo_displayChnEnable(chId, TRUE);
                break;

            case '3':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);

                value = Demo_getIntValue("Encode Bit-rate (in Kbps)", 64, 4000, 2000);

                memset(&params, 0, sizeof(params));

                params.targetBitRate = value * 1000;

                Venc_setDynamicParam(chId, 0, &params, VENC_BITRATE);
                break;

            case '4':

                if(demoId == DEMO_VCAP_VENC_VDEC_VDIS_INTERLACED)
                {
                    printf("*****NOTE: This is not supported by this interlace demo now.\n");
                    break;
                }

                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);

                value = Demo_getIntValue("Encode frame-rate", 1, 30, 30);

                memset(&params, 0, sizeof(params));

                params.frameRate = value;

                Venc_setDynamicParam(chId, 0, &params, VENC_FRAMERATE);
                break;

            case '5':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);

                value = Demo_getIntValue("Encode I-frame interval", 1, 30, 30);

                memset(&params, 0, sizeof(params));

                params.intraFrameInterval = value;

                Venc_setDynamicParam(chId, 0, &params, VENC_IPRATIO);
                break;

            case '6':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);

                Venc_forceIDR(chId, 0);
                break;
            case '7':
                chId = Demo_getChId("ENCODE[For MJPEG Channels: Quality factor]", gDemo_info.maxVencChannels);
                memset(&params, 0, sizeof(params));
                value = Demo_getIntValue("Encode qpMinI", 0, 51, 10);
                params.qpMin = value;
                value = Demo_getIntValue("Encode qpMaxI(qpMaxI > qpMinI)", 0, 51, 36);
                params.qpMax = value;
                /*Range for H264 qpInitI is -1 to 51,Default value being 28*/
                /*Range for MJPEG is 2 to 97, Default value being 50*/
                value = Demo_getIntValue
                ("Encode qpI(qpMinI < qpI < qpMaxI)\n H264: -1 to 51\n MJPEG: 2 to 97, qpMinI/qpMaxI ignored\n ", -1, 97, 28);
                params.qpInit = value;

                Venc_setDynamicParam(chId,0,&params,VENC_QPVAL_I);
                break;
            case '8':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);
                memset(&params, 0, sizeof(params));
                value = Demo_getIntValue("Encode qpMinP", 0, 51, 10);
                params.qpMin = value;
                value = Demo_getIntValue("Encode qpMaxP(qpMaxP > qpMinP)", 0, 51, 40);
                params.qpMax = value;
                value = Demo_getIntValue("Encode qpP(qpMinP < qpP < qpMaxP)", -1, 51, 28);
                params.qpInit = value;

                Venc_setDynamicParam(chId,0,&params,VENC_QPVAL_P);
                break;
            case '9':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);
                memset(&params, 0, sizeof(params));

                value = Demo_getIntValue("Encode RcAlgo,Enter 0 For VBR / 1 For CBR", 0, 1, 0);
                params.rcAlg = value;

                Venc_setDynamicParam(chId,0,&params,VENC_RCALG);
                break;
            case '0':
                printf("\nEncoder Channels 0-%d\n",(gDemo_info.maxVencChannels-1));
                printf("\nSelect MJPEG Encoder Channels %d-%d\n", \
                    (gDemo_info.maxVencChannels - getVEncContext.numPrimaryChn), (gDemo_info.maxVencChannels-1));

                chId = Demo_getIntValue("MJPEG ENCODE Channel", \
                   (2*getVEncContext.numPrimaryChn),(gDemo_info.maxVencChannels-1),(2*getVEncContext.numPrimaryChn));

                Venc_snapshotDump(chId, 0);
                break;
            case 'a':
                chId = Demo_getChId("ENCODE",gDemo_info.maxVencChannels);
                memset(&params, 0, sizeof(params));

                value = Demo_getIntValue("VBRDuration(CVBR)",0,3600,8);
                params.vbrDuration = value;

                Venc_setDynamicParam(chId,0,&params,VENC_VBRDURATION);
                break;
            case 'b':
                chId = Demo_getChId("ENCODE",gDemo_info.maxVencChannels);
                memset(&params, 0, sizeof(params));

                value = Demo_getIntValue("VBRSensitivity(CVBR) ",0,8,0);
                params.vbrSensitivity = value;

                Venc_setDynamicParam(chId,0,&params,VENC_VBRSENSITIVITY);
                break;
            case 'i':
                chId = Demo_getChId("ENCODE", gDemo_info.maxVencChannels);

                Venc_getDynamicParam(chId, 0, &params, VENC_ALL);

                printf(" VENC CH%d: Bit-rate         = %d Kbps", chId, params.targetBitRate/1000 );
                printf(" VENC CH%d: Frame-rate       = %d fps" , chId, params.frameRate);
                printf(" VENC CH%d: I-frame interval = %d"     , chId, params.intraFrameInterval);

                break;

            case 'p':
                done = TRUE;
                break;
        }
    }

    return 0;
}

