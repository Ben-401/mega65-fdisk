/*
  Extremely simplified FDISK + FORMAT utility for the MEGA65.
  This program is designed to be compilable both for the MEGA65
  using CC65, and also for UNIX-like operating systems for testing.
  All hardware dependent features will be in fdisk_hal_mega65.c and
  fdisk_hal_unix.c, respectively. I.e., this file contains only the
  hardware independent logic.

  This program gets the size of the SD card, and then calculates an
  appropriate MBR, DOS Boot Sector, FS Information Sector, FATs and
  root directory, and puts them in place.

  XXX - We should also create the MEGA65 system partitions for
  installed services, and for task switching.

*/

#include <stdio.h>
#include <string.h>

#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "ascii.h"

#ifdef __CC65__
// Sector buffer is the physical SD card buffer on M65 to avoid copying
uint8_t *sector_buffer=(uint8_t *)0xDE00;
#else
uint8_t sector_buffer[512];
#endif

void clear_sector_buffer(void)
{
#ifndef __CC65__DONTUSE
  int i;
  for(i=0;i<512;i++) sector_buffer[i]=0;
#else
  lfill((uint32_t)sector_buffer,0,512);
#endif
}


/* Build a master boot record that has the single partition we need in
   the correct place, and with the size of the partition set correctly.
*/
void build_mbr(const uint32_t partition_sectors)
{
  clear_sector_buffer();

  // Set disk signature (fixed value)
  sector_buffer[0x1b8]=0x83;
  sector_buffer[0x1b9]=0x7d;
  sector_buffer[0x1ba]=0xcb;
  sector_buffer[0x1bb]=0xa6;

  // FAT32 Partition entry
  sector_buffer[0x1be]=0x00;  // Not bootable by DOS
  sector_buffer[0x1bf]=0x00;  // 3 bytes CHS starting point
  sector_buffer[0x1c0]=0x00;
  sector_buffer[0x1c1]=0x00;
  sector_buffer[0x1c2]=0x0c;  // Partition type (VFAT32)
  sector_buffer[0x1c3]=0x00;  // 3 bytes CHS end point - SHOULD CHANGE WITH DISK SIZE
  sector_buffer[0x1c4]=0x00;
  sector_buffer[0x1c5]=0x00;
  sector_buffer[0x1c6]=0x00;  // LBA starting sector of partition (0x0800 = sector 2,048)
  sector_buffer[0x1c7]=0x08;
  sector_buffer[0x1c8]=0x00;
  sector_buffer[0x1c9]=0x00;
  // LBA size of partition in sectors
  sector_buffer[0x1ca]=(partition_sectors>>0)&0xff;  
  sector_buffer[0x1cb]=(partition_sectors>>8)&0xff;  
  sector_buffer[0x1cc]=(partition_sectors>>16)&0xff;  
  sector_buffer[0x1cd]=(partition_sectors>>24)&0xff;  

  // MBR signature
  sector_buffer[0x1fe]=0x55;
  sector_buffer[0x1ff]=0xaa;
}


