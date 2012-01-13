
#
# Elecard STB820 Demo Application
# Copyright (C) 2007  Elecard Devices
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 1, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA
#

ifneq ($(ARCH),i386)
PROGRAM = StbMainApp
else
PROGRAM = StbMainApp_x86
endif

TEST_MODE ?= 0
ENABLE_VERIMATRIX ?= 0
ENABLE_SECUREMEDIA ?= 0
ifeq ($(_TMBSL),_stb225)
ENABLE_LAN := 0
ENABLE_XWORKS := 0
ENABLE_DLNA := 0
else
ENABLE_LAN ?= 1
ENABLE_XWORKS ?= 1
ENABLE_DLNA ?= 1
endif
ENABLE_VIDIMAX ?= 0
ENABLE_PPP ?= 0
ENABLE_WIFI ?= 0
PROVIDER_PROFILES ?= 0
ENABLE_PASSWORD ?= 1

TEST_SERVER ?= 1

BUILD_FOR_USERFS ?= 1
INCLUDE_CCRTP ?= 0
FORCE_HELPER_SURFACE ?= 0
INTERFACE_SPLASH_IMAGE ?= "splash.jpg"

REVISION := $(shell git svn info . | grep Revision | sed 's/.*: *//')
ifeq ($(REVISION),)
REVISION := $(shell svnversion .)
endif

COMPILE_TIME := $(shell date)
RELEASE := $(shell set | grep -m 1 'SP[0-9]' | sed 's/.*SP\([0-9]*\).*/\1/')
ifeq ($(RELEASE),)
RELEASE = 225
endif

RELEASE_TYPE ?= Custom

include $(PRJROOT)/etc/application_first.mk

LOCAL_PREFIXES = \
        /usr

LOCAL_CFLAGS = \
		-DINTERFACE_SPLASH_IMAGE="\"$(INTERFACE_SPLASH_IMAGE)\"" \
		-DREVISION="\"$(REVISION)\"" \
		-DRELEASE_TYPE="\"$(RELEASE_TYPE)\"" \
		-DCOMPILE_TIME="\"$(COMPILE_TIME)\"" \
		-DRELEASE=$(RELEASE) \
		-Wall -Wextra -Winline -Wno-strict-prototypes -Wno-strict-aliasing -Wno-unused-parameter \
		-INETLib/include \
		-ICOMMONLib/include \
		-IStbPvr/include \
		-I$(STAGINGDIR)/usr/include \
		-I$(STAGINGDIR)/usr/include/PCSC \
		-I$(STAGINGDIR)/usr/include/verimatrix \
		-ISM/includes \
		-D_FILE_OFFSET_BITS=64 \
		-finline-functions

LOCAL_CXXFLAGS = \
	-O3 -w \
	-Itinyxml

#	-I$(STAGINGDIR)/opt/philips/include \


LOCAL_LDFLAGS = \
	-ldirectfb \
	-ldirect \
	-lfusion \
	-lasound \
	-lstdc++ \
	-lnet \
	-lcommon \
	-luuid \
	-Xlinker -rpath -Xlinker /opt/elecard/lib \
	-L$(STAGINGDIR)/usr/lib \
	-L$(STAGINGDIR)/usr/local/lib \
	-LNETLib/$(ARCH) \
	-LCOMMONLib/$(ARCH) \
	-lrt -lpthread -Xlinker -z -Xlinker muldefs \
	--strip-all \

ifeq ($(INCLUDE_CCRTP),1)
LOCAL_LDFLAGS += \
	-lccrtp1 -lccgnu2 -lccext2 -lcrypto -lssl

LOCAL_CFLAGS += \
	-DINCLUDE_CCRTP

endif

ifneq ($(BUILD_LANG),)
    LOCAL_CFLAGS += -DLANG=$(BUILD_LANG)
endif

ifeq ($(TEST_MODE),1)
    LOCAL_CFLAGS += -DENABLE_TEST_MODE=$(TEST_MODE)
endif

ifeq ($(TEST_SERVER),1)
    LOCAL_CFLAGS += -DENABLE_TEST_SERVER=$(TEST_SERVER)
endif

