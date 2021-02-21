// File System Implementation

// ------------------- PART B  ------------------

#include "disk.h"
#include "sfs.h"

disk *state = NULL; // Store the disk_pointer after mount()
int root_inumber; // Inode number of root file directory


int format(disk *diskptr) { 

	if (diskptr == NULL) {
		printf("Error in diskptr argument\n");
		return -1;
	}

	// Initialize the super block

	super_block *SB = (super_block*)malloc(sizeof(super_block));

	int N = diskptr->blocks;
	if (N < 1) {
		return -1;
	} 
	int M = N-1;
	int I = M / 10;

	double diff;
	diff = ((double)(I*128) / (8*4096)) - (int)((I*128) / (8*4096));
	int IB = (I*128) / (8*4096);
	if (diff > 0) IB++;

	int R = M - I - IB;

	diff = ((double)(R) / (8*4096)) - (int)((R) / (8*4096));
	int DBB = R / (8*4096);
	if (diff > 0) DBB++;

	int DB = R - DBB;

	SB->magic_number = MAGIC;
	SB->blocks = M;
	SB->inode_blocks = I;
	SB->inodes = I * 128;
	SB->inode_bitmap_block_idx = 1;
	SB->inode_block_idx = 1 + IB + DBB;
	SB->data_block_bitmap_idx = 1 + IB;
	SB->data_block_idx = 1 + IB + DBB + I;
	SB->data_blocks = DB;

	// write super block at the 1st block with index 0;

	if (write_block(diskptr, 0, SB) != 0) {
		return -1;
	}

	// Initialize all bit_maps to 0, both inode and data bitmaps

	char *bit_init = (char *)malloc(BLOCKSIZE);
	memset(bit_init, 0, BLOCKSIZE);

	for (int i = SB->inode_bitmap_block_idx; i < SB->data_block_bitmap_idx; i++) {
		if (write_block(diskptr, i, bit_init) != 0) {
			return -1;
		}
	}

	for (int i = SB->data_block_bitmap_idx; i < SB->inode_block_idx; i++) {
		if (write_block(diskptr, i, bit_init) != 0) {
			return -1;
		}
	}

	// Initialize inodes with valid = 0;

	inode *inode_ptr = (inode*)malloc(sizeof(inode));
	memset(inode_ptr, 0, sizeof(inode));
	inode_ptr->valid = 0;

	char *inode_init = (char *)malloc(BLOCKSIZE);

	for(int i = 0; i < 128; i++) {
		memmove(inode_init+(i*sizeof(inode)), (const char *)inode_ptr, sizeof(inode));
	}

	for (int i = SB->inode_block_idx; i < SB->data_block_idx; i++) {
		if (write_block(diskptr, i, inode_init) != 0) {
			return -1;
		}
	}

	// Free memory

	free(inode_ptr);
	free(bit_init);
	free(inode_init);
	free(SB);
	return 0;
}




int mount(disk *diskptr) {

	if (diskptr == NULL) {
		return -1;
	}

	if (diskptr->block_arr == NULL || diskptr->block_arr[0] == NULL) {
		return -1;
	}

	// Read into super block

	char *block_data = (char *)malloc(BLOCKSIZE);
	if (read_block(diskptr, 0, block_data) != 0) {
		return -1;
	}

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memmove(SB, (const super_block *)block_data, BLOCKSIZE);

	// free(block_data);

	uint32_t sfs_number = SB->magic_number;

	// verify magic number

	if (sfs_number != MAGIC) {
		return -1;
	}

	// store disk pointer

	state = diskptr;

	// Root directory file is initialized ar inode 0 to hold the file structure.
	root_inumber = 0;

	// Initialize directory types file at inode 0;

	char *block_data_2 = (char *)malloc(BLOCKSIZE);

	if (read_block(state, SB->inode_bitmap_block_idx, block_data_2) != 0) {
		return -1;
	}
	uint8_t *change_bit = (uint8_t *)malloc(sizeof(uint8_t));
	memcpy(change_bit, (const uint8_t *)block_data_2, sizeof(uint8_t));

	*change_bit = (*change_bit | (1 << 7));

	memcpy(block_data_2, (const char *)change_bit, sizeof(uint8_t));
	if (write_block(state, SB->inode_bitmap_block_idx, block_data_2) != 0) {
		return -1;
	}

	free(block_data_2);

	int inode_ind = 0;

	int inode_blk_ind = SB->inode_block_idx + inode_ind / 128;

	char *block_data_3 = (char *)malloc(BLOCKSIZE);

	if (read_block(state, inode_blk_ind, block_data_3) != 0) {
		return -1;
	}


	inode *inode_ptr = (inode*)malloc(sizeof(inode));
	memset(inode_ptr, 0, sizeof(inode));
	inode_ptr->valid = 1;
	inode_ptr->size = 0;
	inode_ptr->indirect = 0;
	for (int i = 0; i < 5; i++) inode_ptr->direct[i] = 0;

	int ind_blk = inode_ind - ((int)(inode_ind / 128))*128;

	memcpy(block_data_3 + (ind_blk * sizeof(inode)), (const char *)inode_ptr, sizeof(inode));

	if (write_block(state, inode_blk_ind, block_data_3) != 0) {
		return -1;
	}

	// Free memory

	free(block_data_3);
	free(change_bit);
	free(inode_ptr);
	free(SB);

	return 0;
}


