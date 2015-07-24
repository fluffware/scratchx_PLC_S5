#include "adc.h"
#include <mcs51reg.h>

#define BSY_MASK 0x10
int16_t
adc_get(uint8_t channel)
{
  ADCON0 = (channel & 0x07);
  DAPR = 0x00;
  while(ADCON0 & BSY_MASK);
  return ADDAT * 39 + ADDAT/16;
  
}
