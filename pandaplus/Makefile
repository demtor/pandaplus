# Cross toolchain variables
# If these are not in your path, you can make them absolute.
XT_PRG_PREFIX = mipsel-linux-gnu-
CC = $(XT_PRG_PREFIX)gcc
LD = $(XT_PRG_PREFIX)ld

PANDAPLUS_SRC_DIR = ./src
PANDAPLUS_OBJ_DIR = ./build

# Simplistic search for the umps3 installation prefix.
# If you have umps3 installed on some weird location, set UMPS3_DIR_PREFIX by hand.
ifneq ($(wildcard /usr/bin/umps3),)
	UMPS3_DIR_PREFIX = /usr
else
	UMPS3_DIR_PREFIX = /usr/local
endif

UMPS3_DATA_DIR = $(UMPS3_DIR_PREFIX)/share/umps3
UMPS3_INCLUDE_DIR = $(UMPS3_DIR_PREFIX)/include/umps3
PANDAPLUS_INCLUDE_DIR = $(PANDAPLUS_SRC_DIR)/include

# Compiler options
CFLAGS_LANG = -ffreestanding -std=gnu11
CFLAGS_MIPS = -mips1 -mabi=32 -mno-gpopt -G 0 -mno-abicalls -fno-pic -mfp32
CFLAGS = $(CFLAGS_LANG) $(CFLAGS_MIPS) -I$(UMPS3_INCLUDE_DIR) -I$(PANDAPLUS_INCLUDE_DIR) -Wall -O0 -DDEBUG

# Linker options
LDFLAGS = -G 0 -nostdlib -T $(UMPS3_DATA_DIR)/umpscore.ldscript

# Add the location of crt*.S to the search path
VPATH = $(UMPS3_DATA_DIR)

# List of all objects required by Panda+
PANDAPLUS_OBJ_FILES = $(patsubst $(PANDAPLUS_SRC_DIR)/%.c,$(PANDAPLUS_OBJ_DIR)/%.o,$(shell find $(PANDAPLUS_SRC_DIR) -type f -name '*.c'))

.PHONY : all clean

all : kernel.core.umps

kernel.core.umps : kernel
	umps3-elf2umps -k $<

kernel : $(PANDAPLUS_OBJ_FILES) $(addprefix $(PANDAPLUS_OBJ_DIR)/, crtso.o libumps.o)
	$(LD) -o $@ $^ $(LDFLAGS)

# Pattern rule for source files
$(PANDAPLUS_OBJ_DIR)/%.o : $(PANDAPLUS_SRC_DIR)/%.c
	@ mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Pattern rule for assembly modules
$(PANDAPLUS_OBJ_DIR)/%.o : %.S
	@ mkdir -p $(PANDAPLUS_OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	rm -f -r $(PANDAPLUS_OBJ_DIR)
	rm -f kernel *.umps
