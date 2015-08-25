#ifndef __IHEX_H__3PYECF7X4E__
#define __IHEX_H__3PYECF7X4E__

#include <stdint.h>

#ifndef __SDCC
#define __xdata
#endif

#define IHEX_OK 0
#define IHEX_DONE 1
#define IHEX_ERROR_NOT_HEX_DIGIT 2
#define IHEX_ERROR_MISSING_COLON 3
#define IHEX_ERROR_CHECKSUM 4 
#define IHEX_ERROR_UNKNOWN_TYPE 5

extern __xdata uint8_t ihex_parse_error;

void
ihex_init(void);
#endif /* __IHEX_H__3PYECF7X4E__ */
