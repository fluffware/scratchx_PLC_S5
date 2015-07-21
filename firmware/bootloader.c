#include <mcs51reg.h>
#include <stdint.h>
#include <stdio.h>
static int a = 7;
#define BPS 38400
#define F_OSC 14745600
#define S0REL (1024 - (F_OSC/((long)64*BPS)))
#define S1REL (1024 - (F_OSC / ((long)32 * BPS)))

#define TI1_MASK 0x02
#define RI1_MASK 0x01

static char peek_buf = 0;
char getchar(void)
{
  char c;
  if (peek_buf) {
    c = peek_buf;
    peek_buf = 0;
  } else {
    while((S1CON & RI1_MASK) == 0); /* Wait for received data */
    c = S1BUF;
    S1CON &= (uint8_t)~RI1_MASK;
  }
  return c;
}

char
peekchar()
{
  if (peek_buf) return peek_buf;
  peek_buf = getchar();
  return peek_buf;
}

void
putchar(char c)
{
  while((S1CON & TI1_MASK) == 0); /* Wait for transmitter to become ready */
  S1BUF = c;
  S1CON &= (uint8_t)~TI1_MASK;
}

void
init_serial_1(void)
{
  S1CON = 0x92; /* Serial mode B, 8bit async. Enable receiver */
  S1RELH = S1REL>>8;
  S1RELL = S1REL;
}

static uint16_t
get_hex(void)
{
  uint16_t v=0;
  while(1) {
    char c = peekchar();
    if (c >= '0' && c <= '9') {
      v = (v<<4) | (c-'0');
    } else if (c >= 'a' && c <= 'f') {
      v = (v<<4) | (c-('a'-10));
    } else if (c >= 'A' && c <= 'F') {
      v = (v<<4) | (c-('A'-10));
    } else {
      return v;
    }
    getchar();
  }
}

static void
skip_white(void)
{
  while(1) {
    char c = peekchar();
    if ( c != ' ' && c != '\n' && c != '\r') break;
    getchar();
  }
}

static int
skip_space(void)
{
  char c;
  while(1) {
    c = peekchar();
    if ( c != ' ') break;
    getchar();
  }
  return c == '\n' || c == '\r';
}

static void
skip_to_eol(void) {
  char c;
  while(1) {
    c = getchar();
    if (c == '\n' || c == '\r') {
      return;
    }
  }
}

typedef (*GenericCall)();

int main()
{
  /*
  __xdata uint8_t *xp;
  __sfr  *sp;
  */
  init_serial_1();
  printf("Started\n");
  while(1) {
    do {
      char c;
      skip_white();
      c = getchar();
      if (c == 'r') {
	__data uint8_t *p;
	uint16_t l;
	p = (__data uint8_t*)get_hex();
	if (skip_space()) break;
	l = get_hex();
	while(l > 0) {
	  printf(" %02x", *p);
	  p++;
	  l--;
	}
      } else if (c == 'i') {
	uint8_t p;
	uint8_t v = 0;
	if (skip_space()) break;
	p = get_hex();
	switch(p) {
	case 0:
	  v = P0;
	  break;
	case 1:
	  v = P1;
	  break;
	case 2:
	  v = P2;
	  break;
	case 3:
	  v = P3;
	  break;
	case 4:
	  v = P4;
	  break;
	case 5:
	  v = P5;
	  break;
	case 6:
	  v = P6;
	  break;
	case 7:
	  v = P7;
	  break;
	case 8:
	  v = P8;
	  break;
	}
	printf(" %02x", v);
      } else if (c == 'o') {
	uint8_t p;
	uint8_t v = 0;
	if (skip_space()) break;
	p = get_hex();
	if (skip_space()) break;
	v = get_hex();
	switch(p) {
	case 0:
	  P0 = v;
	  break;
	case 1:
	  P1 = v;
	  break;
	case 2:
	  P2 = v;
	  break;
	case 3:
	  P3 = v;
	  break;
	case 4:
	  P4 = v;
	  break;
	case 5:
	  P5 = v;
	  break;
	case 6:
	  P6 = v;
	  break;
	}
      } else if (c == 'R') {
	__xdata uint8_t *p;
	uint16_t l;
	p = (__xdata uint8_t*)get_hex();
	if (skip_space()) break;
	l = get_hex();
	while(l > 0) {
	  printf(" %02x", *p);
	  p++;
	  l--;
	}
      } else if (c == 'W') {
	__xdata uint8_t *p;
	uint8_t v;
	p = (__xdata uint8_t*)get_hex();
	while(1) {
	  if (skip_space()) break;
	  v = get_hex();
	  *p++ = v;
	}
      } else if (c == 'c') { /* Call a subroutine */
	uint16_t a;
	a = get_hex();
	((GenericCall)a)();
      }
    } while(0);
    skip_to_eol();
    putchar('\n');
    putchar('>');
  }
}