int create_file() {

	// check mount status
	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	}

	// read super block
	char *block_data = (char *)malloc(BLOCKSIZE);
	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memmove(SB, (const super_block *)block_data, BLOCKSIZE);

	// check for empty inode in bitmap and set that bit
	bool found = false;
	int inode_ind;

	for(int i = SB->inode_bitmap_block_idx; i < SB->data_block_bitmap_idx; i++) {
		if (read_block(state, i, block_data) != 0) {
			return -1;
		}
		char *c = (char *)malloc(sizeof(char));
		for (int j = 0; j < BLOCKSIZE; j++) {
			memcpy(c, block_data+j, 1);
			int8_t x = (int8_t)(*c);
			for (int k = 0; k < 8; k++) {
				if ((int)(x & (1 << (7-k))) == 0 ) {
					found = true;
					inode_ind = ((i-SB->inode_bitmap_block_idx) * BLOCKSIZE * 8 )+(j * 8) +(k); 
					x = x | (1 << (7-k));
					*c = (char)x;
					memcpy(block_data+j, c, 1);
					if (write_block(state, i, block_data) != 0) {
						return -1;
					}
					break;
				}
			}
			if (found) break;
		}
		free(c);
		if (found) break;
	}

	if (!found) {
		return -1;
	}

	int inode_blk_ind = SB->inode_block_idx + inode_ind / 128;

	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	// initialize the new inode

	inode *inode_ptr = (inode*)malloc(sizeof(inode));
	memset(inode_ptr, 0, sizeof(inode));
	inode_ptr->valid = 1;
	inode_ptr->size = 0;
	inode_ptr->indirect = 0;
	for (int i = 0; i < 5; i++) inode_ptr->direct[i] = 0;

	int ind_blk = inode_ind - ((int)(inode_ind / 128))*128;
	
	// Copy data at repective offset
	memmove(block_data+(ind_blk * sizeof(inode)), (const char *)inode_ptr, sizeof(inode));

	if (write_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	// Free memory
	free(inode_ptr);
	free(block_data);
	free(SB);

	return inode_ind;
}

// Custom function to reset the bitmap to zero for given inode or block 
// inode argument denotes inode bitmap or block bitmap

int reset_bitmap(int index, bool inode) {

	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	}

	char *block_data = (char *)malloc(BLOCKSIZE);

	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	// Read super block

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memmove(SB, (const super_block *)block_data, BLOCKSIZE);

	// select the block according to inode or block bitmap
	int bitmap_blk_ind = index / (8 * BLOCKSIZE);

	if (inode) bitmap_blk_ind += SB->inode_bitmap_block_idx;
	else bitmap_blk_ind += SB->data_block_bitmap_idx;

	int bitmap_ind = index - ((int)(index / (8 * BLOCKSIZE)))*(8 * BLOCKSIZE);
	int byte_ind = bitmap_ind/8;
	int bit_ind = bitmap_ind - byte_ind * 8;

	if (read_block(state, bitmap_blk_ind, block_data) != 0) {
		return -1;
	}

	// Read the 8 bit values
	char *c = (char *)malloc(sizeof(char));
	memmove(c, block_data+byte_ind, 1);
	
	// Reset the respective bit and write back
	int8_t x = (int8_t)(*c);
	x = x & ~(1 << (7-bit_ind));
	*c = (char)x;
	memmove(block_data+byte_ind, c, 1);

	if (write_block(state, bitmap_blk_ind, block_data) != 0) {
		return -1;
	}

	free(block_data);
	//free(SB);
	free(c);
	return 0;
}

