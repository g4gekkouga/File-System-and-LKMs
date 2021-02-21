obj-m+=17CS30025_part_b_2.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	sudo insmod 17CS30025_part_b_2.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	sudo rmmod 17CS30025_part_b_2.ko
