#include<stdint.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

#define MAX_FILE_SIZE 1029*BLOCKSIZE
const static uint32_t MAGIC = 12345;

typedef struct inode {
	uint32_t valid; // 0 if invalid
	uint32_t size; // logical size of the file
	uint32_t direct[5]; // direct data block pointer
	uint32_t indirect; // indirect pointer
} inode;


typedef struct super_block {
	uint32_t magic_number;	// File system magic number
	uint32_t blocks;	// Number of blocks in file system (except super block)

	uint32_t inode_blocks;	// Number of blocks reserved for inodes == 10% of Blocks
	uint32_t inodes;	// Number of inodes in file system == length of inode bit map
	uint32_t inode_bitmap_block_idx;  // Block Number of the first inode bit map block
	uint32_t inode_block_idx;	// Block Number of the first inode block

	uint32_t data_block_bitmap_idx;	// Block number of the first data bitmap block
	uint32_t data_block_idx;	// Block number of the first data block
	uint32_t data_blocks;  // Number of blocks reserved as data blocks
} super_block;


typedef struct lst_node {
	int valid;
	int type;
	char* name;
	int length;
	int inumber;
} lst_node;


int format(disk *diskptr);

int mount(disk *diskptr);

int create_file();

int remove_file(int inumber);

int stat(int inumber);

int read_i(int inumber, char *data, int length, int offset);

int write_i(int inumber, char *data, int length, int offset);

int reset_bitmap(int index, bool inode); // To reset the respective bitmap
int get_data_block(); // to get an empty data block
int min(int x,int y);
int max(int x,int y);

// given a file name and its parent directory inode, search if it exists. add - if not, add the file
// dir - the file is a directory or a data file
int find_inumber(char* file_name, int inode_num, bool add, bool dir); 

int read_file(char *filepath, char *data, int length, int offset);
int write_file(char *filepath, char *data, int length, int offset);
int create_dir(char *dirpath);
int remove_dir(char *dirpath);
