# modify these
PLATFORM = mt6893
XT_TOOLS_VERSION = RI-2018.0-linux
ALPS_PATH = ../../../../../../../../../../

# compile parameter
XT_TOOLS_BIN_DIR = $(ALPS_PATH)/prebuilts/xcc/linux-x86/xtensa/$(XT_TOOLS_VERSION)/XtensaTools/bin
INSTALLDIR = $(ALPS_PATH)/vendor/mediatek/proprietary/tinysys/adsp/HIFI3/middleware/lib/$(XT_TOOLS_VERSION)/libdemocpp
INCLUDES = -I$(ALPS_PATH)/vendor/mediatek/proprietary/tinysys/common/drivers/audio/aurisys/interface
ADSP_CROSS_COMPILE := $(XT_TOOLS_BIN_DIR)/
CC            := $(ADSP_CROSS_COMPILE)xt-xc++
AR            := $(ADSP_CROSS_COMPILE)xt-ar
CFLAGS = -O3 -Wall -mcoproc -Werror -mlongcalls -LNO:simd -ffunction-sections -Wno-error=format -fdata-sections --xtensa-core=$(PLATFORM) -std=c++11
CLEANEXTS = o a
SRC_FILES = $(wildcard *.cpp)

OBJ_FILES = $(patsubst %.cpp,%.o,$(SRC_FILES))
OUT_FILES = libdemocpp.a

all: $(OUT_FILES)
	cp -p $(OUT_FILES) $(INSTALLDIR)

libdemocpp.a : $(OBJ_FILES)
	$(AR) -src $@ $^

%.o : %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean
clean:
	for file in $(CLEANEXTS); do rm -f *.$$file; done
