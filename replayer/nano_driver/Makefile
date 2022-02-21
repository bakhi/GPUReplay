MOD = gr
KPATH :=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)
obj-m += $(MOD).o

#append other source files 
$(MOD)-objs+= gr_main.o gr_mempool.o gr_irq.o gr_hw.o
EXTRA_CFLAGS += -O2
EXTRA_CFLAGS += -I$(PWD)/include

all:
	$(MAKE) -C $(KPATH) M=$(PWD) modules

debug:
	echo $(EXTRA_CFLAGS)
	KCPPFLAGS="-DDEBUG" $(MAKE) -C $(KPATH) M=$(PWD) modules

clean:
	$(MAKE) -C $(KPATH) M=$(PWD) clean
