Note 1: 

sudo insmod ...
This command is included in the makefile

sudo rmmod ...
This command is included in the clean part of makefile

So no need to seperately insert and remove the module

Note 2:

Code is similar for both the parts as we included read, write and ioctl
in single code and any method can be used for userspace interaction.

Seperate makefiles and module names are used for both the parts and included
in respective folder.