int remove_file(int inumber) {

	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	}

	char *block_data = (char *)malloc(BLOCKSIZE);

	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	// Read super block

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memcpy(SB, (super_block *)block_data, BLOCKSIZE);

	int inode_blk_ind = SB->inode_block_idx + inumber / 128;

	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	int ind_blk = inumber - ((int)(inumber / 128))*128;

	// Get the inode, set validity to zero and write back

	inode* inode_ptr = (inode *)malloc(sizeof(inode));
	memmove(inode_ptr, (const inode *)(block_data + (ind_blk * sizeof(inode))), sizeof(inode));

	inode_ptr->valid = 0;

	memmove(block_data+(ind_blk * sizeof(inode)), (const char *)inode_ptr, sizeof(inode));

	if (write_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	// reset inode bitmap

	if (reset_bitmap(inumber, true) != 0) {
		return -1;
	}

	// reset direct pointer bitmaps

	for (int i = 0; i < 5; i++) {
		if (inode_ptr->direct[i] == 0) break;

		int block = inode_ptr->direct[i];
		if (reset_bitmap(block, false) != 0) {
			return -1;
		}

	} 

	// reset indirect pointer bitmaps

	if (inode_ptr->indirect != 0) {

		int in_block = inode_ptr->indirect;

		if (read_block(state, in_block, block_data) != 0) {
			return -1;
		}

		int num_blocks = inode_ptr->size / BLOCKSIZE;
		if (inode_ptr->size % BLOCKSIZE != 0) num_blocks++;

		int direct_ptrs, indirect_ptrs;

		if (num_blocks > 5) {
			direct_ptrs = 5;
			indirect_ptrs = num_blocks - 5;
		}
		else {
			direct_ptrs = num_blocks;
			indirect_ptrs = 0;
		}


		for (int i = 0; i < indirect_ptrs; i++) {

			int32_t *block = (int32_t *)malloc(sizeof(int32_t));
			memmove(block, block_data + (i * sizeof(int32_t)),sizeof(int32_t));

			if (*block == 0) break;

			if (reset_bitmap(*block, false) != 0) {
				return -1;
			}

		}

		if (reset_bitmap(inode_ptr->indirect, false) != 0) {
			return -1;
		}

	}

	// free memory

	free(block_data);
	free(SB);
	free(inode_ptr);
	return 0;
}


int stat(int inumber) {

	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	}

	char *block_data = (char *)malloc(BLOCKSIZE);

	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	// Read super block

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memcpy(SB, (const super_block *)block_data, BLOCKSIZE);

	int inode_blk_ind = SB->inode_block_idx + inumber / 128;

	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	int ind_blk = inumber - ((int)(inumber / 128))*128;

	inode* inode_ptr = (inode *)malloc(sizeof(inode));
	memmove(inode_ptr, (const inode *)(block_data + (ind_blk * sizeof(inode))), sizeof(inode));

	// Get required inode varialbles to print

	int size = inode_ptr->size;
	int valid = inode_ptr->valid;
	int num_blocks = size / BLOCKSIZE;
	if (size % BLOCKSIZE != 0) num_blocks++;

	int direct_ptrs, indirect_ptrs;

	if (num_blocks > 5) {
		direct_ptrs = 5;
		indirect_ptrs = num_blocks - 5;
	}
	else {
		direct_ptrs = num_blocks;
		indirect_ptrs = 0;
	}

	printf("inode size : %d\n", size);
	printf("inode validity : %d\n", valid);
	printf("Active Data Blocks: %d\n", num_blocks);
	printf("direct pointers : %d\n", direct_ptrs);
	printf("indirect pointers : %d\n", indirect_ptrs);

	return size;
}

