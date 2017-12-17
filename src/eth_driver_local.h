#ifndef ETH_DRIVER_LOCAL_H
#define ETH_DRIVER_LOCAL_H

// Global

#define EIE (0x1B)
#define EIR (0x1C)
#define ESTAT (0x1D)
#define ECON2 (0x1E)
#define ECON1 (0x1F)

// Bank 0

#define ERDPTL (0x00)
#define ERDPTH (0x01)

#define EWRPTL (0x02)
#define EWRPTH (0x03)

#define ETXSTL (0x04)
#define ETXSTH (0x05)

#define ETXNDL (0x06)
#define ETXNDH (0x07)

#define ERXSTL (0x08)
#define ERXSTH (0x09)

#define ERXNDL (0x0A)
#define ERXNDH (0x0B)

#define ERXRDPTL (0x0C)
#define ERXRDPTH (0x0D)

#define ERXWRPTL (0x0E)
#define ERXWRPTH (0x0F)

#define EDMASTL (0x10)
#define EDMASTH (0x11)

#define EDMANDL (0x12)
#define EDMANDH (0x13)

#define EDMADSTL (0x14)
#define EDMADSTH (0x15)

#define EDMACSL (0x16)
#define EDMACSH (0x17)

void set_register_word(int address, int data);
void set_bank(int bank);

void set_read_pointer(int address);
void set_write_pointer(int address);
void set_rx_range(int start, int end);

#endif
