
include $(dvr_rdk_PATH)/makerules/common_header_a8.mk

INCLUDE+= $(MCFW_INC) $(OSA_INC) $(SYSLINK_INC) $(CODEC_INC)

LIBS=$(LIB_DIR)/dvr_rdk_demo_link_api.a $(MCFW_LIBS) $(AUDIO_LIBS)

include $(dvr_rdk_PATH)/makerules/common_footer_a8.mk