int min(int x,int y)
{
	return (x<y)?x:y;
}

int max(int x,int y)
{
	return (x>y)?x:y;
}

// Returns a free data blocks and sets its bitmap
int get_data_block() {
	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	}

	char *block_data = (char *)malloc(BLOCKSIZE);
	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	// read super block

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memmove(SB, (const super_block *)block_data, BLOCKSIZE);

	bool found = false;
	int block_ind;

	// Search for an empty bitmap and set its bitmap if found

	for(int i = SB->data_block_bitmap_idx; i < SB->inode_block_idx; i++) {
		if (read_block(state, i, block_data) != 0) {
			return -1;
		}
		char *c = (char *)malloc(sizeof(char));
		for (int j = 0; j < BLOCKSIZE; j++) {
			memmove(c, block_data+j, 1);
			int8_t x = (int8_t)(*c);
			for (int k = 0; k < 8; k++) {
				if ((int)(x & (1 << (7-k))) == 0 ) {
					found = true;
					block_ind = (i-SB->data_block_bitmap_idx) * BLOCKSIZE * 8 + j * 8 + k; 
					x = x | (1 << (7-k));
					*c = (char)x;
					memmove(block_data+j, c, 1);
					if (write_block(state, i, block_data) != 0) {
						return -1;
					}
					break;
				}
			}
			if (found) break;
		}
		free(c);
		if (found) break;
	}

	// If no empty data block
	if (!found) {
		printf("Disk is full \n");
		return -1;
	}

	// make the relative block ind absolute

	block_ind += SB->data_block_idx;

	free(block_data);

	return block_ind;
}

// reads the data associated with corresponding inumber, starting from offset bytes and maximum of length bytes
int read_i(int inumber, char *data, int length, int offset)
{
	// check if disk is mounted
	if (state == NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	}

	// read the super block to get the index values
	char *block_data = (char *)malloc(BLOCKSIZE);

	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memmove(SB, (const super_block *)block_data, BLOCKSIZE);

	// calculate the block index of inode using inumber
	int inode_blk_ind = SB->inode_block_idx + inumber / 128;

	// check if inumber is valid or not
	if((inode_blk_ind < SB->inode_block_idx) || (inode_blk_ind>= SB->data_block_idx))
	{
		printf("Invalid inumber\n");
		return -1;
	}	

	// read the block containing this inode
	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	int ind_blk = inumber - ((int)(inumber / 128))*128;

	// read the corresponding inode data
	inode* inode_ptr = (inode *)malloc(sizeof(inode));
	memmove(inode_ptr, (const inode *)(block_data + (ind_blk * sizeof(inode))), sizeof(inode));

	int size = inode_ptr->size;
	
	// check if offset is valid or not
	if(offset<0 || offset>size)
	{
		printf("Invalid offset\n");
		return -1;
	}
	// no data read
	if (offset == size) {
		return 0;
	}

	if(size==0)
	{
		return 0;
	}

	// maintain a curr_bytes variable to keep track of the bytes read 
	int total_bytes=min(length,size-offset),curr_bytes=0;

	// iterate over all DIRECT POINTER blocks to read the data associated with them 
	for(int i=0;i<5;i++)
	{
		// for each direct block, check if it has associated data or not
		if(offset<(i+1)*BLOCKSIZE && curr_bytes<total_bytes)
		{
			if (read_block(state, inode_ptr->direct[i], block_data) != 0) {
				return -1;
			}
			// if some bytes are read in previous block implies start reading the current block from the start of block
			if(curr_bytes!=0)
			{
				memmove(data+curr_bytes,(const char *)block_data,min(total_bytes-curr_bytes,BLOCKSIZE));
				curr_bytes+=min(total_bytes-curr_bytes,BLOCKSIZE);
			}
			// if no bytes are read in previous block implies start reading the current block according to the offset
			else
			{
				memmove(data,(const char *)(block_data+offset%BLOCKSIZE),min(total_bytes,BLOCKSIZE-(offset%BLOCKSIZE)));
				curr_bytes+=min(total_bytes,BLOCKSIZE-(offset%BLOCKSIZE));
			}
		}
	}
	
	if (read_block(state, inode_ptr->indirect, block_data) != 0) {
		return -1;
	}

	int32_t* block_index=(int32_t *)malloc(4);
	char *indirect_block_data = (char *)malloc(BLOCKSIZE);

	// iterate over all INDIRECT POINTER blocks to read the data associated with them
	for(int i=0;i<1024;i++)
	{
		// for each indirect block, check if it has associated data or not
		if(offset<(i+6)*BLOCKSIZE && curr_bytes<total_bytes)
		{
			memmove(block_index,(const int32_t *)(block_data+i*4),4);
			if (read_block(state, *block_index, indirect_block_data) != 0) {
				return -1;
			}
			// if some bytes are read in previous block implies start reading the current block from the start of block
			if(curr_bytes!=0)
			{
				memmove(data+curr_bytes,(const char *)indirect_block_data,min(total_bytes-curr_bytes,BLOCKSIZE));
				curr_bytes+=min(total_bytes-curr_bytes,BLOCKSIZE);
			}
			// if no bytes are read in previous block implies start reading the current block according to the offset
			else
			{
				memmove(data,(const char *)(indirect_block_data+offset%BLOCKSIZE),min(total_bytes,BLOCKSIZE-(offset%BLOCKSIZE)));
				curr_bytes+=min(total_bytes,BLOCKSIZE-(offset%BLOCKSIZE));
			}
		}
	}

	// free memory

	free(SB);
	free(inode_ptr);
	free(block_index);
	free(indirect_block_data);

	//return total bytes read
	return total_bytes;
}


