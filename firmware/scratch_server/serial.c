#include "serial.h"
#include <mcs51reg.h>
#include <stdint.h>

#define BPS 38400
#define F_OSC 14745600
#define S0REL (1024 - (F_OSC/((long)64*BPS)))
#define S1REL (1024 - (F_OSC / ((long)32 * BPS)))

#define TI1_MASK 0x02
#define RI1_MASK 0x01
#define ES1_MASK 0x01

/* Ring buffer for reading */
static __xdata uint8_t rx_head = 0; 
static __xdata uint8_t rx_tail = 0;
static __xdata uint8_t rx_buffer[256];

/* Ring buffer for writing */
static __xdata uint8_t tx_head = 0;
static __xdata uint8_t tx_tail = 0;
static __xdata uint8_t tx_buffer[256];

void
serial1_isr(void) __interrupt(16)
{
  if (S1CON & TI1_MASK) {
    if (tx_head != tx_tail) {
      tx_tail++;
      if (tx_head != tx_tail) {
	S1BUF = tx_buffer[tx_tail];
      }
    }
    S1CON &= ~TI1_MASK;
  }
  if (S1CON & RI1_MASK) {
    if (rx_head + 1 != rx_tail) {
      rx_buffer[rx_head] = S1BUF;
      rx_head++;
    }
    S1CON &= ~RI1_MASK;
  }
}

void
putchar(char c) __critical
{
  if (tx_head + 1 != tx_tail) {
    if (tx_head == tx_tail) {
      S1BUF = c;
    }
    tx_buffer[tx_head] = c;
    tx_head++;
  }
}

char
getchar(void)
{
  if (rx_head != rx_tail) {
    return rx_buffer[rx_tail++];
  } else {
    return '\0';
  }
}

void
init_serial_1(void)
{
  S1CON = 0x92; /* Serial mode B, 8bit async. Enable receiver */
  S1RELH = S1REL>>8;
  S1RELL = S1REL;
  S1CON &= ~(TI1_MASK | RI1_MASK);
  IEN2 |= ES1_MASK;
}
