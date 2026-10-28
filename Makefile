XCC ?= powerpc-xsdk-elf-gcc
XNM ?= powerpc-xsdk-elf-nm
HOST_CC ?= cc
PYTHON ?= python3
BUILD := build
CORE := src/core.c src/config.c
PLATFORM := platform/xbox360/main.c platform/xbox360/system.c platform/xbox360/usb.c platform/xbox360/hooks.c platform/xbox360/libc.c
IMPORT_NAMES := $(shell $(PYTHON) tools/gen_imports.py --list platform/xbox360/imports.txt)
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(CORE) $(PLATFORM)) $(addprefix $(BUILD)/imports/,$(addsuffix .o,$(IMPORT_NAMES)))
CFLAGS := -std=c17 -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns \
    -fno-stack-protector -fno-common -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -m32 -mbig-endian -mcpu=cell -mpowerpc64 -mcall-xsdk -msdata=none -ffixed-r2 -ffixed-r13 \
    -ffixed-cr2 -ffixed-cr3 -ffixed-cr4 -ffixed-cr5 -ffixed-cr6 -ffixed-cr7 -fno-tree-vectorize \
    -fno-optimize-sibling-calls -fno-reorder-blocks-and-partition -fno-gcse -Iinclude -Iplatform/xbox360

.PHONY: all test test-xex package clean
all: $(BUILD)/MK360.xex
$(BUILD)/imports/.stamp: platform/xbox360/imports.txt tools/gen_imports.py
	mkdir -p $(BUILD)/imports
	$(PYTHON) tools/gen_imports.py $< $(BUILD)/imports
	touch $@
$(BUILD)/imports/%.o: $(BUILD)/imports/.stamp
	$(XCC) $(CFLAGS) -Wa,-mregnames -c $(BUILD)/imports/$*.S -o $@
$(BUILD)/%.o: %.c include/mk360.h platform/xbox360/kernel.h platform/xbox360/usb.h platform/xbox360/hooks.h
	mkdir -p $(dir $@)
	$(XCC) $(CFLAGS) -c $< -o $@
$(BUILD)/MK360.elf: $(OBJECTS) platform/xbox360/plugin.ld
	$(XCC) $(CFLAGS) -nostdlib -Wl,-T,platform/xbox360/plugin.ld,-Map,$(BUILD)/MK360.map -o $@ $(OBJECTS)
	test -z "$$($(XNM) -u $@)"
$(BUILD)/MK360.xex: $(BUILD)/MK360.elf tools/elf2xex.py
	$(PYTHON) tools/elf2xex.py $< $@ --image-base 0x81F00000 --dll --title-id 4D4B3336 --title-name MK360 --stack-size 0x10000 --version 0.1.0.0
	$(PYTHON) tools/verify_xex.py $@
$(BUILD)/test_core: $(CORE) tests/test_core.c include/mk360.h
	mkdir -p $(BUILD)
	$(HOST_CC) -std=c17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(CORE) tests/test_core.c -o $@
test: $(BUILD)/test_core
	ASAN_OPTIONS=detect_leaks=0 $(BUILD)/test_core > $(BUILD)/core-test.log
	cat $(BUILD)/core-test.log
test-xex: $(BUILD)/MK360.xex
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v
package: test test-xex
	$(PYTHON) tools/package.py --compiler "$(XCC)"
clean:
	rm -rf $(BUILD) dist
