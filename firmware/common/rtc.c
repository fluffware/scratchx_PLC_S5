#include "rtc.h"
#include <mcs51reg.h>

#define IO P4_7 = 0
#define MEM P4_7 = 1

#define HOLD 0x01
#define BUSY 0x02
#define IRQ_FLAG 0x04
#define ADJ30S 0x08

#define MASK 0x01
#define ITRPT_STND 0x02
#define t0 0x04
#define t1 0x04

#define RESET 0x01
#define STOP 0x02
#define H24_12 0x04
#define TEST 0x08

#define BASE 0x2000

__xdata __at (BASE + 0x00) volatile uint8_t reg_S1;
__xdata __at (BASE + 0x01) volatile uint8_t reg_S10;
__xdata __at (BASE + 0x02) volatile uint8_t reg_MI1;
__xdata __at (BASE + 0x03) volatile uint8_t reg_MI10;
__xdata __at (BASE + 0x04) volatile uint8_t reg_H1;
__xdata __at (BASE + 0x05) volatile uint8_t reg_H10;
__xdata __at (BASE + 0x06) volatile uint8_t reg_D1;
__xdata __at (BASE + 0x07) volatile uint8_t reg_D10;
__xdata __at (BASE + 0x08) volatile uint8_t reg_MO1;
__xdata __at (BASE + 0x09) volatile uint8_t reg_MO10;
__xdata __at (BASE + 0x0a) volatile uint8_t reg_Y1;
__xdata __at (BASE + 0x0b) volatile uint8_t reg_Y10;
__xdata __at (BASE + 0x0c) volatile uint8_t reg_W;
__xdata __at (BASE + 0x0d) volatile uint8_t reg_CD;
__xdata __at (BASE + 0x0e)volatile uint8_t reg_CE;
__xdata __at (BASE + 0x0f) volatile uint8_t reg_CF;

void
rtc_read(__xdata struct RTCTime *time) __critical
{
  uint8_t v;
  IO;
  do {
    reg_CD = IRQ_FLAG;
    reg_CD = HOLD | IRQ_FLAG;
  } while(reg_CD & BUSY);
  v = reg_S1 + 10 * reg_S10;
  MEM;
  time->sec = v;
  IO;
  v = reg_MI1 + 10 * reg_MI10;
  MEM;
  time->min = v;
  IO;
  v = reg_H1 + 10 * reg_H10;
  MEM;
  time->hour = v;
  IO;
  v = reg_D1 + 10 * reg_D10;
  MEM;
  time->day = v;
  IO;
  v = reg_MO1 + 10 * reg_MO10;
  MEM;
  time->month = v;
  IO;
  v = reg_Y1 + 10 * reg_Y10;
  MEM;
  time->year = v;
  IO;
  v = reg_W;
  MEM;
  time->wday = v;
  IO;
  reg_CD = IRQ_FLAG;
  MEM;
}

void
rtc_init(void) __critical
{
  IO;
  reg_CF = H24_12;
  reg_CE = MASK | ITRPT_STND;
  reg_CD = 0;
  
  do {
    reg_CD = IRQ_FLAG;
    reg_CD = HOLD | IRQ_FLAG;
  } while(reg_CD & BUSY);

  reg_CF = STOP | RESET | H24_12;
  reg_CF = H24_12;
  MEM;
}

#define SET_DEC_REG(va, reg)			\
  do {						\
    uint8_t v = (va);				\
    IO;						\
    reg ## 1 = v % 10;				\
    reg ## 10 = v / 10;				\
    MEM;					\
  } while(0)

void
rtc_set(__xdata struct RTCTime *time) __critical
{
  uint8_t v;
  IO;
  do {
    reg_CD = IRQ_FLAG;
    reg_CD = HOLD | IRQ_FLAG;
  } while(reg_CD & BUSY);
  MEM;
  SET_DEC_REG(time->sec, reg_S);
  SET_DEC_REG(time->min, reg_MI);
  SET_DEC_REG(time->hour, reg_H);
  SET_DEC_REG(time->day, reg_D);
  SET_DEC_REG(time->month, reg_MO);
  SET_DEC_REG(time->year, reg_Y);
  v =  time->wday;
  IO;
  reg_W = v;
  reg_CD = IRQ_FLAG;
  MEM;
}
  
