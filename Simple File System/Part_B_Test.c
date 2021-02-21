#include "sfs.c"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main()
{
    uint32_t nbytes = 409600;
    disk *diskptr = create_disk(nbytes);
    assert(diskptr != NULL);
    printf("Creation Disk Done\n\n");
    assert(format(diskptr) >= 0);
    printf("Disk Format Done\n\n");
    assert(mount(diskptr) >= 0);
    printf("Disk Mount Done\n\n");

    int inum = create_file();
    assert(inum > 0);
    printf("New file created and given inode = %d\n\n", inum);
    assert(stat(inum) >= 0);
    printf("Printed inode stats\n\n");
    int off = 0, i;
    char data[10*BLOCKSIZE + 178];
    for (i = 0; i < 10*BLOCKSIZE + 178; i++)
    	data[i] = (char)('0' + i%100);

    int ret = write_i(inum, data, 10*BLOCKSIZE + 178 , off);
    assert(ret == 10*BLOCKSIZE + 178);
    printf("write_i() done. written size = %d bytes\n\n", 10*BLOCKSIZE + 178);

    char data2[10*BLOCKSIZE + 178];
    ret = read_i(inum, data2, 10*BLOCKSIZE + 178, off);
    assert(ret == 10*BLOCKSIZE + 178);
    printf("Data read from read_i() and returned same length written = %d bytes\n\n", 10*BLOCKSIZE + 178);

    for (i = 0; i < 10*BLOCKSIZE + 178; i++)
    	assert(data2[i] == data[i]);

    printf("Compared the data written and read. They are similar\n\n");

    assert(stat(inum) >= 0);
    printf("New stats of the block printed\n\n");
    assert(remove_file(inum) >= 0);
    printf("File inode removed\n\n");

    assert(stat(inum) >= 0);
    
    free_disk(diskptr);
    printf("New Stats. Valid = 0\n\n");
    printf("Tests successful\n");
}
















