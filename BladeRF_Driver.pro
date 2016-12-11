#==========================================================================================
# + + +   This Software is released under the "Simplified BSD License"  + + +
# Copyright 2014 F4GKR Sylvain AZARIAN . All rights reserved.
#
#Redistribution and use in source and binary forms, with or without modification, are
#permitted provided that the following conditions are met:
#
#   1. Redistributions of source code must retain the above copyright notice, this list of
#	  conditions and the following disclaimer.
#
#   2. Redistributions in binary form must reproduce the above copyright notice, this list
#	  of conditions and the following disclaimer in the documentation and/or other materials
#	  provided with the distribution.
#
#THIS SOFTWARE IS PROVIDED BY Sylvain AZARIAN F4GKR ``AS IS'' AND ANY EXPRESS OR IMPLIED
#WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
#FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Sylvain AZARIAN OR
#CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
#ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
#ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#The views and conclusions contained in the software and documentation are those of the
#authors and should not be interpreted as representing official policies, either expressed
#or implied, of Sylvain AZARIAN F4GKR.
#
# Adds BladeRF capability to SDRNode
#==========================================================================================

QT       -= core gui

TARGET = CloudSDR_BladeRF
TEMPLATE = lib


LIBS += -lpthread -lusb-1.0  
win32 {
    DEFINES += "_WINDOWS"
    DESTDIR = C:/SDRNode/addons
	RC_FILE = resources.rc
}

unix {
    DESTDIR = /opt/sdrnode/addons
}

SOURCES += \
    entrypoint.cpp \
    jansson/dump.c \
    jansson/error.c \
    jansson/hashtable.c \
    jansson/hashtable_seed.c \
    jansson/load.c \
    jansson/memory.c \
    jansson/pack_unpack.c \
    jansson/strbuffer.c \
    jansson/strconv.c \
    jansson/utf.c \
    jansson/value.c \
    BladeRF/nuand/async.c \
    BladeRF/nuand/bladerf.c \
    BladeRF/nuand/bladerf_priv.c \
    BladeRF/nuand/capabilities.c \
    BladeRF/nuand/config.c \
    BladeRF/nuand/conversions.c \
    BladeRF/nuand/dc_cal_table.c \
    BladeRF/nuand/device_identifier.c \
    BladeRF/nuand/devinfo.c \
    BladeRF/nuand/file_ops.c \
    BladeRF/nuand/flash.c \
    BladeRF/nuand/flash_fields.c \
    BladeRF/nuand/fpga.c \
    BladeRF/nuand/fx3_fw.c \
    BladeRF/nuand/fx3_fw_log.c \
    BladeRF/nuand/gain.c \
    BladeRF/nuand/image.c \
    BladeRF/nuand/init_fini.c \
    BladeRF/nuand/log.c \
    BladeRF/nuand/sha256.c \
    BladeRF/nuand/si5338.c \
    BladeRF/nuand/sync.c \
    BladeRF/nuand/sync_worker.c \
    BladeRF/nuand/tuning.c \
    BladeRF/nuand/version_compat.c \
    BladeRF/nuand/xb.c \
    BladeRF/nuand/backend/backend.c \
    BladeRF/nuand/backend/dummy.c \
    BladeRF/nuand/backend/usb/libusb.c \
    BladeRF/nuand/backend/usb/nios_access.c \
    BladeRF/nuand/backend/usb/nios_legacy_access.c \
    BladeRF/nuand/backend/usb/usb.c \
    BladeRF/nuand/fpga_common/band_select.c \
    BladeRF/nuand/fpga_common/lms.c

HEADERS +=\
    external_hardware_def.h \
    entrypoint.h \
    jansson/hashtable.h \
    jansson/jansson.h \
    jansson/jansson_config.h \
    jansson/jansson_private.h \
    jansson/lookup3.h \
    jansson/strbuffer.h \
    jansson/utf.h \
    BladeRF/nuand/async.h \
    BladeRF/nuand/bladeRF.h \
    BladeRF/nuand/bladerf_priv.h \
    BladeRF/nuand/capabilities.h \
    BladeRF/nuand/clock_gettime.h \
    BladeRF/nuand/config.h \
    BladeRF/nuand/conversions.h \
    BladeRF/nuand/dc_cal_table.h \
    BladeRF/nuand/device_identifier.h \
    BladeRF/nuand/devinfo.h \
    BladeRF/nuand/file_ops.h \
    BladeRF/nuand/flash.h \
    BladeRF/nuand/flash_fields.h \
    BladeRF/nuand/fpga.h \
    BladeRF/nuand/fx3_fw.h \
    BladeRF/nuand/fx3_fw_log.h \
    BladeRF/nuand/gain.h \
    BladeRF/nuand/host_config.h \
    BladeRF/nuand/libbladeRF.h \
    BladeRF/nuand/log.h \
    BladeRF/nuand/logger_entry.h \
    BladeRF/nuand/logger_id.h \
    BladeRF/nuand/metadata.h \
    BladeRF/nuand/minmax.h \
    BladeRF/nuand/rel_assert.h \
    BladeRF/nuand/sha256.h \
    BladeRF/nuand/si5338.h \
    BladeRF/nuand/sync.h \
    BladeRF/nuand/sync_worker.h \
    BladeRF/nuand/thread.h \
    BladeRF/nuand/tuning.h \
    BladeRF/nuand/types.h \
    BladeRF/nuand/version.h \
    BladeRF/nuand/version_compat.h \
    BladeRF/nuand/xb.h \
    BladeRF/nuand/backend/backend.h \
    BladeRF/nuand/backend/backend_config.h \
    BladeRF/nuand/backend/dummy.h \
    BladeRF/nuand/backend/usb/nios_access.h \
    BladeRF/nuand/backend/usb/nios_legacy_access.h \
    BladeRF/nuand/backend/usb/usb.h \
    BladeRF/nuand/fpga_common/band_select.h \
    BladeRF/nuand/fpga_common/lms.h \
    BladeRF/nuand/fpga_common/nios_pkt_8x8.h \
    BladeRF/nuand/fpga_common/nios_pkt_8x16.h \
    BladeRF/nuand/fpga_common/nios_pkt_8x32.h \
    BladeRF/nuand/fpga_common/nios_pkt_8x64.h \
    BladeRF/nuand/fpga_common/nios_pkt_32x32.h \
    BladeRF/nuand/fpga_common/nios_pkt_formats.h \
    BladeRF/nuand/fpga_common/nios_pkt_legacy.h \
    BladeRF/nuand/fpga_common/nios_pkt_retune.h \
    driver_version.h \
    resources.rc

unix {
    #target.path = /usr/lib
    #INSTALLS += target
}
