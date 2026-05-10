# Build the example guest payload as a flat binary.
#
# Workflow (Apple-toolchain only — no llvm-objcopy required):
#   1. assemble     hello.S  -> hello.o    (Mach-O aarch64 object)
#   2. link         hello.o  -> hello.macho (Mach-O executable, __text starts at 0)
#   3. extract __text via Apple's `segedit` -> hello.bin (flat binary)

CLANG ?= clang

CFLAGS := -target arm64-apple-macos11 -ffreestanding -nostdlib

.PHONY: all clean
all: hello.bin

hello.o: hello.S
	$(CLANG) $(CFLAGS) -c -o $@ $<

hello.macho: hello.o
	ld -arch arm64 -e _start -static -no_pie -pagezero_size 0x0 -o $@ $<

hello.bin: hello.macho
	segedit $< -extract __TEXT __text $@

clean:
	rm -f hello.o hello.macho hello.bin
