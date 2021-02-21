obj-m+=17CS30025_part_b_1.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	sudo insmod 17CS30025_part_b_1.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	sudo rmmod 17CS30025_part_b_1.ko
