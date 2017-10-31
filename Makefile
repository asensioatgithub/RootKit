#obj-m += hideps1_mod.o
#obj-m += printsct5.o
#obj-m += printsct.o
#obj-m += unlink.o
obj-m += myrootkit.o
all:
		make -C /lib/modules/$(shell uname -r)/build  M=$(PWD) modules
clean:
		make -C /lib/modules/$(shell uname -r)/build  M=$(PWD) clean
