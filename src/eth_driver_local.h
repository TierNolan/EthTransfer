#ifndef ETH_DRIVER_LOCAL_H
#define ETH_DRIVER_LOCAL_H

#include "eth_driver.h"

// Global

#define EIE (0x1B)
#define EIR (0x1C)
#define ESTAT (0x1D)
#define ECON2 (0x1E)
#define ECON1 (0x1F)

#define ECON1_TXRST  (0x80)
#define ECON1_RXRST  (0x40)
#define ECON1_DMAST  (0x20)
#define ECON1_CSUMEN (0x10)
#define ECON1_TXRTS  (0x08)
#define ECON1_RXEN   (0x04)
#define ECON1_BSEL   (0x03)

#define ECON2_AUTOINC (0x80)
#define ECON2_PKTDEC  (0x40)
#define ECON2_PWRSV   (0x20)
#define ECON2_VRPS    (0x08)

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

// Bank 1

#define EHT0 (0x00)
#define EHT1 (0x01)
#define EHT2 (0x02)
#define EHT3 (0x03)
#define EHT4 (0x04)
#define EHT5 (0x05)
#define EHT6 (0x06)
#define EHT7 (0x07)

#define EPMM0 (0x08)
#define EPMM1 (0x09)
#define EPMM2 (0x0A)
#define EPMM3 (0x0B)
#define EPMM4 (0x0C)
#define EPMM5 (0x0D)
#define EPMM6 (0x0E)
#define EPMM7 (0x0F)

#define EPMCSL (0x10)
#define EPMCSH (0x11)

#define EPMOL (0x14)
#define EPMOH (0x15)

#define ERXFCON (0x18)
#define EPKTCNT (0x19)

// Bank 2

#define MACON1 (0x00)
#define MACON3 (0x02)
#define MACON4 (0x03)
#define MABBIPG (0x04)
#define MAIPGL (0x06)
#define MAIPGH (0x07)
#define MACLCON1 (0x08)
#define MACLCON2 (0x09)
#define MAMXFLL (0x0A)
#define MAMXFLH (0x0B)
#define MICMD (0x12)
#define MIREGADR (0x14)
#define MIWRL (0x16)
#define MIWRH (0x17)
#define MIRDL (0x18)
#define MIRDH (0x19)

#define MACON1_TXPAUS  (0x08)
#define MACON1_RXPAUS  (0x04)
#define MACON1_PASSALL (0x02)
#define MACON1_MARXEN  (0x01)

#define MACON3_PADCFG  (0xE0)
#define MACON3_TXCRCEN (0x10)
#define MACON3_PHDREN  (0x08)
#define MACON3_HFRMEN  (0x04)
#define MACON3_FRMLNEN (0x02)
#define MACON3_FULDPX  (0x01)

#define MACON4_DEFER   (0x40)
#define MACON4_BPEN    (0x20)
#define MACON4_NOBKOFF (0x10)

#define MICMD_MIISCAN  (0x02)
#define MICMD_MIIRD    (0x01)

// Bank 3

#define MAADR5 (0x00)
#define MAADR6 (0x01)
#define MAADR3 (0x02)
#define MAADR4 (0x03)
#define MAADR1 (0x04)
#define MAADR2 (0x05)
#define EBSTSD (0x06)
#define EBSTCON (0x07)
#define EBSTCSL (0x08)
#define EBSTCDH (0x09)
#define MISTAT (0x0A)
#define EREVID (0x12)
#define ECOCON (0x15)
#define EFLOCON (0x17)
#define EPAUSL (0x18)
#define EPAUSH (0x19)

#define MISTAT_NVALID   (0x04)
#define MISTAT_SCAN     (0x02)
#define MISTAT_BUSY     (0x01)

// Phy Regs

#define PHCON1 (0x00)
#define PHSTAT1 (0x01)
#define PHID1 (0x02)
#define PHID2 (0x03)
#define PHCON2 (0x10)
#define PHSTAT2 (0x11)
#define PHIE (0x12)
#define PHIR (0x13)
#define PHLCON (0x14)

#define PHCON1_PRST    (0x8000)
#define PHCON1_PLOOPBK (0x4000)
#define PHCON1_PPWRSV  (0x0800)
#define PHCON1_PDPXMD  (0x0100)

// Channel parameters

#define TX_SIZE (0x0C00)
#define MEM_HIGH (0x1FFF)
#define RX_END (MEM_HIGH - TX_SIZE)
#define TX_START (RX_END + 1)
#define TX_BANK2 (TX_START + (TX_SIZE / 2))

void set_rx_range(int start, int end);

void set_receive_enable(int receive);

void ntoh_eth(eth_packet* packet);
void hton_eth(eth_packet* packet);

#endif