ifeq ($(ENABLE_DLNA),1)
    LOCAL_LDFLAGS += -LDLNALib/$(ARCH) -ledlna
    LOCAL_CFLAGS += -IDLNALib \
		-IDLNALib/CdsObjects \
		-IDLNALib/HttpFiles \
		-IDLNALib/MediaServerBrowser \
		-IDLNALib/FileAbstractionFiles \
		-IDLNALib/ProtocolInfoParser \
		-IDLNALib/StringUtils \
		-IDLNALib/PlaylistTrackManager \
		-IDLNALib/MediaRenderer \
		-IDLNALib/PlaySingleUri \
		-DENABLE_DLNA=$(ENABLE_DLNA)
    DLNA_LIB_INSTALL = $(Q)install -m0755 DLNALib/$(ARCH)/libedlna.so $(ROOTFS)/usr/local/lib
    DLNA_LIB_DEPENDS = DLNALib/$(ARCH)/libedlna.so
endif

ifeq ($(ENABLE_VERIMATRIX),1)
    LOCAL_LDFLAGS += -lvmclient -lclientid -lvmerror
    LOCAL_CFLAGS += -DENABLE_VERIMATRIX=$(ENABLE_VERIMATRIX)
endif

ifeq ($(ENABLE_SECUREMEDIA),1)
    LOCAL_LDFLAGS += '-Wl,--start-group' SM/libs/ctoolkit.a SM/libs/smclient.a SM/libs/smelecard.a SM/libs/smes.a SM/libs/smlinux.a SM/libs/smplatform.a -lpthread -lm '-Wl,--end-group'
    LOCAL_CFLAGS += -DENABLE_SECUREMEDIA=$(ENABLE_SECUREMEDIA)
endif

ifeq ($(ENABLE_LAN),1)
	LOCAL_CFLAGS += -DENABLE_LAN=$(ENABLE_LAN)
endif

ifeq ($(ENABLE_PPP),1)
	LOCAL_CFLAGS += -DENABLE_PPP=$(ENABLE_PPP)
endif

ifeq ($(ENABLE_WIFI),1)
	LOCAL_CFLAGS += -DENABLE_WIFI=$(ENABLE_WIFI)
endif

ifeq ($(ENABLE_VIDIMAX),1)
	LOCAL_CFLAGS += -DENABLE_VIDIMAX=$(ENABLE_VIDIMAX)
endif

ifeq ($(ENABLE_XWORKS),1)
	LOCAL_CFLAGS += -DENABLE_XWORKS=$(ENABLE_XWORKS)
endif

ifeq ($(PROVIDER_PROFILES),1)
	LOCAL_CFLAGS += -DENABLE_PROVIDER_PROFILES=$(PROVIDER_PROFILES)
endif

ifeq ($(ENABLE_PASSWORD),1)
	LOCAL_CFLAGS += -DENABLE_PASSWORD=$(ENABLE_PASSWORD)
endif

ifneq ($(DEFAULT_FONT),)
	LOCAL_CFLAGS += -DDEFAULT_FONT=\"$(DEFAULT_FONT)\"
endif

ifneq ($(INTERFACE_WALLPAPER_IMAGE),)
	LOCAL_CFLAGS += -DINTERFACE_WALLPAPER_IMAGE=\"$(INTERFACE_WALLPAPER_IMAGE)\"
endif

ifeq ($(BUILD_FOR_USERFS),x)

$(error Build for userfs is DEPRECATED!)

INSTALL_DIR = $(ROOTFS).user/bin
DATA_DIR = $(ROOTFS).user/share/elecard/StbMainApp
CONFIG_DIR = $(ROOTFS)/config.defaults/StbMainApp

LOCAL_CFLAGS += \
	-DIMAGE_DIR="\"/usr/local/share/elecard/StbMainApp/images/\"" \
	-DFONT_DIR="\"/usr/local/share/elecard/StbMainApp/fonts/\"" \
	-DRTSP_STREAM_FILE="\"/config/StbMainApp/streams.txt\"" \
	-DCHANNEL_FILE_NAME="\"/config/StbMainApp/channels.conf\"" \
	-DSETTINGS_FILE="\"/config/StbMainApp/settings.conf\"" \
	-DPLAYLIST_FILENAME="\"/config/StbMainApp/playlist.txt\"" \

