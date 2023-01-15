// Exfatformatter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include "FsStructs.h"
#include "headerstruct.h"
#include "exfatstuff.h"


const uint32_t BOOT_BACKUP_OFFSET = 12;
const uint16_t BYTES_PER_SECTOR = 512;
const uint16_t SECTOR_MASK = BYTES_PER_SECTOR - 1;
const uint32_t BITMAP_CLUSTER = 2;
const uint32_t NUMBER_OF_FATS = 1;



unsigned long ExFatFormatter::xsum32(	/* Returns 32-bit checksum */
    unsigned char dat,			/* Byte to be calculated (byte-by-byte processing) */
    unsigned long sum			/* Previous sum value */
)
{
    sum = ((sum & 1) ? 0x80000000 : 0) + (sum >> 1) + dat;
    return sum;
}




bool ExFatFormatter::writeUpcase(uint32_t sector) {

    memset(m_secBuf, 0, BYTES_PER_SECTOR);
    
    bool success = true;
							/* Table checksum to be stored in the 82 entry */
    m_upcaseSize = 0;
   
    m_upcaseChecksum = 0;
    
    uint32_t bufferIter = 0;
    
    uint32_t upcaseIter = 0;

    while (m_upcaseSize<sizeof(microUpcase)) {

        m_upcaseChecksum = xsum32(m_secBuf[bufferIter] = (unsigned char)microUpcase[upcaseIter], m_upcaseChecksum); // Put it into the write buffer and also calculate the checksum of the current byte (using the previous checksum)
       
        bufferIter++; m_upcaseSize++; upcaseIter++;

        if (bufferIter == BYTES_PER_SECTOR) {		/* Write buffered data when buffer full or end of process */
           
            if (!writeSector(sector)) success = false;
            
            sector++; bufferIter = 0; // Increment our working sector and reset our buffer incrementer
            
            memset(m_secBuf, 0, BYTES_PER_SECTOR); // Reset our buffer each time to flush it...important for last step. 
        } 

    
    }

    if (bufferIter != 0) {

        if (!writeSector(sector)) success = false;
        memset(m_secBuf, 0, BYTES_PER_SECTOR);
    
    }

     return success;

}

int main() {

    HEADER_MAIN header;

    char stringBuffer[100];

    strcpy_s(stringBuffer, _countof(stringBuffer), R"(\\EXAMPLE\\PATH\\TO\\VOLUME)");

    FILE* f;
    errno_t error;
   
    
    if (!(error = fopen_s(&f, stringBuffer, "rb+")))
    {
        
        fread(&header, sizeof(HEADER_MAIN), 1, f);

        ExFatFormatter formatObj;

        formatObj.format(f, &header);
   
    }

}

bool ExFatFormatter::writeSector(uint32_t sector) {

    uint64_t offset = ((uint64_t)sector * BYTES_PER_SECTOR) + m_headerOffset;

    if (_fseeki64(m_dev, offset, SEEK_SET) == 0) {

        fwrite(m_secBuf, BYTES_PER_SECTOR, 1, m_dev);

        return true;
    }
        
    return false;
 
}

int FastLog2(int input) {

    int res;
    int halver;

    for (res = 0, halver = input; halver >>= 1; res++);

    return res;

}


