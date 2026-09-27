# Resilio Versio firmware (standard libDaisy app, internal flash, ADR 0011).
TARGET = resilio_versio

CPP_SOURCES = main.cpp $(wildcard ../core/dsp/*.cpp)
C_INCLUDES += -I../core
CPP_STANDARD = -std=gnu++17
OPT = -O3

LIBDAISY_DIR = ../libs/libDaisy

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Internal flash limit for NE Firmware Swap installs (ADR 0011).
FLASH_LIMIT = 131072

.PHONY: size
size: $(BUILD_DIR)/$(TARGET).bin
	@bytes=$$(wc -c < $(BUILD_DIR)/$(TARGET).bin | tr -d ' '); \
	pct=$$((bytes * 100 / $(FLASH_LIMIT))); \
	echo "$(TARGET).bin: $$bytes / $(FLASH_LIMIT) bytes ($$pct%)"; \
	if [ $$bytes -gt $(FLASH_LIMIT) ]; then echo "ERROR: exceeds internal flash (ADR 0011)"; exit 1; fi
