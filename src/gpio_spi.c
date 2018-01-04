#include "gpio_spi.h"
#include "eth_driver_local.h"

// This code is based on code from Dom and Gert
// https://elinux.org/RPi_GPIO_Code_Samples#Direct_register_access

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "gpio_spi.h"

#define GPIO_OFFSET (0x200000)

#define BLOCK_SIZE (4096)
#define BANK_STACK_SIZE (128)

#define SHORT_WAIT {int wait_delay;  for (wait_delay = 0; wait_delay < 16; wait_delay++) {*(gpio_clr) = 0;}}

#define OUTPUT (1)
#define INPUT (2)

#define MOSI (10)
#define MISO (9)
#define SCK (11)
#define CS (8)
#define INT (25)

#define CS_BIT (1 << CS)
#define SCK_BIT (1 << SCK)
#define MOSI_BIT (1 << MOSI)
#define MISO_BIT (1 << MISO)
#define INT_BIT (1 << INT)

int get_peri_base();
int get_revision_id();
int drop_root();

void set_dir(int gpio, int dir);
void set_input(int gpio);
void set_output(int gpio);
int init_gpio();
int close_gpio();

int init_spi();
void assert_cs();
void deassert_cs();
void spi_write_byte(int b);
int spi_read_byte();

int peri_base_table[][2] = {
	{0x0002,   0x20000000},
	{0x0003,   0x20000000},
	{0x0004,   0x20000000},
	{0x0005,   0x20000000},
	{0x0006,   0x20000000},
	{0x0007,   0x20000000},
	{0x0008,   0x20000000},
	{0x0009,   0x20000000},
	{0x000D,   0x20000000},
	{0x000E,   0x20000000},
	{0x000F,   0x20000000},
	{0x0010,   0x20000000},
	{0x0011,   0x20000000},
	{0x0012,   0x20000000},
	{0x900092, 0x20000000},
	{0x900093, 0x20000000},
	{0x9000C1, 0x20000000},
	{0xa01041, 0x3F000000},
	{0xa21041, 0x3F000000},
	{0xa02082, 0x3F000000},
	{0xa22082, 0x3F000000}
};

int bank_stack[BANK_STACK_SIZE];
int bank_index = 0;
int current_bank = 0;

void *gpio_mmap = NULL;

volatile unsigned *gpio_base;
volatile unsigned *gpio_set;
volatile unsigned *gpio_clr;
volatile unsigned *gpio_get;

int init_spi() {
	set_input(MISO);
	set_input(INT);
	set_output(MOSI);
	set_output(CS);
	set_output(SCK);

	*(gpio_set) = CS_BIT | MOSI_BIT;
	*(gpio_clr) = SCK_BIT;
	return 1;
}

void assert_cs() {
	SHORT_WAIT;
	*(gpio_clr) = CS_BIT;
}

void deassert_cs() {
	SHORT_WAIT;
	*(gpio_clr) = SCK_BIT;
	SHORT_WAIT;
	*(gpio_set) = CS_BIT;
	SHORT_WAIT;
	*(gpio_set) = SCK_BIT;
}

void spi_write_byte(int b) {
	b <<= (MOSI - 7);
	int i;
	for (i = 0; i < 8; i++) {
		SHORT_WAIT;
		*(gpio_clr) = SCK_BIT | MOSI_BIT;
		*(gpio_set) = b & MOSI_BIT;
		b <<= 1;
		SHORT_WAIT;
		*(gpio_set) = SCK_BIT;
	}
}

int spi_read_byte() {
	int b = 0;
	int i;
	for (i = 0; i < 8; i++) {
		SHORT_WAIT;
		*(gpio_clr) = SCK_BIT;
		SHORT_WAIT;
		*(gpio_set) = SCK_BIT;
		b |= (*(gpio_get)) & MISO_BIT;
		b <<= 1;
	}
	b >>= (MISO + 1);
	return b;
}

void soft_reset() {
	assert_cs();
	spi_write_byte(0xFF);
	deassert_cs();
	usleep(200); // Must wait at least 50 us after a reset
}

