#include <stdio.h>
#include <string.h>

#include "eth_driver.h"
#include "eth_driver_local.h"

int test_present() {
	clear_control_register(ECON1, 0x3);
	int read = read_control_register(ECON1);
	if ((read & 0x3) != 0) {
		return 0;
	}
	set_control_register(ECON1, 0x3);
	read = read_control_register(ECON1);
	if ((read & 0x3) != 0x3) {
		return 0;
	}
	set_bank(0);
	return 1;
}

int init_ethernet() {
	if (!init_gpio_spi()) {
		fprintf(stderr, "Unable to initialize spi gpio driver\n");
		return 0;
	}

	soft_reset();

	if (!test_present()) {
		fprintf(stderr, "Unable to detect ENC28J60 board\n");
		return 0;
	}

	set_bank(0);
	set_rx_range(0x800, 0x1FFF);
	set_write_pointer(0x0000);
	set_read_pointer(0x0800);

	int clk_ready = 0;
	while (!clk_ready) {
		clk_ready =  read_control_register(ESTAT) & 0x01;
	}

	return 1;
}

int close_ethernet() {
	if (!close_gpio_spi()) {
		fprintf(stderr, "Unable to close spi gpio driver\n");
		return 0;
	}
	return 1;
}

void set_bank(int bank) {
	clear_control_register(ECON1, 0x03);
	set_control_register(ECON1, bank & 0x03);
}

void set_register_word(int address, int data) {
	write_control_register(address, data);
	write_control_register(address + 1, data >> 8);
}

void set_read_pointer(int address) {
	set_register_word(ERDPTL, address);
}

void set_write_pointer(int address) {
	set_register_word(EWRPTL, address);
}

void set_rx_range(int start, int end) {
	set_register_word(ERXSTL, start);
	set_register_word(ERXNDL, end);
}
