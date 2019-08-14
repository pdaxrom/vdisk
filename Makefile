TARGET = vdisk.efi

LD = ld
OBJCOPY = objcopy

CFLAGS = -Os -fno-strict-aliasing -fno-stack-protector -fshort-wchar -Wall -DEFIX64 -DEFI_FUNCTION_WRAPPER -m64 -mno-red-zone  -fpic
CFLAGS += -I/usr/include/efi -I/usr/include/efi/x86_64 -I/usr/include/efi/protocol

OBJS = vdisk.o

$(TARGET): $(OBJS)
	$(LD) -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic -nostdlib -L/usr/lib -L/usr/lib /usr/lib/crt0-efi-x86_64.o -znocombreloc -zdefs \
	    $^ -o $@.so -lefi -lgnuefi /usr/lib/gcc/x86_64-linux-gnu/7/libgcc.a
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
 -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* \
 -j .reloc --target=efi-bsdrv-x86_64 $@.so $@

clean:
	rm -f *.o *.so *.efi
