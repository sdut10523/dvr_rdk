
SRCDIR += links_m3vpss/capture links_m3vpss/system \
		links_m3vpss/dei links_m3vpss/display \
		links_m3vpss/nsf links_m3vpss/swMs \
		links_m3vpss/sclr \
		links_m3vpss/avsync

SRCS_m3vpss += captureLink_tsk.c captureLink_drv.c \
		deiLink_tsk.c deiLink_drv.c \
		displayLink_tsk.c displayLink_drv.c \
		nsfLink_tsk.c nsfLink_drv.c \
		swMsLink_tsk.c swMsLink_drv.c swMsLink_layouts.c \
		system_m3vpss.c \
		system_dctrl_common.c \
		systemLink_tsk_m3vpss.c \
		sclrLink_tsk.c sclrLink_drv.c \
		avsync_m3vpss.c\
		system_dctrl.c
		

