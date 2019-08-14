#ifndef _LOOP_IO_H_
#define _LOOP_IO_H_

#include <efi.h>
#include <efilib.h>

#define BASE_CR(Record, TYPE, Field)  ((TYPE *) ((CHAR8 *) (Record) - (CHAR8 *) &(((TYPE *) 0)->Field)))

#define PRIVATE_FROM_BLOCK_IO(a) \
        BASE_CR (a, PRIVATE_BLOCK_IO_DEVICE, BlockIo)

typedef struct {
//    UINT32 Signature;
    EFI_DEVICE_PATH *Device;
    EFI_FILE *File;
    EFI_BLOCK_IO_MEDIA Media;
    EFI_BLOCK_IO BlockIo;
} PRIVATE_BLOCK_IO_DEVICE;

#endif