void write_control_register(int address, int data) {
	assert_cs();
	int cmd = 0x40 | (0x1F & address);
	spi_write_byte(cmd);
	spi_write_byte(data);
	deassert_cs();
}

int read_control_register(int address) {
	assert_cs();
	int cmd = 0x00 | (0x1F & address);
	spi_write_byte(cmd);
	int data = spi_read_byte();
	deassert_cs();
	return data;
}

void clear_control_register(int address, int data) {
	assert_cs();
	int cmd = 0xA0 | (0x1F & address);
	spi_write_byte(cmd);
	spi_write_byte(data);
	deassert_cs();
}

void set_control_register(int address, int data) {
	assert_cs();
	int cmd = 0x80 | (0x1F & address);
	spi_write_byte(cmd);
	spi_write_byte(data);
	deassert_cs();
}


int buffer_write(uint8_t* buf, int off, int len) {
	assert_cs();
	spi_write_byte(0x7A);
	char *t = buf + off;
	int i;
	for (i = 0; i < len; i++) {
		spi_write_byte(*t);
		t++;
	}
	deassert_cs();
	return len;
}

int buffer_read(uint8_t* buf, int off, int len) {
	assert_cs();
	spi_write_byte(0x3A);
	char *t = buf + off;
	int i;
	for (i = 0; i < len; i++) {
		*t = spi_read_byte();
		t++;
	}
	deassert_cs();
	return len;
}

void set_read_pointer(int address) {
        write_control_register_word(ERDPTL, address);
}

void set_write_pointer(int address) {
        write_control_register_word(EWRPTL, address);
}

void set_bank(int bank) {
        current_bank = bank;
        clear_control_register(ECON1, ECON1_BSEL);
        set_control_register(ECON1, bank & ECON1_BSEL);
}

void push_bank(int bank) {
	if (bank_index >= BANK_STACK_SIZE) {
		fprintf(stderr, "Bank stack exhausted\n");
		exit(-1);
	} 
	bank_stack[bank_index] = current_bank;
	bank_index++;
	set_bank(bank);
}

void pop_bank() {
	if (bank_index <= 0) {
		fprintf(stderr, "Bank stack underflow\n");
		exit(-1);
	}
	bank_index--;
	set_bank(bank_stack[bank_index]);
}

void write_control_register_word(int address, int data) {
        write_control_register(address, data);
        write_control_register(address + 1, data >> 8);
}

int read_control_register_word(int address) {
        return ((read_control_register(address + 1) & 0xFF) << 8) | (read_control_register(address) & 0xFF);
}

void wait_busy() {
        usleep(12);
        push_bank(3);
        while (read_control_register(MISTAT) & MISTAT_BUSY) {
                usleep(1);
        }
        pop_bank();
}

int read_phy_reg(int address) {
        push_bank(2);
        write_control_register(MIREGADR, address & 0x1F);
        set_control_register(MICMD, MICMD_MIIRD);
        wait_busy();
        clear_control_register(MICMD, MICMD_MIIRD);
        int read = read_control_register_word(MIRDL);
        pop_bank();
        return read;
}

void write_phy_reg(int address, int data) {
        push_bank(2);
        write_control_register(MIREGADR, address & 0x1F);
        write_control_register(MIWRL, data);
        write_control_register(MIWRH, data >> 8);
        pop_bank();
        wait_busy();
}

