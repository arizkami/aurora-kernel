# Aurora Kernel Makefile
# NTCore Project - Windows Kernel Implementation

# Compiler and tools
CC = x86_64-w64-mingw32-gcc
LD = x86_64-w64-mingw32-ld
AR = x86_64-w64-mingw32-ar
OBJCOPY = x86_64-w64-mingw32-objcopy

# Compiler flags for kernel mode
CFLAGS = -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
CFLAGS += -O2 -m64 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector
CFLAGS += -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables
CFLAGS += -mno-red-zone -mno-mmx -mno-sse -mno-sse2
CFLAGS += -Iinclude -DAURORA_KERNEL=1

# Linker flags for PE32+ kernel executable
LDFLAGS = -m i386pep -nostdlib -T kernel.lds
LDFLAGS += --subsystem native --image-base 0x140000000
LDFLAGS += --file-alignment 0x200 --section-alignment 0x1000
LDFLAGS += --entry KiSystemStartup

# Directories
SRCDIR = .
OBJDIR = obj
BINDIR = bin
WMIDIR = wmi
KERNDIR = kern
FSDIR = fs
RTLDIR = rtl
BOOTDIR = boot
EFIDIR = $(BOOTDIR)/efi
LEGACYDIR = $(BOOTDIR)/legacy

# Target
TARGET = $(BINDIR)/aurkern.exe
LOADER_EFI = $(BINDIR)/loader.efi
LOADER_ELF = $(BINDIR)/loader.elf

# Entry point object (should be first)
ENTRY_OBJ = $(OBJDIR)/$(KERNDIR)/entry.o

# WMI Source files
WMI_SOURCES = $(WMIDIR)/wmi.c $(WMIDIR)/wmi_provider.c
WMI_ARCH_SOURCES = $(WMIDIR)/amd64/wmi_arch.c

# Kernel Source files
KERN_SOURCES = $(KERNDIR)/kern.c $(KERNDIR)/scheduler.c $(KERNDIR)/syscall.c $(KERNDIR)/arch_shim.c
KERN_ARCH_SOURCES = $(KERNDIR)/amd64/kern_arch.c

# File System Source files
FS_SOURCES = $(FSDIR)/fs.c $(FSDIR)/fat32.c $(FSDIR)/exfat.c $(FSDIR)/ntfs.c

# Runtime sources
RTL_SOURCES = $(RTLDIR)/runtime.c $(RTLDIR)/aurora_runtime.c

# All source files (excluding entry point)
SOURCES = $(WMI_SOURCES) $(WMI_ARCH_SOURCES) $(KERN_SOURCES) $(KERN_ARCH_SOURCES) $(FS_SOURCES) $(RTL_SOURCES)