uint8_t boot_bytes[258]={
  // Jump to boot code, required by most version of DOS
  0xeb, 0x58, 0x90,
  
  // OEM String: MEGA65r1
  0x4d, 0x45, 0x47, 0x41, 0x36, 0x35, 0x72, 0x31,
  
  // BIOS Parameter block.  We patch certain
  // values in here.
  0x00, 0x02,  // Sector size = 512 bytes
  0x08 , // Sectors per cluster
  /* 0x0e */ 0x38, 0x02,  // Number of reserved sectors (0x238 = 568)
  /* 0x10 */ 0x02, // Number of FATs
  0x00, 0x00, // Max directory entries for FAT12/16 (0 for FAT32)
  /* offset 0x13 */ 0x00, 0x00, // Total logical sectors (0 for FAT32)
  0xf8, // Disk type (0xF8 = hard disk)
  0x00, 0x00, // Sectors per FAT for FAT12/16 (0 for FAT32)
  /* offset 0x18 */ 0x00, 0x00, // Sectors per track (0 for LBA only)
  0x00, 0x00, // Number of heads for CHS drives, zero for LBA
  0x00, 0x00, 0x00, 0x00, // 32-bit Number of hidden sectors before partition. Should be 0 if logical sectors == 0
  
  /* 0x20 */ 0x00, 0xe8, 0x0f, 0x00, // 32-bit total logical sectors
  /* 0x24 */ 0xf8, 0x03, 0x00, 0x00, // Sectors per FAT
  /* 0x28 */ 0x00, 0x00, // Drive description
  /* 0x2a */ 0x00, 0x00, // Version 0.0
  /* 0x2c */ 0x02, 0x00 ,0x00, 0x00, // Number of first cluster
  /* 0x30 */ 0x01, 0x00, // Logical sector of FS Information sector
  /* 0x32 */ 0x06, 0x00, // Sector number of backup-copy of boot sector
  /* 0x34 */ 0x00, 0x00, 0x00, 0x00, // Filler bytes
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, // Filler bytes
  /* 0x40 */ 0x80, // Physical drive number
  /* 0x41 */ 0x00, // FAT12/16 use only
  /* 0x42 */ 0x29, // 0x29 == Extended Boot Signature
  /* 0x43 */ 0x6d, 0x66, 0x62, 0x61, // Volume ID "mfba"
  /* 0x47 */ 0x4d, 0x2e, 0x45, 0x2e, 0x47, // 11 byte volume label
  0x2e, 0x41 ,0x2e, 0x20, 0x36, 0x35,
  /* 0x52 */ 0x46, 0x41, 0x54, 0x33, 0x32, 0x20, 0x20, 0x20, // "FAT32   "
  // Boot loader code starts here
  0x0e, 0x1f, 0xbe, 0x77 ,0x7c, 0xac,
  0x22, 0xc0, 0x74, 0x0b, 0x56, 0xb4, 0x0e, 0xbb,
  0x07, 0x00, 0xcd, 0x10, 0x5e, 0xeb ,0xf0, 0x32,
  0xe4, 0xcd, 0x16, 0xcd, 0x19, 0xeb, 0xfe,
  // From here on is the non-bootable error message
  // 0x82 - 0x69 = 
  0x4d, 0x45, 0x47, 0x41, 0x36, 0x35, 0x20, 0x4b,
  0x49, 0x43, 0x4b, 0x53, 0x54, 0x41, 0x52, 0x54,
  0x20, 0x56, 0x30, 0x30, 0x2e, 0x31, 0x31,
  0x0d, 0x0a, 0x0d, 0x3f, 0x4e, 0x4f, 0x20, 0x34,
  0x35, 0x47, 0x53, 0x30, 0x32, 0x2c, 0x20, 0x34,
  0x35, 0x31, 0x30, 0x2c, 0x20, 0x36, 0x35, 0x5b,
  0x63, 0x65, 0x5d, 0x30, 0x32, 0x2c, 0x20, 0x36,
  0x35, 0x31, 0x30, 0x20, 0x4f, 0x52, 0x20, 0x38,
  0x35, 0x31, 0x30, 0x20, 0x50, 0x52, 0x4f, 0x43,
  0x45, 0x53, 0x53, 0x4f, 0x52, 0x20, 0x20, 0x45,
  0x52, 0x52, 0x4f, 0x52, 0x0d, 0x0a, 0x49, 0x4e, 0x53,
  0x45, 0x52, 0x54, 0x20, 0x44, 0x49, 0x53, 0x4b,
  0x20, 0x49, 0x4e, 0x20, 0x52, 0x45, 0x41, 0x4c,
  0x20, 0x43, 0x4f, 0x4d, 0x50, 0x55, 0x54, 0x45,
  0x52, 0x20, 0x41, 0x4e, 0x44, 0x20, 0x54, 0x52,
  0x59, 0x20, 0x41, 0x47, 0x41, 0x49, 0x4e, 0x2e,
  0x0a, 0x0a, 0x52, 0x45, 0x41, 0x44, 0x59, 0x2e,
  0x0d, 0x0a
  
};

void build_dosbootsector(const uint8_t volume_name[11],
			 uint32_t data_sectors, uint32_t fs_sectors_per_fat)
{
  uint16_t i;
  
  clear_sector_buffer();
  
  // Start with template, and then modify relevant fields */
  for(i=0;i<sizeof(boot_bytes);i++) sector_buffer[i]=boot_bytes[i];

  // 0x20-0x23 = 32-bit number of data sectors in file system
  for(i=0;i<4;i++) sector_buffer[0x20+i]=((data_sectors)>>(i*8))&0xff;

  // 0x24-0x27 = 32-bit number of sectors per fat
  for(i=0;i<4;i++) sector_buffer[0x24+i]=((fs_sectors_per_fat)>>(i*8))&0xff;

  // 0x43-0x46 = 32-bit volume ID (random bytes)
  // 0x47-0x51 = 11 byte volume string
  
  // Boot sector signature
  sector_buffer[510]=0x55;
  sector_buffer[511]=0xaa;
  
}

