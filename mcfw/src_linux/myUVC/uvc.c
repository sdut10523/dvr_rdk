#include "uvc.h"
int myArg(VDParams* vdParams){
	int static count=0;
	vdParams->width = 640;
	vdParams->height = 480;
	vdParams->fps   = 30 ;
	switch( count){
	case 0:
		vdParams->filename = "/dev/video0";
		count++;
		break;
	case 1:
		vdParams->filename = "/dev/video1";
		count=0;
		break;
	default:
		vdParams->filename = "/dev/video0";
		count++;
		break;
	}
	vdParams->avifilename = "test.avi";
	vdParams->format = V4L2_PIX_FMT_MJPEG;
	vdParams->grabmethod = 1;
	vdParams->file = fopen(vdParams->filename, "wb");
	if(vdParams->file == NULL)
	{
			printf("Unable to open file for raw frame capturing\n ");
			exit(1);
	}
	else{
		printf("Succeed to open %s\n",vdParams->filename);
	}
}
int myInit(vdIn *vd,VDParams* vdParams)
{

	//v4l2 init
	///vdIn* vd = (struct vdIn *) calloc(1, sizeof(struct vdIn));
	//gVdecVdis_obj.vd= (struct vdIn *) calloc(1, sizeof(struct vdIn));
	//vdIn *vd=gVdecVdis_obj.vd;
	if(init_videoIn(vd, (char *) vdParams->filename, vdParams->width, vdParams->height,vdParams->fps,vdParams->format,vdParams->grabmethod,vdParams->avifilename) < 0)
	{
		exit(1);
	}
	
	if (video_enable(vd))
	{
	   exit(1);
	}
	

	return 0;
}
int myUpdate(unsigned char* dstVirtAddr,vdIn *vd){

	    int i;
	    //int numBufs=1;
	    int ret;
	   // for (i = 0; i < numBufs; i++)
	    {
	    	memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
	    	vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    	vd->buf.memory = V4L2_MEMORY_MMAP;
	        ///////deque
	        	ret = ioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
	        			if (ret < 0)
	        			{
	        				printf("Unable to dequeue buffer\n");
	        				exit(1);
	        			}
	        /////copydata
	        	memcpy( dstVirtAddr, vd->mem[vd->buf.index],vd->buf.bytesused);
	        			//memcpy( dstVirtAddr, vd->mem[vd->buf.index],vd->width*vd->height);
	        			//printf("**********************count:%d\n",vd->framecount++);
	        			//printf("************w*h:%d####bytesused:%d\n",vd->width*vd->height,vd->buf.bytesused);
	        ////////reque
	        	ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
	        					if (ret < 0)
	        					{
	        						printf("Unable to requeue buffer\n");
	        						exit(1);
	        					}

	    }
	    return 0;
}

int myDestroy(FILE *file,vdIn *vd){
	fclose(file);
	close_v4l2(vd);
	return 0;
}
int myPack(unsigned char * dstVirtAddr,VDParams* vdParams,vdIn* vd){
	int bytesused;
	//memset(*vd, 0, sizeof(vdIn));
	//memset(*vdParams, 0, sizeof(VDParams));
	myArg(vdParams);
	fprintf(stderr, " goto here  (%s|%s|%d):\n ", __FILE__, __func__, __LINE__);
	myInit(vd,vdParams);
	fprintf(stderr, " goto here  (%s|%s|%d):\n ", __FILE__, __func__, __LINE__);
	myUpdate(dstVirtAddr,vd);
	fprintf(stderr, " goto here  (%s|%s|%d):\n ", __FILE__, __func__, __LINE__);
	bytesused=vd->buf.bytesused;
	myDestroy(vdParams->file,vd);
	fprintf(stderr, " goto here  (%s|%s|%d):\n ", __FILE__, __func__, __LINE__);
	return bytesused;
}
