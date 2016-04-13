# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# We're moving the emulator-specific platform libs to
# development.git/tools/emulator/. The following test is to ensure
# smooth builds even if the tree contains both versions.
#
LOCAL_PATH := $(call my-dir)

include $(call all-subdir-makefiles)

#############################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := nfcd.cpp

 LOCAL_C_INCLUDES	:= bionic \
 			   external/stlport/stlport \
 			   device/aicVM/common/libandroidincloud \
 			   device/aicVM/common/libcppsensors_packet \
 			   external/protobuf/src \
   			   external/protobuf/src/google/protobuf \
   			   external/protobuf/src/google/protobuf/stubs \
   			   external/protobuf/src/google/protobuf/io \
   			   external/libnfc-nci/halimpl/bcm2079x/include  \
   			   external/libnfc-nci/src              \
   			   external/libnfc-nci/src/include      \
   			   external/libnfc-nci/src/gki/ulinux 	\
   			   external/libnfc-nci/src/hal/include  \
   			   external/libnfc-nci/src/gki/common	\
   			   external/libnfc-nci/src/nfc/include

IGNORED_WARNINGS := -Wno-sign-compare -Wno-unused-parameter -Wno-sign-promo -Wno-error=return-type
LOCAL_CFLAGS := -O2 -fpermissive -Wmissing-field-initializers -DGOOGLE_PROTOBUF_NO_RTTI

LOCAL_MODULE := nfcd
LOCAL_SHARED_LIBRARIES := liblog libcutils libstlport libcppsensors_packet libnfc-nci
LOCAL_STATIC_LIBRARIES += libprotobuf-cpp-2.3.0-lite libprotobuf-cpp-2.3.0-full
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

