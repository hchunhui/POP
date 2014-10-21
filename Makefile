
all:
	make -C io
	make -C maple
	make -C cbench
	make -C main

clean:
	make -C io clean
	make -C maple clean
	make -C cbench clean
	make -C main clean
