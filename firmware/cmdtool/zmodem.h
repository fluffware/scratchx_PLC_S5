#ifndef __ZMODEM_H__2486C1B1QC__
#define __ZMODEM_H__2486C1B1QC__
#include <stdint.h>

#define ZMODEM_RECEIVE_BUFFER_SIZE 1024

void
zmodem_init(void);

uint8_t
zmodem_input(uint8_t c);

/* Externally implemented function for sending replies */
extern void
zmodem_send(uint8_t c);

/* Externally implemented function called when a new file transfer is started*/
extern void
zmodem_file_start(void);

/* Externally implemented function called when a new file transfer ends*/
extern void
zmodem_file_end(void);

/* Externally implemented function called when a byte is received */
/* Return non-zero to signal error */
extern uint8_t
zmodem_received(uint8_t data);

#endif /* __ZMODEM_H__2486C1B1QC__ */
