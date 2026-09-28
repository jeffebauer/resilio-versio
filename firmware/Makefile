# Resilio Versio firmware (standard libDaisy app, internal flash, ADR 0011).
#
# Three variants, one main.cpp, selected by MODE (SPEC §7 M0/M3, ADR 0011):
#   make -C firmware MODE=m0test   -> build/resilio_versio_m0test.bin  (SPEC §7 M0)
#   make -C firmware MODE=profile  -> build/resilio_versio_profile.bin (SPEC §7 M3)
#   make -C firmware [MODE=release]-> build/resilio_versio.bin         (default; the real instrument)
#
# `make -C firmware all-variants` builds all three and reports each size
# against the 128 KB internal-flash limit (ADR 0011), failing if any is over.
#
# PROFILE_BLOCK (profile variant only, default 48, SPEC §5) selects the audio
# block size so M3 can compare 48 vs 96/128:
#   make -C firmware MODE=profile PROFILE_BLOCK=96

MODE ?= release
PROFILE_BLOCK ?= 48

ifeq ($(MODE),m0test)
  TARGET    = resilio_versio_m0test
  MODE_DEFS = -DRV_MODE_M0TEST
else ifeq ($(MODE),profile)
  TARGET    = resilio_versio_profile
  MODE_DEFS = -DRV_MODE_PROFILE
else ifeq ($(MODE),release)
  TARGET    = resilio_versio
  MODE_DEFS = -DRV_MODE_RELEASE
else
  $(error Unknown MODE '$(MODE)': use m0test, profile, or release)
endif

CPP_SOURCES = main.cpp $(wildcard ../core/dsp/*.cpp)
C_INCLUDES += -I../core
C_DEFS     += $(MODE_DEFS) -DRV_PROFILE_BLOCK=$(PROFILE_BLOCK)
CPP_STANDARD = -std=gnu++17
OPT = -O3

# Object files are isolated per (MODE, PROFILE_BLOCK): all three variants
# compile the same main.cpp under different -D flags, so sharing one object
# directory would let a stale main.o from a previous MODE get silently
# relinked (same filename, unrelated content) into the "wrong" variant. Each
# combination gets its own scratch dir under build/, and the final artifacts
# are copied up to build/<TARGET>.* (flat) once linked, so the three .bin
# files always sit side by side under distinct, unambiguous names.
override BUILD_DIR = build/obj-$(MODE)-$(PROFILE_BLOCK)
# libDaisy's own $(BUILD_DIR) rule is a plain `mkdir $@`, which fails for a
# nested path when build/ doesn't exist yet; pre-create it here (mkdir -p).
$(shell mkdir -p $(BUILD_DIR))

LIBDAISY_DIR = ../libs/libDaisy
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Internal flash limit for NE Firmware Swap installs (ADR 0011).
FLASH_LIMIT = 131072

# libDaisy's included Makefile defines `all` (elf+hex+bin in $(BUILD_DIR));
# this appends the copy-up step so `make` alone (default goal = all) leaves
# a flat build/<TARGET>.bin/.elf/.hex behind.
all: copy-artifacts

.PHONY: copy-artifacts
copy-artifacts: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	@mkdir -p build
	@cp $(BUILD_DIR)/$(TARGET).elf build/$(TARGET).elf
	@cp $(BUILD_DIR)/$(TARGET).hex build/$(TARGET).hex
	@cp $(BUILD_DIR)/$(TARGET).bin build/$(TARGET).bin

.PHONY: size
size: build/$(TARGET).bin
	@bytes=$$(wc -c < build/$(TARGET).bin | tr -d ' '); \
	pct=$$((bytes * 100 / $(FLASH_LIMIT))); \
	echo "$(TARGET).bin: $$bytes / $(FLASH_LIMIT) bytes ($$pct%)"; \
	if [ $$bytes -gt $(FLASH_LIMIT) ]; then echo "ERROR: exceeds internal flash (ADR 0011)"; exit 1; fi

# Builds all three variants (release, m0test, profile) and reports each
# size against the 128 KB limit. Fails (nonzero exit) if any variant is over.
.PHONY: all-variants
all-variants:
	@$(MAKE) --no-print-directory MODE=release
	@$(MAKE) --no-print-directory MODE=m0test
	@$(MAKE) --no-print-directory MODE=profile
	@echo ""; \
	echo "=== Variant sizes vs $(FLASH_LIMIT)-byte internal-flash limit (ADR 0011) ==="; \
	fail=0; \
	for f in resilio_versio resilio_versio_m0test resilio_versio_profile; do \
		bytes=$$(wc -c < build/$$f.bin | tr -d ' '); \
		pct=$$((bytes * 100 / $(FLASH_LIMIT))); \
		if [ $$bytes -gt $(FLASH_LIMIT) ]; then status="EXCEEDS LIMIT"; fail=1; else status="ok"; fi; \
		printf "  %-28s %7d / %d bytes (%3d%%)  %s\n" $$f.bin $$bytes $(FLASH_LIMIT) $$pct "$$status"; \
	done; \
	exit $$fail
