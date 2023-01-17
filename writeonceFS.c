#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
// Maximum size of the file_system 4MB
#define DISK_SIZE 4 * 1024 * 1024
// Block size 1KB
#define BLOCK_SIZE 1024
// Maximum number of files that can be stored
#define MAX_FILES 100
// File modes
#define WO_RDONLY 1
#define WO_WRONLY 2
#define WO_RDWR 3
// File creation flags
#define WO_CREAT 1
 
// Structure for superblock
typedef struct
{
  int size;        	// Size of the file_system in number of blocks
  int free_blocks; 	// Number of free blocks
  int used_blocks; 	// Number of used blocks
  int last_used_block; // Last Accessed Block Index in data region
  char format[9];  	// To check if disk is in the right format or not
} superblock_t;
// Structure for inode
typedef struct inode_t
{
  char name[256];         	// Filename
  int blocks;             	// Number of blocks used by the file
  int fd;                 	// File Descriptor
  int last_updated_block; 	// Last Updated Node Index
  short int file_block[4096]; // file block
  struct inode_t *next;   	// next inode's pointer
} inode_t;
// Structure for open file table
typedef struct open_file
{
  int mode;   	// File access mode
  inode_t *inode; // Pointer to the inode
  int cursor; 	// Current file cursor
} open_file;
// disk name
char *disk_Name;
// Pointer to the file system in memory
char *file_system;
// Pointer to the data region in memory
char *data_region;
// Pointer to the inode region in memory
inode_t *inode_region;
// Pointer to the superblock in memory
superblock_t *superblock_region;
// Open file table
open_file open_file_table[MAX_FILES];
// Mounts the file system from the given file
int wo_mount(char *filename, void *memory_address)
{
//   printf("Mounting");
  // Open the file to read
  disk_Name = filename;
  FILE *fp = fopen(disk_Name, "rb");
  if (fp == NULL)
  {
  	// File access error
  	return errno;
  }
  // Check the size of the file
  fseek(fp, 0, SEEK_END);
  int file_size = ftell(fp);
  if (file_size != DISK_SIZE)
  {
  	// Invalid file_system size
  	fclose(fp);
  	errno = ENOTDIR;
  	return errno;
  }
  // Read the entire file system into memory
  fseek(fp, 0, SEEK_SET);
  fread(memory_address, DISK_SIZE, 1, fp);
  fclose(fp);
  // Set up pointers to the different regions in memory
  file_system = (char *)memory_address;
  superblock_region = (superblock_t *)memory_address;
  inode_region = (inode_t *)(memory_address + sizeof(superblock_t) + 1);
  inode_t *inode_traverse = inode_region;
  for (int i = 1; i < MAX_FILES; i++)
  {
  	inode_traverse->next = (inode_t *)(memory_address + sizeof(superblock_t) + 1 + sizeof(inode_t) * i);
  	inode_traverse = inode_traverse->next;
  }
  data_region = (char *)(memory_address + sizeof(superblock_t) + sizeof(inode_t) * MAX_FILES + 1);
  if (strcmp(superblock_region->format, "format") != 0)
  {
  	superblock_region->free_blocks = ((DISK_SIZE - (sizeof(superblock_t) + sizeof(inode_t) * MAX_FILES))) / BLOCK_SIZE;
  	superblock_region->used_blocks = 0;
  	superblock_region->size = DISK_SIZE;
  	superblock_region->last_used_block = -1;
  	strcpy(superblock_region->format, "format");
  }
  return 0;
}
// Unmounts the file system from the given file
int wo_unmount(void *memory_address)
{
//   printf("Unmounting");
  // Open the file for writing
  FILE *fp = fopen(disk_Name, "wb");
  if (fp == NULL)
  {
  	// File access error
  	return errno;
  }
  // Write the entire file_system from memory to the file
  fwrite(memory_address, DISK_SIZE, 1, fp);
  fclose(fp);
  return 0;
}
int wo_create(char *filename, int flags)
{
  int inode_index = -1;
  inode_t *inode_traverse = (inode_t *)inode_region;
  int fd = -1;
  if (strlen(filename) > 256)
  {
  	errno = ENAMETOOLONG;
  	return errno;
  }
  while (inode_traverse->name[0] != '\0')
  {
  	if (strcmp(inode_traverse->name, filename) == 0)
  	{
      	// File already present in system
      	errno = EEXIST;
      	return errno;
  	}
  	fd += 1;
  	inode_traverse = inode_traverse->next;
  }
  strcpy(inode_traverse->name, filename);
  inode_traverse->blocks = 0;
  for (int i = 0; i < 4096; i++)
  {
  	inode_traverse->file_block[i] = -1;
  }
  inode_traverse->fd = fd;
  // Open the file
  for (int i = 0; i < MAX_FILES; i++)
  {
  	if (open_file_table[i].inode == NULL)
  	{
      	// Find the first empty slot in the file descriptor table
      	open_file *fd = &open_file_table[i];
      	fd->mode = flags;
      	fd->inode = inode_traverse;
      	fd->cursor = inode_traverse->file_block[0];
      	// printf("\n i = %d", i);
      	return i;
  	}
  }
  errno = EMFILE;
  return errno;
}
// Opens the file with file descriptor, flags and mode
int wo_open(char *filename, int flags)
{
//   printf("Opening");
  int inode_index = -1;
  inode_t *inode_traverse = (inode_t *)inode_region;
  int fd = -1;
  while (inode_traverse->name[0] != '\0')
  {
  	if (strcmp(inode_traverse->name, filename) == 0)
  	{
      	// File Found
      	break;
  	}
  	fd += 1;
  	inode_traverse = inode_traverse->next;
  }
  if (inode_traverse->name[0] == '\0')
  {
  	errno = ENOENT;
  	return errno;
  }
  // Open the file
  for (int i = 0; i < MAX_FILES; i++)
  {
  	if (open_file_table[i].inode == NULL)
  	{
      	// Find the first empty slot in the file descriptor table
      	open_file *fd = &open_file_table[i];
      	fd->mode = flags;
      	fd->inode = inode_traverse;
      	fd->cursor = inode_traverse->file_block[0];
      	return i;
  	}
  }
  errno = EMFILE;
  return errno;
}
// Reads the bytes from the file with file descriptor into the buffer
int wo_read(int fd, void *buffer, int bytes)
{
//   printf("Reading");
  // Check if the file descriptor is valid
  if (fd < 0 || fd >= MAX_FILES || open_file_table[fd].inode == NULL)
  {
  	errno = EBADF;
  	return errno;
  }
  open_file *file = &open_file_table[fd];
  inode_t *inode = file->inode;
  // Check if the file is open in read mode
  if (file->mode == WO_WRONLY)
  {
  	// File is not open in read mode
  	errno = EACCES;
  	return errno;
  }
  // Calculate the number of blocks to read
  int block_count = (bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
  // Check if the file has enough data
  if (block_count > inode->blocks)
  {
  	block_count = inode->blocks;
  }
  // Read the data
  int bytes_read = 0;
  for (int i = 0; i < block_count; i++)
  {
  	int block_index = file->cursor;
  	char *src = data_region + block_index * BLOCK_SIZE;
  	memcpy(buffer + bytes_read, src, BLOCK_SIZE);
  	bytes_read += BLOCK_SIZE;
  	if (inode->file_block[block_index + 1] != -1)
  	{
      	file->cursor = block_index + 1;
  	}
  }
  return bytes_read;
}
// Writes the bytes from the file with file descriptor into the buffer
int wo_write(int fd, void *buffer, int bytes)
{
//   printf("Writing");
  // Checking if the file descriptor is valid
  if (fd < 0 || fd >= MAX_FILES || open_file_table[fd].inode == NULL)
  {
  	// Invalid file descriptor
  	errno = EBADF;
  	return errno;
  }
  open_file *file = &open_file_table[fd];
  inode_t *inode = file->inode;
  // Check if the file is open in write mode
  if (file->mode != WO_WRONLY && file->mode != WO_RDWR)
  {
  	// File is not open in write mode
  	errno = EROFS;
  	return errno;
  }
  // Calculate the number of blocks to write
  int block_count = (bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
  // Check if there is enough free space in the filesystem
  if (block_count > superblock_region->free_blocks)
  {
  	// Not enough free space
  	errno = EFBIG;
  	return errno;
  }
  // Find the first available block
  inode_t *inode_traverse = (inode_t *)inode_region;
  int block_index = superblock_region->last_used_block;
  block_index += 1;
  // Write the data into the block
  int bytes_written = 0;
  for (int i = 0; i < block_count; i++)
  {
  	if (inode_traverse->file_block[0] == -1)
  	{
      	file->cursor = block_index;
      	inode_traverse->file_block[0] = block_index;
      	inode_traverse->last_updated_block = 0;
  	}
  	else
  	{
      	file->cursor = block_index;
      	inode_traverse->last_updated_block += 1;
      	inode_traverse->file_block[inode_traverse->last_updated_block] = block_index;
  	}
  	superblock_region->free_blocks -= 1;
  	superblock_region->used_blocks += 1;
  	superblock_region->last_used_block = block_index;
  	char *dest = data_region + block_index * BLOCK_SIZE;
  	memcpy(dest, buffer + bytes_written, BLOCK_SIZE);
  	bytes_written += BLOCK_SIZE;
  	block_index += 1;
  }
  // Update the block count
  inode_traverse->blocks += block_count;
  return bytes_written;
}
// Closes the file and removes from the open file table.
int wo_close(int fd)
{
//   printf("Closing");
  // Check if the file descriptor is valid
  if (fd < 0 || fd >= MAX_FILES || open_file_table[fd].inode == NULL)
  {
  	// Invalid file descriptor
  	errno = EBADF;
  	return errno;
  }
  // Close the file
  open_file_table[fd].inode = NULL;
  return 0;
}
// To print the string
void display_string(char *str, int size)
{
  for (int i = 0; i < size; i++)
  	printf("%c", *str++);
}
 

 
 
 

