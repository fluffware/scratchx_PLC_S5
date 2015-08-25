#include <mcs51reg.h>
#include <stdint.h>
#include <stdio.h>
#include <ext_io.h>
#include <int_io.h>
#include <rtc.h>
#include <zmodem.h>
#include <ihex.h>
#include <crc.h>

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

#define TICKS_PER_SECOND 150

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

/* Returns -1 on timeout */
int16_t getchar_timeout(uint16_t ticks)
{
  uint8_t c;
  if (peek_buf) {
    c = peek_buf;
    peek_buf = 0;
    return c;
  } else {
    TF0 = 0;
    while (ticks > 0) {
      while((S1CON & RI1_MASK) == 0 && !TF0); /* Wait for received data */
      if ((S1CON & RI1_MASK) != 0) {
	c = S1BUF;
	S1CON &= (uint8_t)~RI1_MASK;
	return c;
      } else {
	TF0 = 0;
	ticks--;
      }
    }
    return -1;
  }
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

  /* Setup timer 0 for timed reads */
  TMOD = (TMOD & 0xf0) | 0x0; /* Mode 1 150Hz clock */
  TL0 = 0;
  TH0 =0;
  TF0 = 0;
  ET0 = 0;
  TR0 = 1;
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

uint16_t
calc_crc(__xdata uint8_t *p, uint16_t l)
{
  uint16_t crc = 0xffff;
  while(l-- > 0) { 
    uint8_t c = *p++;
    crc = updcrc(c, crc);
  }
  return crc;
}

typedef (*GenericCall)();

/* Place CRC last in program RAM */

#define FLAG_AUTORUN 0x01

#define MAGIC1 0xfa
#define MAGIC2 0x50

struct footer_t {
  uint8_t flags;
  uint8_t magic1;
  uint8_t magic2;
  uint8_t crc_hi;
  uint8_t crc_lo;
};

static __xdata __at (0x10000 - sizeof(struct footer_t)) struct footer_t footer;
static __xdata __at 0x8000 uint8_t prg_ram;
#define PRG_RAM_SIZE 0x8000

static void
receive_file(void)
{
  ihex_init();
  zmodem_init();
  while(zmodem_input(getchar()));
  if (ihex_parse_error != IHEX_DONE) {
    /* Consume any extra bytes sent */
    while(getchar_timeout(TICKS_PER_SECOND*2) >= 0);
  }
  printf("\n%02x",ihex_parse_error);
}

static void
parse_transfer_cmd(void)
{
  char c;
  c = peekchar();
  if (c == '\n' || c=='\r') return; /* Empty line*/
  c = getchar();
  if (c == 'c') {
    uint16_t crc;
    footer.flags = 0;
    if (peekchar() == '+') {
      footer.flags += FLAG_AUTORUN;
      getchar();
    }
    skip_line();
    footer.magic1 = MAGIC1;
    footer.magic2 = MAGIC2;
    footer.crc_hi = 0;
    footer.crc_lo = 0;
    crc = calc_crc(&prg_ram, PRG_RAM_SIZE);
    footer.crc_hi = crc >> 8;
    footer.crc_lo = crc;
  } else if (c == 'R') {
    skip_line();
    if (footer.magic1 == MAGIC1 && footer.magic2 == MAGIC2) {
      if (calc_crc(&prg_ram, PRG_RAM_SIZE) == 0) {
	((GenericCall)&prg_ram)();
      } else {
	printf("CRC error");
      }
    } else {
      printf("No program footer found");
    }
  } else if (c == '?') {
    skip_line();
    printf("%02x", ihex_parse_error);
  }
}
void
zmodem_file_start(void)
{
  P1_3 = 0;
}

void
zmodem_file_end(void)
{
  P1_3 = 1;
}

void
zmodem_send(uint8_t c)
{
  putchar(c);
}



int main()
{
  char  prev_eol = '\0';
  init_serial_1();
  ext_io_init();
  int_io_init();
  if ((P8 & 1) == 0) {
    if (footer.magic1 == MAGIC1 && footer.magic2 == MAGIC2) {
      if (footer.flags & FLAG_AUTORUN) {
	if (calc_crc(&prg_ram, PRG_RAM_SIZE) == 0) {
	  ((GenericCall)&prg_ram)();
	} else {
	  printf("Program RAM CRC error\n");
	}
      }
    }
  }
  printf("PLC95 command tool\n>");
  while(1) {
    do {
      char c;
      /* Skip LF if the previos line ended with CR */
      if (prev_eol == '\r' && peekchar() == '\n') {
	getchar();
      }
      prev_eol = '\0';
      skip_space();
      c = peekchar();
      if (c == '\n' || c=='\r') break; /* Empty line*/
      c = getchar();
      if (c == 'r') {
	__data uint8_t *p;
	uint16_t l;
	if (peekchar() == 'z') {
	  prev_eol = skip_to_eol();
	  receive_file();
	} else {
	  p = (__data uint8_t*)get_hex();
	  if (skip_space()) break;
	  l = get_hex();
	  while(l > 0) {
	    printf(" %02x", *p);
	    p++;
	    l--;
	  }
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
      } else if (c == '%') {
	parse_transfer_cmd();
      } else {
	printf("\nUnknown command %c", c);
      }
    } while(0);
    if (prev_eol == '\0') {
      prev_eol = skip_to_eol();
    }
    putchar('\n');
    putchar('>');
  }
}
