#ifndef GPIO_SPI_H
#define GPIO_SPI_H

#include <stdint.h>

/**
 * Initialize spi gpio driver
 * returns non-zero on success
 */

int init_gpio_spi();

/**
 * Close spi gpio driver
 * returns non-zero on success
 */
int close_gpio_spi();

/**
 * Issues a soft reset
 */
void soft_reset();

/**
 * Writes to a control register
 *
 * address:  The address of the control register
 * data:     The data to write
 */
void write_control_register(int address, int data);

/**
 * Reads from a control register
 *
 * address:  The address of the control register
 *
 * returns the byte read
 */
int read_control_register(int address);

/**
 * Writes to a 2 byte control register
 *
 * address:  The address of the control register
 * data:     The data to write
 */
void write_control_register_word(int address, int data);

/**
 * Reads from a 2 byte control register
 *
 * address:  The address of the control register
 *
 * returns the byte read
 */
int read_control_register_word(int address);

/**
 * Writes to buffer memory
 *
 * buf:      Buffer containing data to write
 * off:      The location of the first byte to write
 * len:      The number of bytes to write
 *
 * returns the number of bytes written
 */
int buffer_write(uint8_t* buf, int off, int len);

/**
 * Reads from buffer memory
 *
 * buf:      Buffer to place the data
 * off:      The location to place the first byte read
 * len:      The number of bytes to read
 *
 * returns the number of bytes read
 */
int buffer_read(uint8_t* buf, int off, int len);

/**
 * Clear control register bits
 *
 * address:  The address of the control register
 * data:     The bits to clear
 */
void clear_control_register(int address, int data);

/**
 * Set control register bits
 *
 * address:  The address of the control register
 * data:     The bits to set
 */
void set_control_register(int address, int data);

/**
 * Sets the bank and pushes the old value onto the stack
 *
 * bank:     The new bank value
 */
void push_bank(int bank);

/**
 * Pops the bank off the stack and sets the bank to the popped value
 */
void pop_bank();

/**
 * Sets the buffer read pointer location
 *
 * address:  The new read pointer location
 */
void set_read_pointer(int address);

/**
 * Sets the buffer write pointer location
 *
 * address:  The new write pointer location
 */
void set_write_pointer(int address);

/**
 * Reads from a physical register
 *
 * address:  The address of the physical control register
 */
int read_phy_reg(int address);

/**
 * Writes to a physical register
 *
 * address:  The address of the physical control register
 * data:     The data to write
 */
void write_phy_reg(int address, int data);


#endif
