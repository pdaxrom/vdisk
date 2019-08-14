/*
 * UEFI Virtual Disk driver
 *
 * (c) Madisa Technology, 2014
 */

#include "vdisk.h"

#define EFI_CALLER_ID_GUID \
    {0x6B38F7B4, 0xAD98, 0x40e9, {0x90, 0x93, 0xAC, 0xA2, 0xB5, 0xA2, 0x53, 0xC4}}

#define SIGNATURE_16(A, B)		((A) | (B << 8))
#define SIGNATURE_32(A, B, C, D)	(SIGNATURE_16 (A, B) | (SIGNATURE_16 (C, D) << 16))
#define SIGNATURE_64(A, B, C, D, E, F, G, H) \
    (SIGNATURE_32 (A, B, C, D) | ((UINT64) (SIGNATURE_32 (E, F, G, H)) << 32))

typedef struct {
    VENDOR_DEVICE_PATH	Guid;
    EFI_DEVICE_PATH	End;
} LOOP_DEVICE_PATH;

LOOP_DEVICE_PATH gDevicePath = {
    {
	{ HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } },
	EFI_CALLER_ID_GUID
    },
    { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH), 0} }
};

EFI_FILE_INFO* FileGetInfo(
    IN EFI_FILE		*File)
{
    EFI_STATUS Status;
    EFI_FILE_INFO *FileInfo;
    UINTN FileInfoSize;
    EFI_GUID fileinfo_guid = EFI_FILE_INFO_ID;

    if (File == NULL) {
	return (NULL);
    }

    FileInfoSize = 0;
    FileInfo = NULL;
    Status = File->GetInfo(File, &fileinfo_guid, &FileInfoSize, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL){
	FileInfo = AllocateZeroPool(FileInfoSize);
	Status = File->GetInfo(File, &fileinfo_guid, &FileInfoSize, FileInfo);
	if (EFI_ERROR(Status) && (FileInfo != NULL)) {
	    FreePool(FileInfo);
	    FileInfo = NULL;
	}
    }
    return FileInfo;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.Reset
//
EFI_STATUS EFIAPI VdiskReset(
    IN  EFI_BLOCK_IO	*This,
    IN  BOOLEAN		ExtendedVerification)
{
    Print(L"SpReset SpReset SpReset *************\n");

    return EFI_SUCCESS;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.ReadBlocks
//
EFI_STATUS EFIAPI VdiskReadBlocks(
    IN  EFI_BLOCK_IO	*This,
    IN  UINT32		MediaId,
    IN  EFI_LBA		Lba,
    IN  UINTN		BufferSizeInBytes,
    OUT VOID		*Buffer)
{
    EFI_STATUS Status;
    PRIVATE_BLOCK_IO_DEVICE *Private = PRIVATE_FROM_BLOCK_IO (This);
    EFI_BLOCK_IO_MEDIA *Media = &Private->Media;
    EFI_FILE *File = Private->File;
    UINT32  Offset;

    Print(L"SpReadBlocks called ...LBA:%d size:%d\n", Lba, BufferSizeInBytes);

    if ( Lba == 0 ) {
	Offset = 0;
    } else {
	Offset = (Lba * Media->BlockSize);
    }

    Status = File->SetPosition(File, Offset);

    if (EFI_ERROR(Status)) {
	Print(L"Vdisk ERROR: SetPosition returned %x\n", Status);
	return Status;
    }

    Status = File->Read(File, &BufferSizeInBytes, Buffer);

    if (EFI_ERROR(Status)) {
	Print(L"Vdisk ERROR: Read returned %x\n", Status);
	return Status;
    }

    return EFI_SUCCESS;
}


//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.WriteBlocks
//
EFI_STATUS EFIAPI VdiskWriteBlocks(
    IN  EFI_BLOCK_IO	*This,
    IN  UINT32		MediaId,
    IN  EFI_LBA		Lba,
    IN  UINTN		BufferSizeInBytes,
    IN  VOID		*Buffer)
{
    Print(L"SpWriteBlocks called\n");
    return EFI_SUCCESS;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.FlushBlocks
//
EFI_STATUS EFIAPI VdiskFlushBlocks(
    IN EFI_BLOCK_IO	*This)
{
//    Print(L"SpFlushBlocks Called\n");

    return EFI_SUCCESS;
}

// ----------------------------------

VOID *mSFSRegistration;

EFI_STATUS CheckStore(
    IN  EFI_HANDLE		SimpleFileSystemHandle,
    OUT EFI_DEVICE_PATH		**Device)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO *BlkIo;

    *Device = NULL;
    Status  = BS->HandleProtocol (
                   SimpleFileSystemHandle,
                   &BlockIoProtocol,
                   (void*)&BlkIo
                   );

    if (EFI_ERROR (Status)) {
	return Status;
    }

    if (!BlkIo->Media->MediaPresent) {
	Print(L"FwhMappedFile: Media not present!\n");
	Status = EFI_NO_MEDIA;
	return Status;
    }

    *Device = DuplicateDevicePath(DevicePathFromHandle(SimpleFileSystemHandle));

    return Status;
}

EFI_STATUS CheckStoreExists(IN EFI_DEVICE_PATH *Device)
{
    EFI_GUID simplefs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL;
    EFI_HANDLE Handle;
    EFI_FILE_IO_INTERFACE *Volume;
    EFI_STATUS Status;

    Status = BS->LocateDevicePath(&simplefs_guid, &Device, &Handle);

    if (EFI_ERROR(Status)) {
	return Status;
    }

    Status = BS->HandleProtocol(Handle, &simplefs_guid, (void **) &Volume);
    if (EFI_ERROR(Status)) {
	return Status;
    }

    return EFI_SUCCESS;
}

VOID FileClose(
    IN  EFI_FILE		*File)
{
    File->Flush(File);
    File->Close(File);
}

EFI_STATUS FileOpen(
    IN  EFI_DEVICE_PATH		*Device,
    IN  CHAR16			*MappedFile,
    OUT EFI_FILE		**File,
    IN  UINT64			OpenMode)
{
    EFI_HANDLE Handle;
    EFI_FILE_IO_INTERFACE *Volume;
    EFI_STATUS Status;
    EFI_GUID simplefs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL;
    EFI_FILE_HANDLE Root = NULL;

    *File = NULL;

    Status = BS->LocateDevicePath(&simplefs_guid, &Device, &Handle);

    if (EFI_ERROR(Status)) {
	return Status;
    }

    Status = BS->HandleProtocol(Handle, &simplefs_guid, (void **)&Volume);

    if (EFI_ERROR (Status)) {
	return Status;
    }

    Status = Volume->OpenVolume(Volume, &Root);

    Status = Root->Open(Root, File, MappedFile, OpenMode, 0);

    if (EFI_ERROR (Status)) {
	*File = NULL;
    }

    Root->Close (Root);
    return Status;
}

EFI_HANDLE	gImageHandle;

VOID EFIAPI OnSimpleFileSystemInstall (
    IN EFI_EVENT        Event,
    IN VOID             *Context)
{
    EFI_STATUS Status;
    UINTN HandleSize;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH *Device;

    while (TRUE) {
	HandleSize = sizeof (EFI_HANDLE);
	Status = BS->LocateHandle(ByRegisterNotify, NULL, mSFSRegistration,
                    &HandleSize, &Handle);
	if (Status == EFI_NOT_FOUND) {
	    break;
	}
	Print(L"Vdisk: New FileSystem Installed!\n");
	Status = CheckStore(Handle, &Device);
	if (!EFI_ERROR(Status)) {
	    Print(L"Vdisk:Store checked!\n");
	    CHAR16 *Vdisk = L"\\vdisk.vhd";

	    EFI_FILE *File;
	    Status = FileOpen(Device, Vdisk, &File, EFI_FILE_MODE_READ);
	    if (EFI_ERROR(Status)) {
		Print(L"Open file failed\n");
		continue;
	    }
	    Print(L"+++++ OPENED +++++++\n");
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
    EFI_GUID blockio_guid = BLOCK_IO_PROTOCOL;
    EFI_GUID devicepath_guid = DEVICE_PATH_PROTOCOL;
    EFI_FILE_INFO *Info = FileGetInfo(File);
    if (!Info) {
	Print(L"Can't get info for vdisk.\n");
	continue;
    }

    if (Info->FileSize == 0) {
	Print(L"Zero size vdisk.\n");
	continue;
    }

    UINTN Remainder = 0;
    UINTN BlockSize = 512;
    UINTN TotalBlock = (UINTN) DivU64x32 (Info->FileSize, BlockSize, &Remainder);

    if (Remainder != 0) {
	Print(L"Virtual disk is not aligned to %d.\n", BlockSize);
	continue;
    }

    PRIVATE_BLOCK_IO_DEVICE *Private = (PRIVATE_BLOCK_IO_DEVICE*) AllocateZeroPool (sizeof (*Private));
    Private->Device = Device;
    Private->File = File;
    Private->Media.MediaId = SIGNATURE_32('V','d','s','k');
    Private->Media.RemovableMedia = TRUE;
    Private->Media.MediaPresent = TRUE;
    Private->Media.LogicalPartition = FALSE;
    Private->Media.ReadOnly = FALSE;
    Private->Media.WriteCaching = FALSE;
    Private->Media.BlockSize = BlockSize;
    Private->Media.IoAlign = 4;
    Private->Media.LastBlock = TotalBlock - 1;
    Private->BlockIo.Revision = EFI_BLOCK_IO_INTERFACE_REVISION;
    Private->BlockIo.Media = &Private->Media;
    Private->BlockIo.Reset = VdiskReset;
    Private->BlockIo.ReadBlocks = VdiskReadBlocks;
    Private->BlockIo.WriteBlocks = VdiskWriteBlocks;
    Private->BlockIo.FlushBlocks = VdiskFlushBlocks;

    Print(L"Installing virtual disk %s\n", Vdisk);

    Status = BS->InstallMultipleProtocolInterfaces(
	&gImageHandle,
	&blockio_guid, &Private->BlockIo,
	&devicepath_guid, &gDevicePath,
	NULL);

    Print(L"Installing virtual disk .... 2 %s\n", Vdisk);

    if(EFI_ERROR(Status)) {
	if (Private != NULL) {
	    FreePool(Private);
	}
	FileClose(File);
	Print(L"BLOCK IO Failed\n");
	continue;
    }

    Print(L"Installing virtual disk %s OKAY\n", Vdisk);

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
	}
    }
}

VOID InstallSfsNotify(VOID)
{
    EFI_STATUS Status;
    EFI_EVENT  Event;

    EFI_GUID simplefs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL;

    Status = BS->CreateEvent(
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnSimpleFileSystemInstall,
                  NULL,
                  &Event
                  );

    if (EFI_ERROR(Status)) {
	Print(L"Create Event\n");
    }

    Status = BS->RegisterProtocolNotify (
                  &simplefs_guid,
                  Event,
                  &mSFSRegistration
                  );

    if (EFI_ERROR(Status)) {
	Print(L"Register protocol notify\n");
    }
}

EFI_STATUS EFIAPI VdiskDriverUninstall(EFI_HANDLE ImageHandle)
{
    return EFI_SUCCESS;
}

// ENTRY point function.

EFI_STATUS EFIAPI VdiskMain(
    IN EFI_HANDLE	ImageHandle,
    IN EFI_SYSTEM_TABLE	*SystemTable)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage = NULL;

    InitializeLib(ImageHandle, SystemTable);

    gImageHandle = ImageHandle;

////////////////////////
////////////////////////
#if 0
    EFI_GUID simplefs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL;

    CHAR16 *Vdisk = L"\\vdisk.vhd";

    Print(L"***** Virtual disk driver version XXX2\n");

    EFI_FILE_IO_INTERFACE *Volume;
    Status = BS->LocateProtocol(&simplefs_guid, NULL, (void **)&Volume);
    if (EFI_ERROR(Status)) {
	Print(L"Locate protocol failed\n");
	return Status;
    }

    EFI_FILE *Root;
    Status = Volume->OpenVolume(Volume, &Root);
    if (EFI_ERROR(Status)) {
	Print(L"Open volume failed\n");
	return Status;
    }

    EFI_FILE *File;
    Status = Root->Open(Root, &File, Vdisk, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
    if (EFI_ERROR(Status)) {
	Print(L"Open file failed\n");
	return Status;
    }

    EFI_GUID blockio_guid = BLOCK_IO_PROTOCOL;
    EFI_GUID devicepath_guid = DEVICE_PATH_PROTOCOL;
    EFI_FILE_INFO *Info = FileGetInfo(File);
    if (!Info) {
	Print(L"Can't get info for vdisk.\n");
	return Status;
    }

    if (Info->FileSize == 0) {
	Print(L"Zero size vdisk.\n");
	return Status;
    }

    UINTN Remainder = 0;
    UINTN BlockSize = 512;
    UINTN TotalBlock = (UINTN) DivU64x32 (Info->FileSize, BlockSize, &Remainder);

    if (Remainder != 0) {
	Print(L"Virtual disk is not aligned to %d.\n", BlockSize);
	return Status;
    }

    PRIVATE_BLOCK_IO_DEVICE *Private = (PRIVATE_BLOCK_IO_DEVICE*) AllocateZeroPool (sizeof (*Private));
//    Private->Device = Device;
    Private->File = File;
    Private->Media.MediaId = SIGNATURE_32('V','d','s','k');
    Private->Media.RemovableMedia = TRUE;
    Private->Media.MediaPresent = TRUE;
    Private->Media.LogicalPartition = FALSE;
    Private->Media.ReadOnly = FALSE;
    Private->Media.WriteCaching = FALSE;
    Private->Media.BlockSize = BlockSize;
    Private->Media.IoAlign = 4;
    Private->Media.LastBlock = TotalBlock - 1;
    Private->BlockIo.Revision = EFI_BLOCK_IO_INTERFACE_REVISION;
    Private->BlockIo.Media = &Private->Media;
    Private->BlockIo.Reset = VdiskReset;
    Private->BlockIo.ReadBlocks = VdiskReadBlocks;
    Private->BlockIo.WriteBlocks = VdiskWriteBlocks;
    Private->BlockIo.FlushBlocks = VdiskFlushBlocks;

    Print(L"Installing virtual disk %s\n", Vdisk);

    Status = BS->InstallMultipleProtocolInterfaces(
	&gImageHandle,
	&blockio_guid, &Private->BlockIo,
	&devicepath_guid, &gDevicePath,
	NULL);

    Print(L"Installing virtual disk .... 2 %s\n", Vdisk);

    if(EFI_ERROR(Status)) {
	if (Private != NULL) {
	    FreePool(Private);
	}
	FileClose(File);
	Print(L"BLOCK IO Failed\n");
	return Status;
    }

    Print(L"Installing virtual disk %s OKAY\n", Vdisk);
#endif
////////////////////////
////////////////////////

    /* Grab a handle to this image, so that we can add an unload to our driver */
    Status = BS->OpenProtocol(ImageHandle, &LoadedImageProtocol,
		(VOID **) &LoadedImage, ImageHandle,
		NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    Print(L"Installing virtual disk #3\n");

    if (EFI_ERROR(Status)) {
	Print(L"Could not open loaded image protocol.\n");
	return Status;
    }

    Print(L"Installing virtual disk #4\n");

    InstallSfsNotify();
    return EFI_SUCCESS;

    Print(L"Installing virtual disk #5\n");

    /* Register the uninstall callback */
    LoadedImage->Unload = VdiskDriverUninstall;

    Print(L"Installing virtual disk #6\n");

    return EFI_SUCCESS;
}

EFI_DRIVER_ENTRY_POINT(VdiskMain)
