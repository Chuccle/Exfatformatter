#pragma once

#define ULONG uint32_t
#define ULONGLONG uint64_t
#define UCHAR unsigned char

#pragma pack (1)
typedef struct _tDLPVDiskHeader_Main
{
	ULONG			ulSig;
	ULONG			ulVersion;

	ULONGLONG		ullContainerDiskSize;
	ULONGLONG		ullSecCtxOffset;
	ULONGLONG		ullUserCtxOffset;

	ULONG			ulFlags;
	ULONG			ulFlagsEx;

	ULONG			ulReservedBlocks;

	ULONG			_pad1;
	ULONG			ulStatusFlags;

	UCHAR			bReservedForJames;		// flags to set if created with trial or expired licence

	ULONGLONG		ullLastMountTime;		// Last time the vd was mounted

	ULONGLONG		ulDiskStartOffset;		// Actual offset to disk data

	ULONG			ulNumParts;
	ULONG			ulPartsSize;
	UCHAR			bTieUserIV[32];			// only used if a users are tied to the drive

	UCHAR			_pad2[403];				// pad to 512 byte boundary

}DLPVDISK_HEADER_MAIN, * PDLPVDISK_HEADER_MAIN;
#pragma pack (4)
