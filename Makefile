# SPDX-License-Identifier: GPL-2.0-only
# Derived from exlaunch f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
# Keep the SilkModLoader artifact name after the project split. The runtime
# NPDM is deliberately named main.npdm because that is the ExeFS filename
# consumed by the target game; do not derive a SilkModLoader.npdm from the
# generic devkitPro application manifest.
# See THIRD_PARTY_NOTICES.md and licenses/source-inventory.json.
# Define paths.
PWD := $(shell pwd)
MISC_PATH := $(PWD)/misc
MK_PATH := $(MISC_PATH)/mk
SCRIPTS_PATH := $(MISC_PATH)/scripts
SPECS_PATH := $(MISC_PATH)/specs
OUTPUT_ROOT := $(PWD)/output
BUILD := $(OUTPUT_ROOT)/build

# To configure exlaunch, edit config.mk.
include $(PWD)/config.mk

# Define common variables.
# Keep the deployed/debug artifact name stable while the source project lives
# in the separately submitted SilkRuntime directory.
NAME := SilkModLoader
OUT := $(OUTPUT_ROOT)/deploy
SD_OUT := atmosphere/contents/$(PROGRAM_ID)/exefs

# The runtime package uses the H1e/D-stage Skyline capability manifest.
# These are overridable for the container build, where the workspace is
# mounted at separate paths.
RUNTIME_NPDM_JSON ?= $(MISC_PATH)/npdm-json/skyline.json
RUNTIME_NPDM ?= $(OUTPUT_ROOT)/main.npdm


# Set load kind specific variables.
ifeq ($(LOAD_KIND), Module)
    LOAD_KIND_ENUM := 2
    BINARY_NAME := subsdk9 # TODO: support subsdkX?
    SPECS_NAME := module.specs
    MK_NAME := module.mk
else
    $(error SilkRuntime only supports LOAD_KIND=Module)
endif

.PHONY: clean all

# Built internal C flags variable.
EXL_CFLAGS   := $(C_FLAGS) -DEXL_LOAD_KIND=$(LOAD_KIND) -DEXL_LOAD_KIND_ENUM=$(LOAD_KIND_ENUM) -DEXL_PROGRAM_ID=0x$(PROGRAM_ID)
EXL_CXXFLAGS := $(CXX_FLAGS)

# Export all of our variables to sub-makes and sub-processes.
export

include $(MK_PATH)/$(MK_NAME)
include $(MK_PATH)/common.mk
include $(MK_PATH)/npdm.mk
