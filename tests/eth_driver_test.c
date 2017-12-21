#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include "eth_driver.h"
#include "eth_driver_local.h"

int pass() {
	printf("PASS\n");
	return 1;
}

int fail(char *message) {
	printf("FAIL [%s]\n", message);
	return 0;
}

int init_ethernet_test() {
	printf("Ethernet Initialization: ");
	fflush(stdout);
	if (!init_ethernet()) {
		return fail("Unable to initialize ethernet");
	}
	return pass();
}

int soft_reset_test() {
	printf("Soft-Reset:              ");
	set_control_register(ECON1, 0x03);
	int read = read_control_register(ECON1) & 0x03;
	if (read & 0x03 != 0x03) {
		return fail("Unable to set control register");
	}

	soft_reset();

	read = read_control_register(ECON1) & 0x03;
	if (read & 0x03 != 0x00) {
		return fail("Soft reset did not reset ECON1 register");
	}
	return pass();
}

int write_control_register_test() {
	printf("Control Register Write:  ");
	write_control_register(ECON1, 0xC3);
	int read = read_control_register(ECON1);
	if (read != 0xC3) {
		return fail("Unable to write to control register");
	}
	return pass();
}

int read_control_register_test() {
	printf("Control Register Read:   ");
	write_control_register(ECON1, 0x81);
	int read = read_control_register(ECON1);
	if (read != 0x81) {
		return fail("Unable to read from control register");
	}
	return pass();
}

int set_control_register_test() {
	printf("Control Register Set:    ");
	write_control_register(ECON1, 0x42);
	set_control_register(ECON1, 0x81);
	int read = read_control_register(ECON1);
	if (read != 0xC3) {
		return fail("Unable to set control register bit");
	}
	return pass();
}

int clear_control_register_test() {
	printf("Control Register Clear:  ");
	write_control_register(ECON1, 0xC3);
	clear_control_register(ECON1, 0x42);
	int read = read_control_register(ECON1);
	if (read != 0x81) {
		return fail("Unable to clear control register bit");
	}
	return pass();
}

int write_buffer_test() {
	printf("Buffer Read/Write:       ");
	soft_reset();

	char write_buf[8192];
	char read_buf[8192];

	srand(time(NULL));

	int i;
	for (i = 0; i < sizeof(write_buf); i++) {
		write_buf[i] = rand();
	}

	set_bank(0);

	for (i = 0; i < 4; i++) {
		write_control_register(ERDPTL, 0);
		write_control_register(ERDPTH, 0);

		write_control_register(EWRPTL, 0);
		write_control_register(EWRPTH, 0);

		buffer_write(write_buf, 0, sizeof(write_buf));

		buffer_read(read_buf, 0, sizeof(read_buf));

		if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) {
			return fail("Memory block mismatch");
		}
	}

	return pass();
}

int phy_reg_test() {
	printf("Phy Registers:           ");

	write_phy_reg(PHCON1, 0x4000);
	int read = read_phy_reg(PHCON1);

	if (read != 0x4000) {
		return fail("PHCON1 readback failed");
	}

	write_phy_reg(PHCON2, 0x4000);
	read = read_phy_reg(PHCON2);

	if (read != 0x4000) {
		return fail("PHCON2 readback failed");
	}

	write_phy_reg(PHIE, 0x0012);
	read = read_phy_reg(PHIE);

	if (read != 0x0012) {
		return fail("PHIE readback failed");
	}

	return pass();
}

int read_phy_reg(int address);
void write_phy_reg(int address, int data);


int main() {
	if (!init_ethernet_test()) {
		return -1;
	}

	int pass = 1;

	pass &= soft_reset_test();
	pass &= write_control_register_test();
	pass &= read_control_register_test();
	pass &= set_control_register_test();
	pass &= clear_control_register_test();
	pass &= write_buffer_test();
	pass &= phy_reg_test();

	printf("\nTests: %s\n", pass ? "PASS" : "FAIL");

	if (!pass) {
		return -1;
	}

	return 0;
}

