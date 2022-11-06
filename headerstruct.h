#pragma once

#pragma pack (1)
typedef struct _Header_Main
{
	uint32_t			ulSig;
	uint32_t			ulVersion;

	uint64_t			ullContainerDiskSize;
	uint64_t			ullSecCtxOffset;
	uint64_t			ullUserCtxOffset;

	uint32_t			ulFlags;
	uint32_t			ulFlagsEx;

	uint32_t			ulReservedBlocks;

	uint32_t			_pad1;
	uint32_t			ulStatusFlags;

unsigned char			bReserved;		

	uint64_t			ullLastMountTime;		

	uint64_t			ulDiskStartOffset;		// Actual offset to disk data

	uint32_t			ulNumParts;
	uint32_t			ulPartsSize;
unsigned char			bTieUserIV[32];			

	unsigned char	_pad2[403];				// pad to 512 byte boundary

}HEADER_MAIN, * PHEADER_MAIN;
#pragma pack (4)
