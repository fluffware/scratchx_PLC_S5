#include <mcs51reg.h>
#include <stdint.h>
#include <stdio.h>
#include <ext_io.h>
#include <int_io.h>
#include <rtc.h>

#ifdef DEST_ROM
/* Build jumptable into RAM if compiling for ROM */
#define ISR(n) void isr_##n(void) __interrupt (n)
ISR(0);
ISR(1);
ISR(2);
ISR(3);
ISR(4);
ISR(5);
ISR(6);
ISR(7);
ISR(8);
ISR(9);
ISR(10);
ISR(11);
ISR(12);
ISR(13);
ISR(14);
ISR(15);
ISR(16);
ISR(17);
ISR(18);
ISR(19);
#endif

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

uint16_t
get_dec(void)
{
  uint16_t v=0;
  while(1) {
    char c = peekchar();
    if (c >= '0' && c <= '9') {
      v = (c-(uint8_t)'0') + (v * 10);
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

static char
skip_to_eol(void) {
  char c;
  while(1) {
    c = getchar();
    if (c == '\n' || c == '\r') {
      return c;
    }
  }
}

static char
skip_line(void) {
  char c;
  while(1) {
    c = peekchar();
    if (c == '\n' || c == '\r') {
      return c;
    }
    getchar();
  }
}

static void
wd_reset(void)
{
  P3_4 = 0;
  P3_4 = 1;
}

static __xdata struct EIOConfig eio_config;

static void
parse_ext_io_cmd(void)
{
  char c;
  c = peekchar();
  if (c == '\n' || c=='\r') return; /* Empty line*/
  c = getchar();
  if (c == 'c') {
    uint8_t s;
    skip_line();
    if ((P8 & 0x02) == 0) {
      printf("Set switch to RUN");
      return;
    }
    wd_reset();
    ext_io_read_config(&eio_config);
    printf("Found %d slots\n", eio_config.nslots);
    for (s = 0; s < eio_config.nslots; s++) {
      printf(" %02x", eio_config.slots[s]); 
    }
  } else if (c == 'r') {
    uint8_t s;
    skip_line();
    if ((P8 & 0x02) == 0) {
      printf("Set switch to RUN");
      return;
    }
    wd_reset();
    ext_io_read_config(&eio_config);
    wd_reset();
    ext_io_read(&eio_config);
    for (s = 0; s < eio_config.nslots; s++) {
      printf(" %02x", eio_config.slots[s]); 
    }
  }
}

static void
parse_int_io_cmd(void)
{
  char c;
  c = peekchar();
  if (c == '\n' || c=='\r') return; /* Empty line*/
  c = getchar();
  if (c == 'w') {
    uint16_t d;
    skip_white();
    d = get_hex();
    skip_line();
    if ((P8 & 0x02) == 0) {
      printf("Set switch to RUN");
      return;
    }
    wd_reset();
    int_io_write(d);
  } else if (c == 'r') {
    uint16_t d;
    skip_line();
    if ((P8 & 0x02) == 0) {
      printf("Set switch to RUN");
      return;
    }
    wd_reset();
    d = int_io_read();
    printf("%04x", d);
  }
}

__xdata struct RTCTime rtc_time;

static void
print_time(void)
{
  printf("%02d:",  rtc_time.hour);
  printf("%02d:", rtc_time.min);
  printf("%02d ", rtc_time.sec);
  printf("%02d-", rtc_time.year);
  printf("%02d-", rtc_time.month);
  printf("%02d ", rtc_time.day);
  printf("wd %d", rtc_time.wday);
}

static void
parse_rtc_cmd(void)
{
  char c;
  c = peekchar();
  if (c == '\n' || c=='\r') return; /* Empty line*/
  c = getchar();
  if (c == 'r') {
    skip_line();
    rtc_read(&rtc_time);
    print_time();
  } else if (c == 't') {
    rtc_read(&rtc_time);
    skip_white();
    rtc_time.hour = get_dec();
    if (getchar() != ':') return;
    rtc_time.min = get_dec();
    if (getchar() != ':') return;
    rtc_time.sec = get_dec();
    skip_line();
    rtc_init();
    rtc_set(&rtc_time);
    print_time();
  } else if (c == 'd') {
    rtc_read(&rtc_time);
    skip_white();
    rtc_time.year = get_dec();
    if (getchar() != '-') return;
    rtc_time.month = get_dec();
    if (getchar() != '-') return;
    rtc_time.day = get_dec();
    skip_line();
    rtc_init();
    rtc_set(&rtc_time);
    print_time();
  } else if (c == 'w') {
    rtc_read(&rtc_time);
    skip_white();
    rtc_time.wday = get_dec();
    rtc_init();
    rtc_set(&rtc_time);
    print_time();
  }
}

typedef (*GenericCall)();

int main()
{
  char  prev_eol = '\0';
  init_serial_1();
  ext_io_init();
  int_io_init();
  printf("PLC95 command tool\n>");
  while(1) {
    do {
      char c;
      /* Skip LF if the previos line ended with CR */
      if (prev_eol == '\r' && peekchar() == '\n') {
	getchar();
      }
      skip_space();
      c = peekchar();
      if (c == '\n' || c=='\r') break; /* Empty line*/
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
      } else if (c == '!') {
	parse_ext_io_cmd();
      } else if (c == ':') {
	parse_rtc_cmd();
      } else if (c == '@') {
	parse_int_io_cmd();
      } else {
	printf("\nUnknown command");
      }
    } while(0);
    prev_eol = skip_to_eol();
    putchar('\n');
    putchar('>');
  }
}