int write_i(int inumber, char *data, int length, int offset) {

	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	}

	char *block_data = (char *)malloc(BLOCKSIZE);

	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	// read the super block

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memcpy(SB, (super_block *)block_data, BLOCKSIZE);


	int inode_blk_ind = SB->inode_block_idx + inumber / 128;

	// check inode number validity
	
	if((inode_blk_ind < SB->inode_block_idx) || (inode_blk_ind>= SB->data_block_idx))
	{
		printf("Invalid inumber\n");
		return -1;
	}	

	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	int ind_blk = inumber - ((int)(inumber / 128))*128;

	inode* inode_ptr = (inode *)malloc(sizeof(inode));

	memmove(inode_ptr, (const inode *)(block_data + (ind_blk * sizeof(inode))), sizeof(inode));


	int size = inode_ptr->size;

	// offset validity

	if(offset<0 || offset>size)
	{
		printf("Invalid offset\n");
		return -1;
	}
	// keep track of current offset and length written

	int len_written = 0;
	int curr_offset = offset;

	// as long as there is smtng to write
	while (len_written < length) {
		// If file size is full, stop writing
		if (curr_offset >= MAX_FILE_SIZE) break;
		int ptr_ind = curr_offset / BLOCKSIZE;

		// Writing in direct blocks

		if (ptr_ind < 5) {

			// If not allocated, allocate a new data block
			if (inode_ptr->direct[ptr_ind] == 0) {
				int new_block = get_data_block();
				if (new_block == -1) break;
				inode_ptr->direct[ptr_ind] = new_block;
			}



			char *new_blk_data = (char *)malloc(BLOCKSIZE);
			if (read_block(state, inode_ptr->direct[ptr_ind], block_data) != 0) {
				return -1;
			}
			int temp_ind = curr_offset - ((int)curr_offset / BLOCKSIZE)*BLOCKSIZE;

			// copy based on offset and blocksize
			memmove(new_blk_data, (const char *)block_data, temp_ind);
			memmove(new_blk_data + temp_ind, (const char *)(data + len_written), min(length - len_written, BLOCKSIZE - temp_ind));
			if (write_block(state, inode_ptr->direct[ptr_ind], new_blk_data) != 0) {
				printf("Error in writing data to new block\n");
				return -1;
			}
			free(new_blk_data);
			// increase current length and offset
			curr_offset += min(length - len_written, BLOCKSIZE - temp_ind);
			len_written += min(length - len_written, BLOCKSIZE - temp_ind);
		}	
		// writing in indirect blocks
		else {
			// allocate indirect block pointer if not present
			if (inode_ptr->indirect == 0) {
				int new_block = get_data_block();
				if (new_block == -1) break;
				inode_ptr->indirect = new_block;
			}
			if (read_block(state, inode_ptr->indirect, block_data) != 0) {
				return -1;
			}
			int32_t *block_ind = (int32_t *)malloc(sizeof(int32_t));
			// get a new indirect block
			if (curr_offset >= inode_ptr->size) {
				*block_ind = get_data_block();
				if (*block_ind == -1) break;
				memmove(block_data + (ptr_ind - 5)*4, (const char *)block_ind, 4);
				if (write_block(state, inode_ptr->indirect, block_data) != 0) {
					printf("Error in writing data to new block\n");
					return -1;
				}
			}
			// if already there and has to get the indirect block pointer
			else {
				memmove(block_ind, (int32_t *)(block_data + ((ptr_ind - 5)*4)), 4);
			}

			// Copy the data to block as above
			char *new_blk_data = (char *)malloc(BLOCKSIZE);
			if (read_block(state, *block_ind, block_data) != 0) {
				return -1;
			}
			int temp_ind = curr_offset - ((int)curr_offset / BLOCKSIZE)*BLOCKSIZE;
			memmove(new_blk_data, (const char *)block_data, temp_ind);
			memmove(new_blk_data + temp_ind, (const char *)(data + len_written), min(length - len_written, BLOCKSIZE - temp_ind));
			if (write_block(state, *block_ind, new_blk_data) != 0) {
				printf("Error in writing data to new block\n");
				return -1;
			}
			free(new_blk_data);
			// increase current length and offset
			curr_offset += min(length - len_written, BLOCKSIZE - temp_ind);
			len_written += min(length - len_written, BLOCKSIZE - temp_ind);
		}
	}

	// Adjust the file size in inode
	inode_ptr->size = max(inode_ptr->size, curr_offset);
	inode_ptr->size = min(inode_ptr->size, MAX_FILE_SIZE);

	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	memmove(block_data + (ind_blk * sizeof(inode)), (const char *)inode_ptr, sizeof(inode));

	if (write_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}
	
	// free memory
	//free(block_data);
	free(SB);
	free(inode_ptr);

	return len_written;
}

