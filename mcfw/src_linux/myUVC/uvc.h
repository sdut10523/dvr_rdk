#ifndef _MYINIT_H_
#define _MYINIT_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

///#include <X11/Xlib.h>

#include "avilib.h"
#include "v4l2uvc.h"
typedef struct {
	    FILE *file;
		char *filename;
		char *avifilename;
		int width ;
		int height;
		int fps;
		int format;
		int grabmethod;
}VDParams;
int myArg(VDParams* vdParams);
int myInit(vdIn *vd,VDParams* vdParams);
int myUpdate(unsigned char* dstVirtAddr,vdIn *vd);
int myDestroy(FILE *file,vdIn *vd);
int myPack(unsigned char * dstVirtAddr,VDParams* vdParams,vdIn* vd);
#endif
