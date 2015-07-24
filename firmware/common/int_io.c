#include "int_io.h"
#include <mcs51reg.h>

void
int_io_init()
{
  S0CON = 0; /* Mode 0 */
  P4_2 = 1;
  P4_3 = 1;
  P4_4 = 1;
}

#define SER_WRITE P4_3 = 0; /* Write to I/O */
#define SER_READ P4_3 = 1; /* Read from I/O */

#define SER_START P4_4 = 0
#define SER_END P4_4 = 1

#define SER_LOW P4_2 = 0
#define SER_HIGH P4_2 = 1

void
int_io_write(uint16_t d)
{
  SER_LOW;
  SER_WRITE;
  SER_START;

  TI0 = 0;
  S0BUF = d;

  while(!TI0); /* Wait for send to complete */

  SER_HIGH;
  TI0 = 0;
  S0BUF = d>>8;
  
  while(!TI0); /* Wait for send to complete */

  TI0 = 0;
  SER_END;

  SER_READ;
}


uint16_t
 int_io_read(void)
{
  uint16_t d;
  SER_LOW;
  SER_READ;
  SER_START;

  REN0 = 1;
  RI0 = 0;
  while(!RI0); /* Wait for receive to complete */
  d = S0BUF;


  SER_HIGH;
  
  RI0 = 0;
  
  while(!RI0); /* Wait for receive to complete */
  d |= S0BUF<<8;
  REN0 = 0;
  RI0 = 0;
  SER_END;
  return d;
}
  
