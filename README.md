# PiSafeTransfer
## File transfer over ethernet for Raspberry Pi with ENC28J60 controlled by GPIO

The ENC28J60 is a basic ethernet board that can be controlled via the GPIO pins from the Raspberry Pi.

By using this board, ethernet packets can be sent and received without interacting with the OS network stack.
The ethernet port on the ENC28J60 is fully under the control of the software.

Ethernet can be used to transfer files to and from an offline computer without the risk
of malware exploiting a vulnerability in the network stack of the OS on the Pi.

This still allows malware to exploit vulnerabilities in this software.
To combat that, this software is kept as simple as possible to allow easy review.

The software includes the following parts

*   SPI driver that communicates with the ENC28J60 via GPIO bit-bashing
*   ENC28J60 driver that allows sending and receiving ethernet packets
*   TCP stack that allows a single TCP connection at a time
*   Webserver that allows uploading and downloading via http

Each part contains only the minimum required for operation.