// Function to search in a directory and add a file if soesnt exist

int find_inumber(char* file_name, int inode_num, bool add, bool dir) {
	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	} 

	char *block_data = (char *)malloc(BLOCKSIZE);

	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}

	// read super block

	super_block* SB = (super_block*)malloc(sizeof(super_block));
	memcpy(SB, (const super_block *)block_data, BLOCKSIZE);

	int inode_blk_ind = SB->inode_block_idx + inode_num / 128;
	
	if((inode_blk_ind < SB->inode_block_idx) || (inode_blk_ind>= SB->data_block_idx))
	{
		printf("Invalid inumber\n");
		return -1;
	}	

	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	free(SB);

	int ind_blk = inode_num - ((int)(inode_num / 128))*128;

	inode* inode_ptr = (inode *)malloc(sizeof(inode));
	memmove(inode_ptr, (const inode *)(block_data + (ind_blk * sizeof(inode))), sizeof(inode));

	lst_node *file_node = (lst_node *)malloc(sizeof(lst_node));

	free(block_data);

	char *inode_data = (char *)malloc(inode_ptr->size);
	if(read_i(inode_num, inode_data, inode_ptr->size, 0) == -1) {
		return -1;
	}

	// Search for the file in the given directory file list

	bool pres = false;
	bool replica = false;
	int8_t *ret_inumber = (int8_t *)malloc(sizeof(int8_t));

	for (int i = 0; i < inode_ptr->size / sizeof(lst_node); i++) {
		memmove(file_node, (const lst_node *)(inode_data + (i*sizeof(lst_node))), sizeof(lst_node));
		if (strcmp(file_node->name, file_name) == 0 && file_node->valid == 1) {
			if (dir && file_node->type == 1) {
				pres = true;
				*ret_inumber = file_node->inumber;
				break;
			}  
			else if (!dir && file_node->type == 0) {
				pres = true;
				*ret_inumber = file_node->inumber;
				break;
			}
			else {
				pres = true;
				replica = true;
				break;
			}
		}
	}

	// Add the folder / file if not present and add flag is set
	if (!pres) {

		if (!add || replica) {
			return -1;
		}


		*ret_inumber = create_file();
		if (*ret_inumber == -1) {
			return -1;
		}


		file_node->valid = 1;
		if (dir) file_node->type = 1;
		else file_node->type = 0;
		file_node->name = (char *)malloc((strlen(file_name) + 1)*sizeof(char));
		strcpy(file_node->name, file_name);
		file_node->inumber = *ret_inumber;
		file_node->length = strlen(file_name);

		if (inode_ptr->size + sizeof(lst_node) > MAX_FILE_SIZE) {
			return -1;
		}

		int x = write_i(inode_num, (char *)file_node, sizeof(lst_node), inode_ptr->size);

		if (x == -1) {
			return -1;
		}


	}

	return *ret_inumber;
}