else

INSTALL_DIR = $(ROOTFS)/opt/elecard/bin
DATA_DIR = $(ROOTFS)/opt/elecard/share/StbMainApp
CONFIG_DIR = $(ROOTFS)/config.defaults/StbMainApp

LOCAL_CFLAGS += \
	-DIMAGE_DIR="\"/opt/elecard/share/StbMainApp/images/\"" \
	-DFONT_DIR="\"/opt/elecard/share/StbMainApp/fonts/\"" \
	-DRTSP_STREAM_FILE="\"/config/StbMainApp/streams.txt\"" \
	-DCHANNEL_FILE_NAME="\"/config/StbMainApp/channels.conf\"" \
	-DSETTINGS_FILE="\"/config/StbMainApp/settings.conf\"" \
	-DPLAYLIST_FILENAME="\"/config/StbMainApp/playlist.txt\"" \

endif

#	-lliveMedia \
#	-lgroupsock \
#	-lUsageEnvironment \
#	-lBasicUsageEnvironment \
#
#	-LNWSourcePlus/lib -lnwsplus -lrt -lpthread -Xlinker -z -Xlinker muldefs LinTools/src/Linux/*.o \
#	-lccrtp1 -lccgnu2 -lccext2 -lcrypto -lssl

ifneq ($(ARCH),i386)
LOCAL_CFLAGS += \
	-I$(STAGINGDIR)/usr/include/ \
	-I$(STAGINGDIR)/usr/include \
	-I$(STAGINGDIR)/usr/include/directfb \
	-I$(STAGINGDIR)/usr/local/include/directfb \
	-I$(STAGINGDIR)/usr/include/directfb-internal \
	-I$(_phStbAnalogBackend_DIR)/inc \
	-I$(_phStbGpio_DIR)/inc \
	-I$(_phStbIAmAlive_DIR)/inc \
	-I$(_phStbDFBVideoProviderCommon_DIR)/inc \
	-I$(_phStbSystemManager_DIR)/inc \
	-I$(_phStbDFB_DIR)/inc \
	-I$(BUILDROOT) \
	-L$(STAGINGDIR)/usr/lib \
	-L$(STAGINGDIR)/usr/local/lib \
	-L$(STAGINGDIR)/opt/philips/lib \
	-Iinclude \


LOCAL_LDFLAGS += \
	-L${ROOTFS}/usr/lib \
	-lcurl -lssl -lcrypto \


LIBS += \
	phStbEvent

ifeq ($(ARCH),mips)
ifeq ($(_TMBSL),_stb225)
LOCAL_CFLAGS += \
	-I$(_tmSbm_DIR)/inc \
	-I$(_tmgdrTypes_DIR)/inc \
	-I$(_tmGsl_DIR)/inc \
	-I$(_tmosa2osal_DIR)/inc \
	-I$(_tmosal_DIR)/inc \
	-I$(_phStbSystemManager_DIR)/inc \
	-I$(_phStbDFBVideoProviderCommonElc_DIR)/inc \
	-I$(_phStbDFBVideoProvider225Elc_DIR)/inc \
	-I$(_IphStbCommon_DIR)/inc \
	-I$(_phStbRpc_DIR)/inc \
	-I$(_phStbRCProtocol_DIR)/inc \
	-I$(_phStbFB_DIR)/inc \
	-DSTBxx \
	-DSTBPNX \
	-DSTB225 \

#	-g -O0

LOCAL_CXXFLAGS += \

#	-g -O0

REQUIRES = \
	phStbDFBVideoProvider225Elc \


LIBS =	phStbSbmSink \
	phKernelIfce \
	phStbDbg \
	phStbSystemManager

else
LOCAL_CFLAGS += \
	-DSTBxx \
	-DSTBPNX \
	-DSTB6x8x \
	-DSTB82

LIBS += phStbMpegTsTrickMode	\
		phStbSystemManager

endif
endif

ifeq ($(INCLUDE_CCRTP),1)
LOCAL_CFLAGS += \
	-Iccrtp-1.5.0/src \
	-Icommoncpp2-1.5.0/include \
	-Iopenssl-0.9.8d/include \

