#ifndef __INT_IO_H__NVNPFRN6VE__
#define __INT_IO_H__NVNPFRN6VE__
#include <stdint.h>


struct IIOData
{
  uint8_t in[2];
  uint8_t out[2];
};

void
int_io_init();

void
int_io_write(uint16_t d);

uint16_t
int_io_read(void);

#endif /* __INT_IO_H__NVNPFRN6VE__ */
