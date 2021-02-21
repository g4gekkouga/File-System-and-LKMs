// include necessary header files
#include "disk.h"

// creates a disk of size 'nbytes'
disk *create_disk(int nbytes)
{
	disk *DISK=(disk*)malloc(sizeof(disk));
	DISK->size=nbytes;
	DISK->blocks=((nbytes-24)/BLOCKSIZE);
	DISK->reads=0;
	DISK->writes=0;

	// create array of pointers to data blocks and allocate block memory
	char **arr;
	arr=(char**)malloc((DISK->blocks)*sizeof(char*));
	for(int i=0;i<DISK->blocks;i++)
	{
		arr[i]=(char*)malloc(BLOCKSIZE);
		if(arr[i]==NULL)
		{
			printf("Disk Creation Failed\n");
			return NULL;
		}
	}

	DISK->block_arr=arr;
	return DISK;
}

// custom function to check if the block number given is valid or not
bool isValidBlock(disk *diskptr, int blocknr)
{
	if(blocknr>=0 && blocknr<(diskptr->blocks))
		return true;
	return false;
}

// read the disk block data and increment reads count
int read_block(disk *diskptr, int blocknr, void *block_data)
{
	if(!isValidBlock(diskptr,blocknr))
	{
		printf("Invalid Block Pointer\n");
		return -1;
	}
	char* block_ptr=diskptr->block_arr[blocknr];

	//copies data from block_ptr to block_data
	memmove(block_data,(const void *)block_ptr,BLOCKSIZE);

	diskptr->reads=diskptr->reads+1;
	return 0;
}

// write the disk block data and increment writes count
int write_block(disk *diskptr, int blocknr, void *block_data)
{
	if(!isValidBlock(diskptr,blocknr))
	{
		printf("Invalid Block Pointer\n");
		return -1;
	}
	char* block_ptr=diskptr->block_arr[blocknr];

	//copies data from block_data to block_ptr
	memmove(block_ptr,(const char*)block_data,BLOCKSIZE);

	diskptr->writes=diskptr->writes+1;
	return 0;
}

// free the disk pointer 
int free_disk(disk *diskptr)
{
	// free the disk pointer after the disk blocks allocated are freed
	for(int i=0;i<(diskptr->blocks);i++)
	{
		free(diskptr->block_arr[i]);
	}
	free(diskptr);
	return 0;
}
