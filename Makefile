CC=gcc
CFLAGS=-I./src

obj/%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

sudo: etransfer
	sudo chown 0:0 etransfer 
	sudo chmod u+s etransfer
	touch sudo

etransfer: obj/eth_driver.o  obj/gpio_spi.o obj/ip_stack.o  obj/web_server.o
	$(CC) -o etransfer obj/eth_driver.o obj/gpio_spi.o obj/ip_stack.o obj/web_server.o

.PHONY: clean

clean:
	rm obj/*.o
	rm sudo
	rm -f etransfer

