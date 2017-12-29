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

	push_bank(0);

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

	pop_bank();

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

int control_register_word_test() {
        printf("Control Register Word:   ");
	push_bank(0);
        write_control_register_word(ETXSTL, 0x1234);
        int read = read_control_register_word(ETXSTL);
        if (read != 0x1234) {
                return fail("Unable to write/read control register word");
        }

        write_control_register_word(ETXSTL, 0x1AA5);
        read = read_control_register(ETXSTL);
        if (read != 0xA5) {
                return fail("Unexpected high-byte readback after word write");
        }
	read = read_control_register(ETXSTH);
        if (read != 0x1A) {
                return fail("Unexpected low-byte readback after word write");
        }

	write_control_register(ETXSTL, 0x67);
	write_control_register(ETXSTH, 0x09);
	read = read_control_register_word(ETXSTL);
        if (read != 0x0967) {
                return fail("Unexpected word readback after 2 byte writes");
        }
	pop_bank();

        return pass();
}

int push_bank_test() {
	printf("Bank Stack Test:         ");
	push_bank(3);
	int bank = read_control_register(ECON1) & 0x3;
	if (bank != 3) {
		return fail("Failed to update bank with push_bank");
	}
	push_bank(2);
	bank = read_control_register(ECON1) & 0x3;
        if (bank != 2) {
                return fail("Failed to update bank with push_bank");
        }
	pop_bank();
	bank = read_control_register(ECON1) & 0x3;
        if (bank != 3) {
                return fail("Bank did not restore after first pop_bank");
        }
	push_bank(0);
        bank = read_control_register(ECON1) & 0x3;
        if (bank != 0) {
                return fail("Failed to update bank with push_bank");
        }
	push_bank(1);
        bank = read_control_register(ECON1) & 0x3;
        if (bank != 1) {
                return fail("Failed to update bank with push_bank");
        }
	pop_bank();
        bank = read_control_register(ECON1) & 0x3;
        if (bank != 0) {
                return fail("Bank did not restore after second pop_bank");
        }
	pop_bank();
        bank = read_control_register(ECON1) & 0x3;
        if (bank != 3) {
                return fail("Bank did not restore after third pop_bank");
        }
	pop_bank();
	return pass();
}


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
	pass &= control_register_word_test();
	pass &= push_bank_test();

	printf("\nTests: %s\n", pass ? "PASS" : "FAIL");

	if (!pass) {
		return -1;
	}

	return 0;
}

