
demo:
	$(MAKE) -fMAKEFILE.MK libs

libs:
ifeq ($(PLATFORM),ti816x-evm)
#	$(MAKE) -fMAKEFILE.MK -C$(dvr_rdk_PATH)/demos/link_api_demos $(TARGET)
	$(MAKE) -fMAKEFILE.MK -C$(dvr_rdk_PATH)/demos/mcfw_api_demos $(TARGET)
else
	$(MAKE) -fMAKEFILE.MK -C$(dvr_rdk_PATH)/demos/mcfw_api_demos $(TARGET)
endif

clean:
	$(MAKE) -fMAKEFILE.MK libs TARGET=clean

all: clean demo