bool ExFatFormatter::format(FILE* dev, PHEADER_MAIN pHeader) {

    ExFatPbs_t* pbs;
    DirUpcase_t* dup;
    DirBitmap_t* dbm;
    DirLabel_t* label;
    uint64_t bitmapSize;
    uint64_t checksum = 0;
    uint64_t clusterCount;
    uint64_t clusterHeapOffset;
    uint64_t fatLength;
    uint64_t fatOffset;
    uint64_t ns;
    uint64_t partitionOffset;
    uint64_t sector;
    uint32_t sectorsPerCluster = 0;
    uint64_t sectorCount;
    uint8_t sectorsPerClusterShift;
    uint64_t n;
    uint32_t nbit;
    uint8_t clen[3];
    uint32_t clu;
    uint32_t j;

    m_dev = dev;
    
    m_headerOffset = pHeader->ulDiskStartOffset;

    m_bytesPerSectorShift = FastLog2(BYTES_PER_SECTOR);
    
    sectorCount = pHeader->ullContainerDiskSize >> m_bytesPerSectorShift;
 
    if (sectorCount < 0X4000) {
        goto fail;
    }    

    if (sectorsPerCluster == 0)
    {
        // forced to use higher sectorsPerCluster since upcasetable is always 5836
        sectorsPerCluster = 8;
        if (sectorCount >= 0x7A120) sectorsPerCluster = 64;       /* >= 250MBs */
        if (sectorCount >= 0x3B9ACA0) sectorsPerCluster = 256;    /* >= 32GBs */
    }
 
    //sectorsPerClusterShift = OptimalClusterSize(sectorCount);
    sectorsPerClusterShift = FastLog2(sectorsPerCluster);

    //sectorsPerCluster = 1UL << sectorsPerClusterShift;
   
    fatLength = (((sectorCount >> sectorsPerClusterShift) + 2) * 4  + SECTOR_MASK) >> m_bytesPerSectorShift; // == / BYTES_PER_SECTOR

    fatOffset = 32;
   
    partitionOffset = 0;
    
    //Nice rounded number.
    clusterHeapOffset = ((unsigned __int64)fatOffset + fatLength + SECTOR_MASK) & ~((unsigned __int64)(SECTOR_MASK));

    // bitmap size is cluster count / 4096
    clusterCount = (sectorCount - clusterHeapOffset) >> sectorsPerClusterShift; //sector count / sectors per cluster
    

    bitmapSize = (clusterCount + 7) >> 3; // clustercount / 8

 
    clen[0] = (bitmapSize + sectorsPerCluster * SECTOR_MASK) / (sectorsPerCluster * BYTES_PER_SECTOR); // Amount of clusters bitmap will take up

    // We write the Upcase table first simply because we need to reference it's size in clusters early on... it's how elm-chan's does it. :P
    if (!writeUpcase(partitionOffset + clusterHeapOffset + sectorsPerCluster * clen[0])) {
        goto fail;
    }

    clen[1] = (m_upcaseSize + sectorsPerCluster * SECTOR_MASK) / (sectorsPerCluster * BYTES_PER_SECTOR);	/* Number of up-case table clusters */
    
    clen[2] = 1;


    // Partition Boot sector.
    memset(m_secBuf, 0, BYTES_PER_SECTOR);

    pbs = reinterpret_cast<ExFatPbs_t*>(m_secBuf);
    pbs->jmpInstruction[0] = 0XEB;
    pbs->jmpInstruction[1] = 0X76;
    pbs->jmpInstruction[2] = 0X90;
    pbs->oemName[0] = 'E';
    pbs->oemName[1] = 'X';
    pbs->oemName[2] = 'F';
    pbs->oemName[3] = 'A';
    pbs->oemName[4] = 'T';
    pbs->oemName[5] = ' ';
    pbs->oemName[6] = ' ';
    pbs->oemName[7] = ' ';
    setLe64(pbs->bpb.partitionOffset, m_headerOffset);
    setLe64(pbs->bpb.volumeLength, sectorCount);
    setLe32(pbs->bpb.fatOffset, fatOffset);
    setLe32(pbs->bpb.fatLength, fatLength);
    setLe32(pbs->bpb.clusterHeapOffset, clusterHeapOffset);
    setLe32(pbs->bpb.clusterCount, clusterCount);
    setLe32(pbs->bpb.rootDirectoryCluster, BITMAP_CLUSTER + clen[0] + clen[1]);
    setLe32(pbs->bpb.volumeSerialNumber, sectorCount); // restricted to 4 bytes for this value, ignore truncation warning
    setLe16(pbs->bpb.fileSystemRevision, 0X100);
    setLe16(pbs->bpb.volumeFlags, 0);
    pbs->bpb.bytesPerSectorShift = m_bytesPerSectorShift;
    pbs->bpb.sectorsPerClusterShift = sectorsPerClusterShift;
    pbs->bpb.numberOfFats = NUMBER_OF_FATS;
    pbs->bpb.driveSelect = 0X80;
    pbs->bpb.percentInUse = 0;

    setLe16(pbs->signature, PBR_SIGNATURE);
    
    for (unsigned long long i = 0; i < BYTES_PER_SECTOR; i++) {
        if (i == offsetof(ExFatPbs_t, bpb.volumeFlags[0]) ||
            i == offsetof(ExFatPbs_t, bpb.volumeFlags[1]) ||
            i == offsetof(ExFatPbs_t, bpb.percentInUse)) {
            continue;
        }
        checksum = exFatChecksum(checksum, m_secBuf[i]);
    }
    
    sector = partitionOffset;
    
    if (!writeSector(sector) ||
        !writeSector((sector + BOOT_BACKUP_OFFSET))) {
        goto fail;
    }
    
    sector++;
    // Write eight Extended Boot Sectors.
    memset(m_secBuf, 0, BYTES_PER_SECTOR);
    setLe16(pbs->signature, PBR_SIGNATURE);
    for (j = 0; j < 8; j++) {
        for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
            checksum = exFatChecksum(checksum, m_secBuf[i]);
        }
        if (!writeSector(sector) ||
            !writeSector(sector + BOOT_BACKUP_OFFSET)) {
            goto fail;
        }
        
        sector++;
    }
    // Write OEM Parameter Sector and reserved sector.
    memset(m_secBuf, 0, BYTES_PER_SECTOR);
    
    for (j = 0; j < 2; j++) {
        for (unsigned long long i = 0; i < BYTES_PER_SECTOR; i++) {
            checksum = exFatChecksum(checksum, m_secBuf[i]);
        }
        if (!writeSector(sector) ||
            !writeSector(sector + BOOT_BACKUP_OFFSET)) {
            goto fail;
        }
        
        sector++;
    }
    // Write Boot CheckSum Sector.
    for (unsigned long long i = 0; i < BYTES_PER_SECTOR; i += 4) {
        setLe32(m_secBuf + i, checksum);
    }
    if (!writeSector(sector) ||
        !writeSector(sector + BOOT_BACKUP_OFFSET)){
        goto fail;
    }

    // Initialize FAT.
    sector = partitionOffset + fatOffset;
    
    ns = fatLength;

    j = nbit = clu = 0;

    do {
        memset(m_secBuf, 0, BYTES_PER_SECTOR); int i = 0;	/* Clear work area and reset write offset */
        if (clu == 0) {	/* Initialize FAT [0] and FAT[1] */
            setLe32(m_secBuf + i, 0xFFFFFFF8); i += 4; clu++;
            setLe32(m_secBuf + i, 0xFFFFFFFF); i += 4; clu++;
        }
        do {			/* Create chains of bitmap, up-case and root dir */
            while (nbit != 0 && i < BYTES_PER_SECTOR) {	/* Create a chain */
                setLe32(m_secBuf + i, (nbit > 1) ? clu + 1 : 0xFFFFFFFF);
                i += 4; clu++; nbit--;
            }
            if (nbit == 0 && j < 3) nbit = clen[j++];	/* Get next chain length */
        } while (nbit != 0 && i < BYTES_PER_SECTOR);
        
        n = (ns > BYTES_PER_SECTOR) ? BYTES_PER_SECTOR : ns;	/* Write the buffered data */

         if (!writeSector(sector)) goto fail;
     
        sector += n; ns -= n;

    } while (ns);

    // Write bitmap.
    sector = partitionOffset + clusterHeapOffset;

    ns = (bitmapSize) >> m_bytesPerSectorShift;
   
    nbit = clen[0] + clen[1] + clen[2];

   // Initialize bitmap buffer 
   memset(m_secBuf, 0, BYTES_PER_SECTOR);
      
   for (uint32_t i = 0; nbit != 0 && i >> 3 < BYTES_PER_SECTOR; m_secBuf[i >> 3] |= 1 << (i % 8), i++, nbit--);	/* Mark used clusters */
    
    // Allocate clusters for bitmap, upcase, and root.
   for (uint32_t i = 0; i < ns; i++) {
      if (!writeSector(sector + i)) {
          goto fail;
      }
      if (i == 0) {
         m_secBuf[0] = 0;
      }
   }

    // Initialize first sector of root.
    ns = sectorsPerCluster;
    
    sector = partitionOffset + clusterHeapOffset + ((clen[0] + clen[1]) * sectorsPerCluster);
    
    memset(m_secBuf, 0, BYTES_PER_SECTOR);

    // Unused Label entry.
    label = reinterpret_cast<DirLabel_t*>(m_secBuf);
    label->type = EXFAT_TYPE_LABEL & 0X7F;

    // bitmap directory entry.
    dbm = reinterpret_cast<DirBitmap_t*>(m_secBuf + 32);
    dbm->type = EXFAT_TYPE_BITMAP;
    setLe32(dbm->firstCluster, BITMAP_CLUSTER);
    setLe64(dbm->size, bitmapSize);

    // upcase directory entry.
    dup = reinterpret_cast<DirUpcase_t*>(m_secBuf + 64);
    dup->type = EXFAT_TYPE_UPCASE;
    setLe32(dup->checksum, m_upcaseChecksum);
    setLe32(dup->firstCluster, BITMAP_CLUSTER + clen[0]);
    setLe64(dup->size, m_upcaseSize);

    // Write root, cluster four.
    for (uint32_t i = 0; i < ns; i++) {
      if (!writeSector(sector + i)) {
          goto fail;
      }
      if (i == 0) {
          memset(m_secBuf, 0, BYTES_PER_SECTOR);
      }
    }
    wprintf(L"Format finished");
    return true;

fail:
    wprintf(L"Format failed");
    return false;
}



// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