int read_file(char *filepath, char *data, int length, int offset) {

	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	} 

	int file_len = strlen(filepath);
	file_len++;

	// make filepath string copies to split for individual path staructure

	char *filepath_copy = (char *)malloc(file_len * sizeof(char));
	char *filepath_copy_2 = (char *)malloc(file_len * sizeof(char));	

	strcpy(filepath_copy, filepath);
	strcpy(filepath_copy_2, filepath);

	int depth = 0;
	char *token = strtok(filepath_copy, "/"); 
	int prev_dir_inumber = root_inumber;
    while (token != NULL) 
    { 
        depth++;
        token = strtok(NULL, "/"); 
    }

    // it should be dir till the end. file at the last
    char *token_2 = strtok(filepath_copy_2, "/");
    for (int i = 0; i < depth; i++) {
    	if (i == depth-1) prev_dir_inumber = find_inumber(token_2, prev_dir_inumber, false, false);
    	else prev_dir_inumber = find_inumber(token_2, prev_dir_inumber, false, true);
        if (prev_dir_inumber == -1) {
        	printf("unable to read file, error in finding the file \n");
        	return -1;
        }
        token_2 = strtok(NULL, "/");
    }


    int bytes_read = read_i(prev_dir_inumber, data, length, offset);

	return bytes_read;
}

int write_file(char *filepath, char *data, int length, int offset) {
	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	} 

	int file_len = strlen(filepath);
	file_len++;

	// make filepath string copies to split for individual path staructure

	char *filepath_copy = (char *)malloc(file_len * sizeof(char));
	char *filepath_copy_2 = (char *)malloc(file_len * sizeof(char));	

	strcpy(filepath_copy, filepath);
	strcpy(filepath_copy_2, filepath);


	int depth = 0;
	char *token = strtok(filepath_copy, "/"); 
	int prev_dir_inumber = root_inumber;
    while (token != NULL) 
    { 
        depth++;
        token = strtok(NULL, "/");
    }

    // set the add flag true at last depth
    char *token_2 = strtok(filepath_copy_2, "/");
    for (int i = 0; i < depth; i++) {
    	if (i == depth-1) prev_dir_inumber = find_inumber(token_2, prev_dir_inumber, true, false);
    	else prev_dir_inumber = find_inumber(token_2, prev_dir_inumber, false, true);
        if (prev_dir_inumber == -1) {
        	printf("unable to create file, error in finding parent dir\n");
        	return -1;
        }
        token_2 = strtok(NULL, "/");
    }

    int bytes_written = write_i(prev_dir_inumber, data, length, offset);

	return bytes_written;
}