endif

ifeq ($(FORCE_HELPER_SURFACE),1)
LOCAL_CFLAGS += \
	-DGFX_USE_HELPER_SURFACE
endif

#LIBS = \
#	phStbVideoRenderer \
#	phStbMpegTsTrickMode \
#	phStbEvent

LOCAL_LDFLAGS += \
	-Lccrtp-1.5.0/src/.libs -Lcommoncpp2-1.5.0/src/.libs -Lopenssl-0.9.8d
else
LOCAL_CFLAGS += \
	-I/usr/include/directfb \
	-DIMAGE_DIR="\"${ROOTFS}/opt/elecard/share/StbMainApp/images/\"" \
	-DFONT_DIR="\"${ROOTFS}/opt/elecard/share/StbMainApp/fonts/\"" \
	-DRTSP_STREAM_FILE="\"${ROOTFS}/opt/elecard/share/StbMainApp/streams.txt\"" \
	-DCHANNEL_FILE_NAME="\"${ROOTFS}/opt/elecard/share/StbMainApp/channels.conf\"" \
	-DSETTINGS_FILE="\"${ROOTFS}/opt/elecard/share/StbMainApp/settings.conf\"" \
	-DPLAYLIST_FILENAME="\"/config/StbMainApp/playlist.txt\"" \
	-DGFX_USE_HELPER_SURFACE

LOCAL_LDFLAGS += \
#	-lphStbMpegTsTrickMode
#	-Llive555-20060707/liveMedia/ \
#	-Llive555-20060707/groupsock/ \
#	-Llive555-20060707/UsageEnvironment/ \
#	-Llive555-20060707/BasicUsageEnvironment/
endif

C_SOURCES = $(wildcard src/*.c)

CXX_SOURCES = $(wildcard src/*.cpp) \
	      TSDemuxGetStreams/TSGetStreamInfo.cpp \
	      tinyxml/tinyxml.cpp tinyxml/tinyxmlparser.cpp tinyxml/tinyxmlerror.cpp tinyxml/tinystr.cpp

INSTALL_EXTRA = copy_images copy_fonts copy_config copy_laguages copy_sounds

ifeq ($(_TMBSL),_stb225)
LOCAL_CXXFLAGS += $(LOCAL_CFLAGS)
endif

include $(PRJROOT)/etc/application_last.mk

copy_images :
	@echo 'INSTALL $(PROGRAM)/images -> $(DATA_DIR)/images'
	rm -f $(DATA_DIR)/images/*
	$(Q)mkdir -p $(DATA_DIR)/images
	$(Q)install -m0666 images/*.png $(DATA_DIR)/images
	$(Q)install -m0666 images/*.jpg $(DATA_DIR)/images || true

copy_config :
	@echo 'INSTALL $(PROGRAM)/streams.txt & channels.conf & settings.conf -> $(CONFIG_DIR)'
	$(Q)mkdir -p $(CONFIG_DIR)
	$(Q)install -m0666 streams.txt $(CONFIG_DIR)
	$(Q)install -m0666 channels.conf $(CONFIG_DIR)
	$(Q)install -m0666 settings.conf $(CONFIG_DIR)

copy_fonts :
	@echo 'INSTALL $(PROGRAM)/fonts -> $(DATA_DIR)/fonts'
	rm -f $(DATA_DIR)/fonts/*
	$(Q)mkdir -p $(DATA_DIR)/fonts
	$(Q)install -m0666 fonts/*.ttf $(DATA_DIR)/fonts

copy_laguages :
	@echo 'INSTALL $(PROGRAM)/languages -> $(DATA_DIR)/languages'
	rm -f $(DATA_DIR)/languages/*
	$(Q)mkdir -p $(DATA_DIR)/languages
	$(Q)install -m0666 languages/*.lng $(DATA_DIR)/languages

copy_sounds :
	echo 'INSTALL $(PROGRAM)/sounds -> $(DATA_DIR)/sounds'
	rm -f $(DATA_DIR)/sounds/*
	$(Q)mkdir -p $(DATA_DIR)/sounds
	$(Q)install -m0666 sounds/*.wav $(DATA_DIR)/sounds || true
