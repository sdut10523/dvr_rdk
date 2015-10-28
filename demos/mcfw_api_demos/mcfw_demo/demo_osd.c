
#include <demo.h>
#include <ti_swosd_logo_160x32_yuv420sp.h>
#include <ti_swosd_logo_160x32_yuv422i.h>

#define OSD_BUF_HEAP_SR_ID          (0)

static UInt8   *osdBufBaseVirtAddr = NULL;
static UInt32   osdTotalBufSize = 0;
AlgLink_OsdChWinParams g_osdChParam[ALG_LINK_OSD_MAX_CH];

void ReadSeedlog(char* buf, int len)
{
	static FILE* fp = NULL;
	if(fp == NULL)
	{
	    fp = fopen("seed.yuv", "rb");
	    if(fp==NULL)
		printf("Can't open log\n");
	}

	fread(buf, 1, len, fp);
}
void yuyv422_to_yuv420sp(unsigned char *bufsrc, unsigned char *bufdest, int width, int height)
{
	unsigned char *ptrsrcy1, *ptrsrcy2;
	unsigned char *ptrsrcy3, *ptrsrcy4;
	unsigned char *ptrsrccb1, *ptrsrccb2;
	unsigned char *ptrsrccb3, *ptrsrccb4;
	unsigned char *ptrsrccr1, *ptrsrccr2;
	unsigned char *ptrsrccr3, *ptrsrccr4;
	int srcystride, srcccstride;

	unsigned char *ptrdesty1, *ptrdesty2;
	unsigned char *ptrdesty3, *ptrdesty4;
	unsigned char *ptrdestcb1, *ptrdestcb2;
	unsigned char *ptrdestcr1, *ptrdestcr2;
	int destystride, destccstride;

	int i, j;

	ptrsrcy1  = bufsrc ;
	ptrsrcy2  = bufsrc + (width<<1) ;
	ptrsrcy3  = bufsrc + (width<<1)*2 ;
	ptrsrcy4  = bufsrc + (width<<1)*3 ;

	ptrsrccb1 = bufsrc + 1;
	ptrsrccb2 = bufsrc + (width<<1) + 1;
	ptrsrccb3 = bufsrc + (width<<1)*2 + 1;
	ptrsrccb4 = bufsrc + (width<<1)*3 + 1;

	ptrsrccr1 = bufsrc + 3;
	ptrsrccr2 = bufsrc + (width<<1) + 3;
	ptrsrccr3 = bufsrc + (width<<1)*2 + 3;
	ptrsrccr4 = bufsrc + (width<<1)*3 + 3;

	srcystride  = (width<<1)*3;
	srcccstride = (width<<1)*3;



	ptrdesty1 = bufdest;
	ptrdesty2 = bufdest + width;
	ptrdesty3 = bufdest + width*2;
	ptrdesty4 = bufdest + width*3;

	ptrdestcb1 = bufdest + width*height;
	ptrdestcb2 = bufdest + width*height + width;

	ptrdestcr1 = bufdest + width*height + 1;
	ptrdestcr2 = bufdest + width*height + width + 1;

	destystride  = (width)*3;
	destccstride = width;


	for(j=0; j<(height/4); j++)
	{
		for(i=0;i<(width/2);i++)
		{
			(*ptrdesty1++) = (*ptrsrcy1);
			(*ptrdesty2++) = (*ptrsrcy2);
			(*ptrdesty3++) = (*ptrsrcy3);
			(*ptrdesty4++) = (*ptrsrcy4);

			ptrsrcy1 += 2;
			ptrsrcy2 += 2;
			ptrsrcy3 += 2;
			ptrsrcy4 += 2;

			(*ptrdesty1++) = (*ptrsrcy1);
			(*ptrdesty2++) = (*ptrsrcy2);
			(*ptrdesty3++) = (*ptrsrcy3);
			(*ptrdesty4++) = (*ptrsrcy4);

			ptrsrcy1 += 2;
			ptrsrcy2 += 2;
			ptrsrcy3 += 2;
			ptrsrcy4 += 2;

			(*ptrdestcb1) = (*ptrsrccb1);
			(*ptrdestcb2) = (*ptrsrccb3);
			ptrdestcb1 += 2;
			ptrdestcb2 += 2;

			ptrsrccb1 += 4;
			ptrsrccb3 += 4;

			(*ptrdestcr1) = (*ptrsrccr1);
			(*ptrdestcr2) = (*ptrsrccr3);
			ptrdestcr1 += 2;
			ptrdestcr2 += 2;

			ptrsrccr1 += 4;
			ptrsrccr3 += 4;

		}

		/* Update src pointers */
		ptrsrcy1  += srcystride;
		ptrsrcy2  += srcystride;
		ptrsrcy3  += srcystride;
		ptrsrcy4  += srcystride;

		ptrsrccb1 += srcccstride;
		ptrsrccb3 += srcccstride;

		ptrsrccr1 += srcccstride;
		ptrsrccr3 += srcccstride;

		/* Update dest pointers */
		ptrdesty1 += destystride;
		ptrdesty2 += destystride;
		ptrdesty3 += destystride;
		ptrdesty4 += destystride;

		ptrdestcb1 += destccstride;
		ptrdestcb2 += destccstride;

		ptrdestcr1 += destccstride;
		ptrdestcr2 += destccstride;

	}
}