int create_dir(char *dirpath) {
	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	} 

	int dir_len = strlen(dirpath);
	dir_len++;

	// make dirpath string copies to split for individual path staructure

	char *dirpath_copy = (char *)malloc(dir_len * sizeof(char));
	char *dirpath_copy_2 = (char *)malloc(dir_len * sizeof(char));


	strcpy(dirpath_copy, dirpath);
	strcpy(dirpath_copy_2, dirpath);

	// get the file depth from the root first
	int depth = 0;
	char *token = strtok(dirpath_copy, "/"); 
	int prev_dir_inumber = root_inumber;
    while (token != NULL) 
    { 
        depth++;
        token = strtok(NULL, "/"); 
    }


    // check till last dir name with add flag as false.
    // for the last name, sett add flag as tru so that dir is created
    char *token_2 = strtok(dirpath_copy_2, "/");
    for (int i = 0; i < depth; i++) {
    	if (i == depth-1) prev_dir_inumber = find_inumber(token_2, prev_dir_inumber, true, true);
    	else prev_dir_inumber = find_inumber(token_2, prev_dir_inumber, false, true);
        if (prev_dir_inumber == -1) {
        	printf("unable to create dir, error in finding parent dir \n");
        	return -1;
        }
        token_2 = strtok(NULL, "/");
    } 

    return 0;
}

int remove_dir(char *dirpath) {

	if (state== NULL) {
		printf("Operation cannot be done on Unmounted Disk\n");
		return -1;
	} 

	void *block_data = malloc(BLOCKSIZE);

	if (read_block(state, 0, block_data) != 0) {
		return -1;
	}


	int dir_len = strlen(dirpath);
	dir_len++;

	// make dirpath string copies to split for individual path staructure


	char *dirpath_copy = (char *)malloc(dir_len * sizeof(char));
	char *dirpath_copy_2 = (char *)malloc(dir_len * sizeof(char));

	strcpy(dirpath_copy, dirpath);
	strcpy(dirpath_copy_2, dirpath);

	// get the tree depth first

	int depth = 0;
	char *token = strtok(dirpath_copy, "/"); 
	int prev_dir_inumber = root_inumber;
    while (token != NULL) 
    { 
        depth++;
        token = strtok(NULL, "/"); 
    }

    // get the given dirs parent dir to remove the metadata

    char *token_2 = strtok(dirpath_copy_2, "/");
    for (int i = 0; i < depth-1; i++) {
    	prev_dir_inumber = find_inumber(token_2, prev_dir_inumber, false, true);
        if (prev_dir_inumber == -1) {
        	printf("unable to delete dir, error in findind given dir \n");
        	return -1;
        }
        token_2 = strtok(NULL, "/");
    }

    super_block* SB = (super_block*)malloc(sizeof(super_block));
	memcpy(SB, (const super_block *)block_data, BLOCKSIZE);

	int inode_blk_ind = SB->inode_block_idx + prev_dir_inumber / 128;
	
	if((inode_blk_ind < SB->inode_block_idx) || (inode_blk_ind>= SB->data_block_idx))
	{
		printf("Invalid inumber\n");
		return -1;
	}	

	if (read_block(state, inode_blk_ind, block_data) != 0) {
		return -1;
	}

	int ind_blk = prev_dir_inumber - ((int)(prev_dir_inumber / 128))*128;

	inode* inode_ptr = (inode *)malloc(sizeof(inode));
	memmove(inode_ptr, (const inode *)(block_data + (ind_blk * sizeof(inode))), sizeof(inode));

	lst_node *file_node = (lst_node *)malloc(sizeof(lst_node));

	char *inode_data = (char *)malloc(inode_ptr->size);
	if(read_i(prev_dir_inumber, inode_data, inode_ptr->size, 0) == -1) {
		return -1;
	}

	// find the dir metadata in parent dir and set valid as 0

	bool pres = false;
	bool replica = false;

	for (int i = 0; i < inode_ptr->size / sizeof(lst_node); i++) {
		memmove(file_node, (const lst_node *)(inode_data + (i*sizeof(lst_node))), sizeof(lst_node));
		if (strcmp(file_node->name, token_2) == 0 && file_node->valid == 1) {
			if (file_node->type == 1) {
				pres = true;
				file_node->valid = 0;

				if(write_i(prev_dir_inumber, (char *)file_node, sizeof(lst_node), i*sizeof(lst_node)) == -1) {
					return -1;
				}
				pres = true;
				break;
			}  
		}
	}

	if (pres) printf("Directory Deleted \n");

	free(block_data);
	free(SB);

	return 0;
}