void build_fs_information_sector(const uint32_t fs_clusters)
{
  uint8_t i;
  
  clear_sector_buffer();
  
  sector_buffer[0]=0x52;
  sector_buffer[1]=0x52;  
  sector_buffer[2]=0x61;
  sector_buffer[3]=0x41;
  
  sector_buffer[0x1e4]=0x72;
  sector_buffer[0x1e5]=0x72;  
  sector_buffer[0x1e6]=0x41;
  sector_buffer[0x1e7]=0x61;

  // Last free cluster = (cluster count - 1)
#ifndef __CC65__
  fprintf(stderr,"Writing fs_clusters (0x%x) as ",fs_clusters);
#endif
  for(i=0;i<4;i++) {
    // Number of free clusters
    sector_buffer[0x1e8+i]=((fs_clusters-3)>>(i*8))&0xff;
#ifndef __CC65__
    fprintf(stderr,"%02x ",sector_buffer[0x1e8+i]);
#endif
  }
#ifndef __CC65__
  fprintf(stderr,"\n");
#endif
  
  // First free cluster = 2
  sector_buffer[0x1ec]=0x02+1;  // OSX newfs/fsck puts 3 here instead?

  // Boot sector signature
  sector_buffer[510]=0x55;
  sector_buffer[511]=0xaa;
}

uint8_t fat_bytes[12]={0xf8,0xff,0xff,0x0f,0xff,0xff,0xff,0x0f,0xf8,0xff,0xff,0x0f};

void build_empty_fat()
{
  int i;
  clear_sector_buffer();
  for(i=0;i<12;i++) sector_buffer[i]=fat_bytes[i];  
}

uint8_t dir_bytes[15]={8,0,0,0x53,0xae,0x93,0x4a,0x93,0x4a,0,0,0x53,0xae,0x93,0x4a};

void build_root_dir(const uint8_t volume_name[11])
{
  int i;
  clear_sector_buffer();
  for(i=0;i<11;i++) sector_buffer[i]=volume_name[i];
  for(i=0;i<15;i++) sector_buffer[11+i]=dir_bytes[i];
}

uint32_t sdcard_sectors;

uint32_t partition_sectors;

// Calculate clusters for file system, and FAT size
uint32_t fs_clusters=0;
uint32_t reserved_sectors=568; // not sure why we use this value
uint32_t rootdir_sector=0;
uint32_t fat_sectors=0;
uint32_t fat1_sector=0;
uint32_t fat2_sector=0;
uint32_t fs_data_sectors=0;
uint8_t sectors_per_cluster=8;  // 4KB clusters
uint8_t volume_name[11]="M.E.G.A.65!";

// Work out maximum number of clusters we can accommodate
uint32_t sectors_required;
uint32_t available_sectors;