static char Seedlogo[DEMO_OSD_WIN_WIDTH*DEMO_OSD_WIN_HEIGHT*2];
Int32 Demo_osdInit(UInt32 numCh, UInt8 *osdFormat)
{
    int chId, winId, status;

    Vsys_AllocBufInfo bufInfo;
    UInt32 osdBufSize, osdBufSizeY, bufAlign;

    UInt32 bufOffset;
    UInt8 *curVirtAddr;

    assert(numCh <= ALG_LINK_OSD_MAX_CH);

    osdBufSizeY = DEMO_OSD_WIN_PITCH*DEMO_OSD_WIN_HEIGHT;

    osdBufSize = osdBufSizeY * 2 ;

    osdTotalBufSize = osdBufSize * numCh * DEMO_OSD_NUM_WINDOWS;
    bufAlign = 128;

    status = Vsys_allocBuf(OSD_BUF_HEAP_SR_ID, osdTotalBufSize, bufAlign, &bufInfo);
    OSA_assert(status==OSA_SOK);

    osdBufBaseVirtAddr = bufInfo.virtAddr;

    for(chId = 0; chId < numCh; chId++)
    {
        AlgLink_OsdChWinParams * chWinPrm = &g_osdChParam[chId];

        chWinPrm->chId = chId;
        chWinPrm->numWindows = DEMO_OSD_NUM_WINDOWS;

        for(winId=0; winId < chWinPrm->numWindows; winId++)
        {
            chWinPrm->winPrm[winId].startX             = DEMO_OSD_WIN0_STARTX ;
            chWinPrm->winPrm[winId].startY             = DEMO_OSD_WIN0_STARTY + (DEMO_OSD_WIN_HEIGHT+DEMO_OSD_WIN0_STARTY)*winId;
            chWinPrm->winPrm[winId].width              = DEMO_OSD_WIN_WIDTH;
            chWinPrm->winPrm[winId].height             = DEMO_OSD_WIN_HEIGHT;
            chWinPrm->winPrm[winId].lineOffset         = DEMO_OSD_WIN_PITCH;
            chWinPrm->winPrm[winId].globalAlpha        = DEMO_OSD_GLOBAL_ALPHA/(winId+1);
            chWinPrm->winPrm[winId].transperencyEnable = DEMO_OSD_TRANSPARENCY;
            chWinPrm->winPrm[winId].enableWin          = DEMO_OSD_ENABLE_WIN;

            bufOffset = osdBufSize * winId;

            chWinPrm->winPrm[winId].addr[0][0] = (bufInfo.physAddr + bufOffset);

            curVirtAddr = bufInfo.virtAddr + bufOffset;

            /* copy logo to buffer  */
            if(osdFormat[chId] == SYSTEM_DF_YUV422I_YUYV)
            {
                chWinPrm->winPrm[winId].format     = SYSTEM_DF_YUV422I_YUYV;
                chWinPrm->winPrm[winId].addr[0][1] = NULL;
		ReadSeedlog(Seedlogo,DEMO_OSD_WIN_WIDTH*DEMO_OSD_WIN_HEIGHT*2);
                //OSA_assert(sizeof(gMCFW_swosdTiLogoYuv422i_160x32)<=osdBufSize);
                //memcpy(curVirtAddr, gMCFW_swosdTiLogoYuv422i_160x32, sizeof(gMCFW_swosdTiLogoYuv422i_160x32));
		memcpy(curVirtAddr, Seedlogo, DEMO_OSD_WIN_WIDTH*DEMO_OSD_WIN_HEIGHT*2);
            }
            else
            {
                chWinPrm->winPrm[winId].format     = SYSTEM_DF_YUV420SP_UV;
                chWinPrm->winPrm[winId].addr[0][1] =  chWinPrm->winPrm[winId].addr[0][0] + osdBufSizeY;
               // OSA_assert(sizeof(gMCFW_swosdTiLogoYuv420sp_160x32)<= osdBufSize);  
		ReadSeedlog(Seedlogo,DEMO_OSD_WIN_WIDTH*DEMO_OSD_WIN_HEIGHT*2);    
          	//Yuyv2Yuv420sp(Seedlogo,Seedlogo420,DEMO_OSD_WIN_WIDTH,DEMO_OSD_WIN_HEIGHT);
		yuyv422_to_yuv420sp(Seedlogo,curVirtAddr,DEMO_OSD_WIN_WIDTH,DEMO_OSD_WIN_HEIGHT);
		//memcpy(curVirtAddr, Seedlogo420, DEMO_OSD_WIN_WIDTH*DEMO_OSD_WIN_HEIGHT*3/2);
                //memcpy(curVirtAddr, gMCFW_swosdTiLogoYuv420sp_160x32, sizeof(gMCFW_swosdTiLogoYuv420sp_160x32));
            }
        }
    }
    return status;
}

Void Demo_osdDeinit()
{
	if(osdBufBaseVirtAddr != NULL)
	{
		Vsys_freeBuf(OSD_BUF_HEAP_SR_ID, osdBufBaseVirtAddr, osdTotalBufSize);
	}
}
