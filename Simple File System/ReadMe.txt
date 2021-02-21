Directory Structure Implementation :

The directory structure is implemented as follows -

typedef struct lst_node {
	int valid;
	int type;
	char* name;
	int length;
	int inumber;
} lst_node;

- A directory is a normal file with the data stored as the a list of nodes as above.

- Each node is of size 24 bytes. So in a directory file, every 32 bytes, there is 
one file metadata.

- Root directory file is default at inode 0 and initialized during mount().

- Any other file should be in this tree rooted at root dir.

- while creating a dir, give the path as - x1/x2/x3 or /x1/x2/x3
 (Please do not give ./x1/x2/x3 or any others)

- the code will recursive check the files x1, x2 and create a 
file with types directory and insert in x2 data.

- If any x1 or x2 is absent, it will give an error.

- Similar with the read and write files. write file will initialize a file
if not already present. (pleas give as x1/x2/x3.ext or /x1/x2/x3.ext)

- while removing a directory, recursive in bottom up manner, all nodes as above,
the validity is set to zero. So next time this directory or its sub-directory are
tried to access, they will be absent.


Test Files :
 - Included 3 sample test files.
 - One for each PART-A, Part-B, Part-C.
