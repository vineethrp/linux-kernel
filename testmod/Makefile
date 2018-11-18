obj-m += test.o

# Targets if /proc/ikheaders is used
all:
ifneq ("$(wildcard /proc/ikheaders.tbz)", "")
	mkdir -p /tmp/ttt
	rm -rf /tmp/ttt/*
	tar -xvf /proc/ikheaders.tbz -C /tmp/ttt/ >/dev/null
endif
	make -C /tmp/ttt M=$(PWD) modules

clean:
ifneq ("$(wildcard /proc/ikheaders.tbz)", "")
	mkdir -p /tmp/ttt
	rm -rf /tmp/ttt/*
	tar -xvf /proc/ikheaders.tbz -C /tmp/ttt/
endif
	make -C /tmp/ttt M=$(PWD) clean
