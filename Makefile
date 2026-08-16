PRODUCT = fujirkle
PRODUCT_UPPER = FUJIRKLE
PLATFORMS = coco msdos atari

# CoCo targets:
#   make coco        → CoCo 1/2 build
#   make coco3       → CoCo 3 build (320x200x16, MSDOS-style layout)
#   make coco-dist   → combined disk with loader + both CoCo binaries

# SRC_DIRS may use the literal %PLATFORM% token.
# It expands to the chosen PLATFORM plus any of its combos.
SRC_DIRS = src src/%PLATFORM%

# src/platform-specific is needed by everyone (#include "platform-specific/foo.h")
INCLUDE_DIRS = src/platform-specific

# src/include holds wrappers (stdint.h, conio.h ...) for non-cc65 toolchains.
# cc65 ships its own real versions, so adding it globally would shadow them.
EXTRA_INCLUDE_COCO = src/include

# msdos needs fujinet-lib-experimental for RS232 support; scoped so it does not
# drag the experimental branch into the other platforms.
FUJINET_LIB =
ifeq ($(PLATFORM),msdos)
  FUJINET_LIB = https://github.com/FozzTexx/fujinet-lib-experimental.git
endif

# CoCo: optimization + memory layout
CFLAGS_EXTRA_COCO  += -fomit-frame-pointer -O2 -Wno-const

ifeq ($(MAKE_COCO3),COCO3)
  # CoCo 3: the 32K screen lives in MMU blocks 52-55, so the program can
  # start lower and keep clear of the $8000 graphics window.
  COCO_ORG = 1000
  CFLAGS_EXTRA_COCO  += -DCOCO3
  # --initial-s puts the stack at the top of RAM below the $8000 screen window.
  LDFLAGS_EXTRA_COCO += --org=$(COCO_ORG) --limit=7E00 --initial-s=8000
else
  COCO_ORG = 1A00
  LDFLAGS_EXTRA_COCO += --org=$(COCO_ORG) --limit=7B00
endif

ifeq ($(LAYOUT_DEMO),1)
  CFLAGS_EXTRA_COCO += -DLAYOUT_DEMO
endif

# Support 'make coco3'
coco3:
	$(MAKE) coco MAKE_COCO3=COCO3

# MS-DOS (Watcom): bundle AUTOEXEC.BAT into the disk image
msdos/disk-post::
	mcopy -t -i $(DISK) src/msdos/AUTOEXEC.BAT "::AUTOEXEC.BAT"

include mekkogx/toplevel-rules.mk

#################################################################
## CUSTOM DISTRIBUTION RECIPES                                 ##
#################################################################

# "FUJIRKLE" is already 8 characters, so the per-model binaries use the
# shortened FUJIRKL1 / FUJIRKL3 names that support/coco/loader.c expects.
R2R_PRODUCT = r2r/coco/$(PRODUCT)
COCO_DISK   = $(R2R_PRODUCT).dsk

# Combined CoCo 1/2 + CoCo 3 disk, with a loader that auto-detects the model.
coco-dist:
	$(MAKE) clean
	rm -rf build
	$(MAKE) coco
	mv $(R2R_PRODUCT).bin $(R2R_PRODUCT)1.bin

	rm -rf build
	$(MAKE) coco3
	mv $(R2R_PRODUCT).bin $(R2R_PRODUCT)3.bin

	cmoc -o $(R2R_PRODUCT).bin support/coco/loader.c

	$(RM) $(COCO_DISK)
	decb dskini $(COCO_DISK)
	mkdir -p build/coco
	echo RUNM\"$(PRODUCT_UPPER)\" > build/coco/autoexec.bas
	decb copy -t -0 build/coco/autoexec.bas $(COCO_DISK),AUTOEXEC.BAS
	decb copy -b -2 $(R2R_PRODUCT).bin  $(COCO_DISK),$(PRODUCT_UPPER).BIN
	decb copy -b -2 $(R2R_PRODUCT)1.bin $(COCO_DISK),FUJIRKL1.BIN
	decb copy -b -2 $(R2R_PRODUCT)3.bin $(COCO_DISK),FUJIRKL3.BIN

# Layout mock-up disk: same auto-detecting loader, but both binaries show the
# candidate board layouts instead of connecting to a server.
DEMO_DISK = r2r/coco/fujirkle-demo.dsk

coco-demo:
	$(MAKE) clean
	rm -rf build
	$(MAKE) coco LAYOUT_DEMO=1
	mv $(R2R_PRODUCT).bin $(R2R_PRODUCT)1.bin

	rm -rf build
	$(MAKE) coco MAKE_COCO3=COCO3 LAYOUT_DEMO=1
	mv $(R2R_PRODUCT).bin $(R2R_PRODUCT)3.bin

	cmoc -o $(R2R_PRODUCT).bin support/coco/loader.c

	$(RM) $(DEMO_DISK)
	decb dskini $(DEMO_DISK)
	mkdir -p build/coco
	echo RUNM\"$(PRODUCT_UPPER)\" > build/coco/autoexec.bas
	decb copy -t -0 build/coco/autoexec.bas $(DEMO_DISK),AUTOEXEC.BAS
	decb copy -b -2 $(R2R_PRODUCT).bin  $(DEMO_DISK),$(PRODUCT_UPPER).BIN
	decb copy -b -2 $(R2R_PRODUCT)1.bin $(DEMO_DISK),FUJIRKL1.BIN
	decb copy -b -2 $(R2R_PRODUCT)3.bin $(DEMO_DISK),FUJIRKL3.BIN
