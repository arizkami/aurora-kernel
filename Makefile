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
CFLAGS += -Iinclude -I/usr/x86_64-w64-mingw32/include -DAURORA_KERNEL=1

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
PROCDIR = proc
IODIR = io
HALDIR = hal
PERFDIR = perf
RAWDIR = raw
IPCDIR = ipc
L4DIR = l4
FIASCODIR = fiasco
EXT_FIASCO_DIR = external/fiasco/src

# Minimal imported Fiasco sources (future expansion). Initially none compiled to avoid C++ toolchain needs.
EXT_FIASCO_SOURCES = 

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
ACPI_SOURCES = $(KERNDIR)/acpi.c
FONT_SOURCES = $(KERNDIR)/font_spleen.c
KERN_ARCH_SOURCES = $(KERNDIR)/amd64/kern_arch.c

# File System Source files
FS_SOURCES = $(FSDIR)/fs.c \
			 $(FSDIR)/fat32/driver.c \
			 $(FSDIR)/exfat/driver.c \
			 $(FSDIR)/ntfs/driver.c

# Runtime sources
RTL_SOURCES = $(RTLDIR)/runtime.c $(RTLDIR)/aurora_runtime.c

# Memory manager sources
MEMDIR = mem
MEM_SOURCES = $(MEMDIR)/mem.c
MEM_ASM_SOURCES = $(MEMDIR)/amd64/flush.S

# Process sources
PROC_SOURCES = $(PROCDIR)/proc.c $(PROCDIR)/amd64/proc_arch.c
PROC_ASM_SOURCES = $(PROCDIR)/amd64/enter_user.S

# Configuration system sources
CONFIGDIR = config
HIVEDIR = $(CONFIGDIR)/hive
NTCOREDIR = $(CONFIGDIR)/ntcore

# Hive system sources
HIVE_SOURCES = $(HIVEDIR)/hivebin.c $(HIVEDIR)/hivecell.c $(HIVEDIR)/hivechek.c \
			   $(HIVEDIR)/hivefree.c $(HIVEDIR)/hivehint.c $(HIVEDIR)/hiveinit.c \
			   $(HIVEDIR)/hiveload.c $(HIVEDIR)/hivemap.c $(HIVEDIR)/hivesum.c \
			   $(HIVEDIR)/hivesync.c $(HIVEDIR)/hivelock.c $(HIVEDIR)/hiveops.c

# NTCore API sources
NTCORE_SOURCES = $(NTCOREDIR)/api.c $(NTCOREDIR)/pe.c

# All source files (excluding entry point)
IO_SOURCES = $(IODIR)/io.c $(IODIR)/driver.c $(IODIR)/device.c $(IODIR)/irp.c $(IODIR)/pnp/pnp.c
PERF_SOURCES = $(PERFDIR)/perf.c
RAW_SOURCES = $(RAWDIR)/raw.c
IPC_SOURCES = $(IPCDIR)/ipc.c
L4_SOURCES = $(L4DIR)/l4.c
FIASCO_SOURCES = $(FIASCODIR)/fiasco.c

HAL_SOURCES = $(HALDIR)/hal.c
HAL_ASM_SOURCES = $(HALDIR)/amd64/hal_arch.S

SOURCES = $(WMI_SOURCES) $(WMI_ARCH_SOURCES) $(KERN_SOURCES) $(ACPI_SOURCES) $(FONT_SOURCES) $(KERN_ARCH_SOURCES) $(FS_SOURCES) $(RTL_SOURCES) $(MEM_SOURCES) $(MEM_ASM_SOURCES) $(PROC_SOURCES) $(PROC_ASM_SOURCES) $(HIVE_SOURCES) $(NTCORE_SOURCES) $(IO_SOURCES) $(HAL_SOURCES) $(HAL_ASM_SOURCES) $(PERF_SOURCES) $(RAW_SOURCES) $(IPC_SOURCES) $(L4_SOURCES) $(FIASCO_SOURCES) $(EXT_FIASCO_SOURCES)

# Object files
OBJECTS = $(WMI_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(WMI_ARCH_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(KERN_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(ACPI_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(FONT_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(KERN_ARCH_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(FS_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(RTL_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(MEM_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(MEM_ASM_SOURCES:%.S=$(OBJDIR)/%.o) \
		  $(PROC_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(PROC_ASM_SOURCES:%.S=$(OBJDIR)/%.o) \
		  $(HIVE_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(NTCORE_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(IO_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(HAL_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(HAL_ASM_SOURCES:%.S=$(OBJDIR)/%.o) \
		  $(PERF_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(RAW_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(IPC_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(L4_SOURCES:%.c=$(OBJDIR)/%.o) \
		  $(FIASCO_SOURCES:%.c=$(OBJDIR)/%.o)

# All objects including entry point
ALL_OBJECTS = $(ENTRY_OBJ) $(OBJECTS)

# Default target
all: $(TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/$(WMIDIR)
	mkdir -p $(OBJDIR)/$(WMIDIR)/amd64
	mkdir -p $(OBJDIR)/$(KERNDIR)
	mkdir -p $(OBJDIR)/$(KERNDIR)/amd64
	mkdir -p $(OBJDIR)/$(FSDIR)
	mkdir -p $(OBJDIR)/$(RTLDIR)
	mkdir -p $(OBJDIR)/$(MEMDIR)
	mkdir -p $(OBJDIR)/$(MEMDIR)/amd64
	mkdir -p $(OBJDIR)/$(PROCDIR)
	mkdir -p $(OBJDIR)/$(PROCDIR)/amd64
	mkdir -p $(OBJDIR)/$(CONFIGDIR)
	mkdir -p $(OBJDIR)/$(HIVEDIR)
	mkdir -p $(OBJDIR)/$(NTCOREDIR)
	mkdir -p $(OBJDIR)/$(IODIR)
	mkdir -p $(OBJDIR)/$(IODIR)/pnp
	mkdir -p $(OBJDIR)/$(HALDIR)
	mkdir -p $(OBJDIR)/$(HALDIR)/amd64
	mkdir -p $(OBJDIR)/$(PERFDIR)
	mkdir -p $(OBJDIR)/$(RAWDIR)
	mkdir -p $(OBJDIR)/$(IPCDIR)
	mkdir -p $(OBJDIR)/$(L4DIR)
	mkdir -p $(OBJDIR)/$(FIASCODIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Auto-generate Spleen font C source (primary: BDF; fallback: ASM)
$(KERNDIR)/font_spleen.c: external/spleen/spleen-8x16.bdf external/spleen/dos/spleen.asm tools/gen_spleen.py
	@if command -v python3 >/dev/null 2>&1; then \
	  echo "[GEN] Spleen font (BDF)"; \
	  python3 tools/gen_spleen.py external/spleen/spleen-8x16.bdf $@ external/spleen/dos/spleen.asm; \
	else \
	  echo "python3 not found; using existing font_spleen.c"; \
	fi

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

# Assemble .S files
$(OBJDIR)/%.o: %.S | $(OBJDIR)
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