# Object files
OBJECTS = $(WMI_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(WMI_ARCH_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(KERN_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(KERN_ARCH_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(FS_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(RTL_SOURCES:%.c=$(OBJDIR)/%.o)

# All objects including entry point
ALL_OBJECTS = $(ENTRY_OBJ) $(OBJECTS)

# Default target
all: $(TARGET) loader

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/$(WMIDIR)
	mkdir -p $(OBJDIR)/$(WMIDIR)/amd64
	mkdir -p $(OBJDIR)/$(KERNDIR)
	mkdir -p $(OBJDIR)/$(KERNDIR)/amd64
	mkdir -p $(OBJDIR)/$(FSDIR)
	mkdir -p $(OBJDIR)/$(RTLDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Create kernel linker script if it doesn't exist
kernel.lds:
	@echo "Creating kernel linker script..."
	@echo "SECTIONS" > kernel.lds
	@echo "{" >> kernel.lds
	@echo "    . = 0x140000000;" >> kernel.lds
	@echo "    .text : { *(.text*) }" >> kernel.lds
	@echo "    .rdata : { *(.rdata*) *(.rodata*) }" >> kernel.lds
	@echo "    .data : { *(.data*) }" >> kernel.lds
	@echo "    .bss : { *(.bss*) *(COMMON) }" >> kernel.lds
	@echo "    /DISCARD/ : { *(.note*) *(.comment*) }" >> kernel.lds
	@echo "}" >> kernel.lds

# Build target - link as PE32+ executable
$(TARGET): $(ALL_OBJECTS) kernel.lds | $(BINDIR)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJECTS)
	@echo "Kernel executable created: $@"
	@file $@ || echo "File command not available"

# Entry point (must be first object)
$(ENTRY_OBJ): $(KERNDIR)/entry.c | $(OBJDIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Check if entry.c exists, create minimal one if not
$(KERNDIR)/entry.c:
	@echo "Creating minimal kernel entry point..."
	@mkdir -p $(KERNDIR)
	@echo "// Kernel Entry Point" > $@
	@echo "void KiSystemStartup(void) {" >> $@
	@echo "    // Kernel initialization code goes here" >> $@
	@echo "    while(1) { __asm__(\"hlt\"); }" >> $@
	@echo "}" >> $@

# Compile source files
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJDIR) $(BINDIR) kernel.lds

# Install debug symbols
debug: $(TARGET)
	$(OBJCOPY) --only-keep-debug $(TARGET) $(TARGET).debug
	$(OBJCOPY) --strip-debug $(TARGET)
	$(OBJCOPY) --add-gnu-debuglink=$(TARGET).debug $(TARGET)

# WMI-specific targets
wmi: $(OBJDIR)/wmi/wmi.o $(OBJDIR)/wmi/wmi_provider.o

wmi-amd64: $(OBJDIR)/$(WMIDIR)/amd64/wmi_arch.o

wmi-all: wmi wmi-amd64

# Kernel-specific targets  
kern: $(OBJDIR)/kern/kern.o $(OBJDIR)/kern/scheduler.o $(OBJDIR)/kern/syscall.o

kern-amd64: $(OBJDIR)/$(KERNDIR)/amd64/kern_arch.o

kern-all: kern kern-amd64

# FS-specific targets
fs: $(OBJDIR)/$(FSDIR)/fs.o $(OBJDIR)/$(FSDIR)/fat32.o $(OBJDIR)/$(FSDIR)/exfat.o $(OBJDIR)/$(FSDIR)/ntfs.o

# Show target information
info:
	@echo "Target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Linker: $(LD)"
	@echo "Objects: $(words $(ALL_OBJECTS)) files"

# ---------------- EFI/Legacy Loader ----------------
GNUEFI_DIR = external/gnuefi
HOST_OBJCOPY = objcopy
EFIINC = -I$(GNUEFI_DIR)/inc -I$(GNUEFI_DIR)/inc/x86_64
EFICRT0 = $(GNUEFI_DIR)/x86_64/gnuefi/crt0-efi-x86_64.o
EFILIBA = $(GNUEFI_DIR)/x86_64/gnuefi/libgnuefi.a
EFILIBEFI = $(GNUEFI_DIR)/x86_64/lib/libefi.a
EFILIB = $(EFICRT0) $(EFILIBA) $(EFILIBEFI)
EFICC = gcc
EFILD = ld
EFILDFLAGS = -nostdlib -shared -Bsymbolic -T $(GNUEFI_DIR)/gnuefi/elf_x86_64_efi.lds
HOST_CC = gcc

loader: $(LOADER_EFI) $(LOADER_ELF)

$(EFILIB):
	$(MAKE) -C $(GNUEFI_DIR)


$(LOADER_EFI): $(EFIDIR)/loader.c $(EFILIB) | $(BINDIR) $(OBJDIR)
	$(EFICC) -fpic -fshort-wchar -mno-red-zone -DEFI_FUNCTION_WRAPPER \
		$(EFIINC) -c $< -o $(OBJDIR)/efi_loader.o
	$(EFILD) $(EFILDFLAGS) $(OBJDIR)/efi_loader.o $(EFILIB) -o $(BINDIR)/loader.so
	$(HOST_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela \
		-j .rel.* -j .rela.* -j .reloc --target=efi-app-x86_64 $(BINDIR)/loader.so $@

$(LOADER_ELF): $(LEGACYDIR)/loader.c | $(BINDIR)
	$(HOST_CC) -ffreestanding -nostdlib -m64 -Wl,-Ttext=0x100000 $< -o $@

.PHONY: loader

.PHONY: all clean debug wmi wmi-amd64 wmi-all kern kern-amd64 kern-all fs info