#ifdef __CC65__
void main(void)
#else
int main(int argc,char **argv)
#endif
{
#ifdef __CC65__
  mega65_fast();
  setup_screen();
#endif  
  
  sdcard_open();

  // Memory map the SD card sector buffer on MEGA65
  sdcard_map_sector_buffer();
  
  sdcard_sectors = sdcard_getsize();

  // Calculate sectors for partition
  // This is the size of the card, minus 2,048 (=0x0800) sectors
  partition_sectors=sdcard_sectors-0x0800;

  available_sectors=partition_sectors-reserved_sectors;

#ifndef __CC65__
  fprintf(stderr,"PARTITION HAS $%x SECTORS ($%x AVAILABLE)\r\n",
	  partition_sectors,available_sectors);
#else
  // Tell use how many sectors available for partition
  write_line("$         Sectorsr available for partition.",0);
  screen_hex(screen_line_address-79,partition_sectors);
#endif
  
  fs_clusters=available_sectors/(sectors_per_cluster);
  fat_sectors=fs_clusters/(512/4); if (fs_clusters%(512/4)) fat_sectors++;
  sectors_required=2*fat_sectors+((fs_clusters-2)*sectors_per_cluster);
  while(sectors_required>available_sectors) {
    uint32_t excess_sectors=sectors_required-available_sectors;
    uint32_t delta=(excess_sectors/(1+sectors_per_cluster));
    if (delta<1) delta=1;
#ifndef __CC65__
    fprintf(stderr,"%d clusters would take %d too many sectors.\r\n",
	    fs_clusters,sectors_required-available_sectors);
#endif
    fs_clusters-=delta;
    fat_sectors=fs_clusters/(512/4); if (fs_clusters%(512/4)) fat_sectors++;
    sectors_required=2*fat_sectors+((fs_clusters-2)*sectors_per_cluster);
  }
#ifndef __CC65__
  fprintf(stderr,"Creating File System with %u (0x%x) CLUSTERS, %d SECTORS PER FAT, %d RESERVED SECTORS.\r\n",
	  fs_clusters,fs_clusters,fat_sectors,reserved_sectors);
#else
  write_line("Format SD Card with new partition table and FAT32 file fystem?",0);
  write_line("  $         Clusters,       Sectors/FAT,       Reserved Sectors.",0);
  screen_hex(screen_line_address-80+3,fs_clusters);
  screen_decimal(screen_line_address-80+22,fat_sectors);
  screen_decimal(screen_line_address-80+41,reserved_sectors);
  while(1)
  {
    char line_of_input[80];
    unsigned char len;
    write_line(" ",0);
    write_line("Type DELETE EVERYTHING to continue:",0);
    recolour_last_line(2);
    len=read_line(line_of_input,80);
    if (len) {
      write_line(line_of_input,3);
      recolour_last_line(7);
    }
    if (strcmp("DELETE EVERYTHING",line_of_input)) {
      write_line("Entered text does not match. Try again.",0);
      recolour_last_line(8);
    } else
      // String matches -- so proceed
      break;
  }
#endif

  
  
  fat1_sector=0x0800+reserved_sectors;
  fat2_sector=fat1_sector+fat_sectors;
  rootdir_sector=fat2_sector+fat_sectors;
  fs_data_sectors=fs_clusters*sectors_per_cluster;
  
  // MBR is always the first sector of a disk
#ifdef __CC65__
  write_line(" ",0);
  write_line("Writing Partition Table / Master Boot Record...",0);
#endif
  build_mbr(partition_sectors);
  sdcard_writesector(0);

#ifdef __CC65__
  write_line("Erasing reserved sectors before partition...",0);
#endif
  // Blank intervening sectors
  sdcard_erase(0+1,0x0800-1);
  
#ifdef __CC65__
  write_line("Writing FAT Boot Sector...",0);
#endif
  // Partition starts at fixed position of sector 2048, i.e., 1MB
  build_dosbootsector(volume_name,
		      partition_sectors,
		      fat_sectors);
  sdcard_writesector(0x0800);
  sdcard_writesector(0x0806); // Backup boot sector at partition + 6

#ifdef __CC65__
  write_line("Writing FAT Information Block (and backup copy)...",0);
#endif
  // FAT32 FS Information block (and backup)
  build_fs_information_sector(fs_clusters);
  sdcard_writesector(0x0801);
  sdcard_writesector(0x0807);

  // FATs
#ifndef __CC65__
  fprintf(stderr,"Writing FATs at offsets 0x%x AND 0x%x\r\n",
	  fat1_sector*512,fat2_sector*512);
#else
  write_line("Writing FATs at $         and $         ...",0);
  screen_hex(screen_line_address-80+17,fat1_sector*512);
  screen_hex(screen_line_address-80+31,fat2_sector*512);
#endif
  build_empty_fat(); 
  sdcard_writesector(fat1_sector);
  sdcard_writesector(fat2_sector);

#ifdef __CC65__
  write_line("Writing Root Directory...",0);
#endif
  // Root directory
  build_root_dir(volume_name);
  sdcard_writesector(rootdir_sector);

#ifdef __CC65__
  write_line(" ",0);
  write_line("Clearing file system data structures...",0);
#endif
  // Make sure all other sectors are empty
  sdcard_erase(0x0801+1,0x0806-1);
  sdcard_erase(0x0806+1,fat1_sector-1);
  sdcard_erase(fat1_sector+1,fat2_sector-1);
  sdcard_erase(fat2_sector+1,rootdir_sector-1);
  sdcard_erase(rootdir_sector+1,rootdir_sector+1+sectors_per_cluster-1);

#ifdef __CC65__
  POKE(0xd021U,6);
  write_line(" ",0);
  write_line("SD Card has been formatted.  Remove, Copy MEGA65.ROM, Reinsert AND Reboot.",0);
  while(1) continue;
#else
  return 0;
#endif
  
}
