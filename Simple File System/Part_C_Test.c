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


    char dirpath[7] = "folder";
    
    char filepath[16] = "folder/file.txt";

    char data_write[42] = "This is the test data written to the file";

    char *data_read = (char *)malloc(42);

    assert(stat(0) >= 0);
    printf("Printed stats of root dir inode. Empty\n");
    
    assert(create_dir(dirpath) == 0);

    printf("Created Directory %s \n\n", dirpath);

    assert(stat(0) >= 0);
    printf("Printed stats of root dir inode. size = 24 - one sub folder");

    assert(write_file(filepath, data_write, 42, 0) == 42);

    printf("File %s created and data : '%s' is written\n\n", filepath, data_write);

    assert(read_file(filepath, data_read, 42, 0) == 42);

    printf("Data Read = '%s'\n\n", data_read);

    assert(remove_dir(dirpath) == 0);

    printf("Directory %s deteted\n\n", dirpath);

    assert(read_file(filepath, data_read, 42, 0) == -1);

    printf("read_file for %s returned -1. So file is not found", filepath);
    
    free_disk(diskptr);
    printf("Tests successful\n");
}
















