#include "dac.h"
#include <mcs51reg.h>

#define CLK_EN P4_1=0
#define CLK_DIS P4_1=1
#define DAC_LOAD P4_0 = 0; P4_0=1


uint8_t
reverse_bits(uint8_t v)
{
  v = (v&0xaa) >>1 | (v&0x55) << 1;
  v = (v&0xcc) >>2 | (v&0x33) << 2;
  return (v&0xf0) >>4 | (v&0x0f) << 4;
}

void
dac_set(uint16_t v)
{
  CLK_EN;
  TI0 = 0;
  v = ((uint32_t)v * 4096) / 10000;
  S0BUF = reverse_bits(v>>8);
  while(!TI0); /* Wait for send to complete */
  TI0 = 0;
  S0BUF = reverse_bits(v);
  while(!TI0); /* Wait for send to complete */
  DAC_LOAD;
  TI0 = 0;
  CLK_DIS;
}
