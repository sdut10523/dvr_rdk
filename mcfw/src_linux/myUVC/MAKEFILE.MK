##CC=arm-none-linux-gnueabi-gcc
#myInit.o: myInit.c avilib.h v4l2uvc.h myInit.h
#avilib.o:  avilib.h  
#v4l2uvc.o: v4l2uvc.h  
#clean :
#	rm *.o

include $(dvr_rdk_PATH)/makerules/common_header_a8.mk

INCLUDE+=-I $(dvr_rdk_PATH)/mcfw/src_linux/myUVC

include $(dvr_rdk_PATH)/makerules/common_footer_a8.mk