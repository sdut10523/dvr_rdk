
#include <demo.h>

char gDemo_decodeSettingsMenu[] = {
    "\r\n ===================="
    "\r\n Decode Settings Menu"
    "\r\n ===================="
    "\r\n"
    "\r\n 1: Disable channel"
    "\r\n 2: Enable  channel"
    "\r\n 3: Set TrickPlay Speed"
    "\r\n"
    "\r\n p: Previous Menu"
    "\r\n"
    "\r\n Enter Choice: "
};

int Demo_decodeSettings(int demoId)
{
    Bool done = FALSE;
    char ch;
    int chId;
    int speed = 1;


    if(gDemo_info.maxVdecChannels<=0)
    {
        printf(" \n");
        printf(" WARNING: Decode NOT enabled, this menu is NOT valid !!!\n");
        return -1;
    }

    while(!done)
    {
        printf(gDemo_decodeSettingsMenu);

        ch = Demo_getChar();

        switch(ch)
        {
            case '1':
                chId = Demo_getChId("DECODE", gDemo_info.maxVdecChannels);

                Vdec_disableChn(chId);

                /* disable playback channel on display as well */
                /* playback channel ID are after capture channel IDs */
                chId += gDemo_info.maxVcapChannels;

                Demo_displayChnEnable(chId, FALSE);
                break;

            case '2':
                chId = Demo_getChId("DECODE", gDemo_info.maxVdecChannels);

                /* primary stream */
                Vdec_enableChn(chId);

                /* enable playback channel on display as well */
                /* playback channel ID are after capture channel IDs */
                chId += gDemo_info.maxVcapChannels;

                OSA_waitMsecs(500);

                Demo_displayChnEnable(chId, TRUE);
                break;

            case '3':
                chId = Demo_getChId("DECODE", gDemo_info.maxVdecChannels);

                /* Reading TPlay Speed. Currently supporting 1x, 2x, 4x */
                speed = Demo_getIntValue("\n\nEnter TPlay Speed in multiple of 2",1,4,1);

                Vdis_setAvsyncConfig(chId, speed);
                Vdec_setTplayConfig(chId, speed);
                VdecVdis_setTplayConfig(chId, speed);
                break;

            case 'p':
                done = TRUE;
                break;
        }
    }

    return 0;
}