int drop_root() {
	if (setuid(getuid()) == -1) {
		fprintf(stderr, "Unable to change uid to drop root rights\n");
		return 0;
	}

	if (setuid(0) != -1) {
		fprintf(stderr, "ERROR:    Root authority release after memory mapping was unsuccessful\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "          This program should not be run using sudo\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "          You can grant releasable root rights to the executable using\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "          sudo chown 0:0 filename\n");
		fprintf(stderr, "          sudo chmod u+s filename\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "          This allows the program to release root rights as soon as gpio access is\n");
		fprintf(stderr, "          obtained.  If sudo is used to run the program, then it retains root rights\n");
		fprintf(stderr, "          permanently.\n");
		return 0;
	}
	return 1;
}

void set_dir(int gpio, int dir) {
	if (dir == OUTPUT) {
		set_output(gpio);
	} else {
		set_input(gpio);
	}
}

void set_input(int gpio) {
	int offset = gpio / 10;
	int shift = (gpio % 10) * 3;
	*(gpio_base + offset) &= ~(7 << shift);
}

void set_output(int gpio) {
	set_input(gpio);
	int offset = gpio / 10;
	int shift = (gpio % 10) * 3;
	*(gpio_base + offset) |= 1 << shift;
}

int close_gpio() {
	if (gpio_mmap == NULL) {
		fprintf(stderr, "Cannot close GPIO if it has not been initialized\n");
		return 0;
	}
	munmap(gpio_mmap, BLOCK_SIZE);
	gpio_mmap = NULL;
	return 1;
}

int init_gpio_spi() {
	if (!init_gpio()) {
		return 0;
	}
	return init_spi();
}

int close_gpio_spi() {
	return close_gpio();
}

int init_gpio() {
	if (gpio_mmap != NULL) {
		fprintf(stderr, "GPIO memory map is already open\n");
		return 0;
	}

	int peri_base = get_peri_base();
	if (peri_base == -1) {
		fprintf(stderr, "Unable to determine peripheral base address, unknown revision id\n");
		gpio_mmap = NULL;
		return 0;
	}

	int mmap_fd = open("/dev/mem", O_RDWR | O_SYNC);

	if (mmap_fd < 0) {
		fprintf(stderr, "Unable to open memory map file\n");
		fprintf(stderr, "This normally means that the binary has not been given sufficient rights\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "          You can grant releasable root rights to the executable using\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "          sudo chown 0:0 filename\n");
		fprintf(stderr, "          sudo chmod u+s filename\n");

		gpio_mmap = NULL;
		return 0;
	}

	gpio_mmap = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, peri_base + GPIO_OFFSET);

	close(mmap_fd);

	if (gpio_mmap == MAP_FAILED) {
		fprintf(stderr, "Unable to map GPIO memory\n");
		gpio_mmap = NULL;
		return 0;
	}

	if (!drop_root()) {
		gpio_mmap = NULL;
		return 0;
	}

	gpio_base = (volatile unsigned *) gpio_mmap;
	gpio_set = (volatile unsigned *) (gpio_base + 7);
	gpio_clr = (volatile unsigned *) (gpio_base + 10);
	gpio_get = (volatile unsigned *) (gpio_base + 13);

	return 1;
}

int get_peri_base() {
	int revision_id = get_revision_id();

	int table_size = sizeof(peri_base_table) / sizeof(*peri_base_table);
	int i;
	for (i = 0; i < table_size; i++) {
		if (peri_base_table[i][0] == revision_id) {
			return peri_base_table[i][1];
		}
	}
	return -1;
}

int get_revision_id() {
	FILE* fp;
	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL) {
		fprintf(stderr, "Unable open /proc/cpuinfo file for revision id\n");
		return -1;
	}

	char buffer[16384];
	memset(buffer, 0, sizeof(buffer));

	size_t size = fread(buffer, 1, sizeof(buffer) - 1, fp);

	fclose(fp);

	const char* needle = "Revision";
	int needle_size = strlen(needle);

	int end = size - needle_size - 1;
	int i;
	for (i = 0; i < end; i++) {
		if (buffer[i] != needle[0]) {
			continue;
		}
		if (strncmp(buffer + i, needle, needle_size) == 0) {
			i += needle_size;
			while (i < size && (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == ':')) {
				i++;
			}
			if (i < size) {
				int revision_id = -1;
				int fields = sscanf(buffer + i, "%x", &revision_id);
				if (fields != 1) {
					continue;
				}
				return revision_id;
			}
		}
	}
	fprintf(stderr, "Unable to find Revision field in cpuinfo\n");
	return -1;
}
