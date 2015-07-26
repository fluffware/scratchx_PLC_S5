#include <mcs51reg.h>
#include <stdint.h>
#include <ext_io.h>
#include <int_io.h>
#include <adc.h>
#include <dac.h>
#include <rtc.h>
#include <serial.h>
#include <stdio.h>
#include <sys/pt.h>
#include <string.h>

static volatile __data uint16_t t0_count = 0;

static void
timer0_isr(void) __interrupt(1)
{
  if (++t0_count >= 150) t0_count = 0;
}

static void
serial1_isr(void) __interrupt(16);

static void
timer_init(void)
{
  TMOD = (TMOD & 0xf0) | 0x0; /* Mode 1 150Hz clock */
  TL0 = 0;
  TH0 =0;
  TF0 = 0;
  ET0 = 1;
  TR0 = 1;
}

static void
wd_reset(void)
{
  P3_4 = 0;
  P3_4 = 1;
}

__pdata static uint16_t v;

static __xdata struct pt sub_parse_pt;
PT_THREAD(parse_hex(char c))
{
  PT_BEGIN(&sub_parse_pt);
  v = 0;
  while(1) {
    if (c>= '0' && c <= '9') {
      v = (v<<4) | (c-'0');
    } else if (c >= 'a' && c <= 'f') {
      v = (v<<4) | (c-('a'-10));
    } else if (c >= 'A' && c <= 'F') {
      v = (v<<4) | (c-('A'-10));
    } else {
      break;
    }
    PT_YIELD(&sub_parse_pt);
  }
  PT_END(&sub_parse_pt);
}

PT_THREAD(skip_white(char c))
{
  PT_BEGIN(&sub_parse_pt);
  while(c == ' ' || c == '\t') {
    PT_YIELD(&sub_parse_pt);
  }
  PT_END(&sub_parse_pt);
}

static __xdata char token[10];

PT_THREAD(parse_token(char c))
{
  static uint8_t len;
  PT_BEGIN(&sub_parse_pt);
  len = 0;
  while((c >= '0' && c <= '9') 
	|| (c >= 'a' && c <= 'z')
	|| (c >= 'A' && c <= 'Z')) {
    if (len < (sizeof(token)-1)) {
      token[len++] = c;
    }
    PT_YIELD(&sub_parse_pt);
  }
  token[len] = '\0';
  PT_END(&sub_parse_pt);
}




static __xdata struct pt parse_pt;

#define SKIP_TO_EOL \
  while(c != '\n' && c!='\r') PT_YIELD(&parse_pt)

#define SKIP_WHITE while(c == ' ' || c =='\t') PT_YIELD(&parse_pt)

static int_output_state = 0x0000;

PT_THREAD(parse_thread(char c))
{
  static uint16_t addr;
  PT_BEGIN(&parse_pt);
  while(1) {
    while(c == '\n' || c=='\r') PT_YIELD(&parse_pt); 
    SKIP_WHITE;
    PT_SPAWN(&parse_pt, &sub_parse_pt, parse_token(c));
    if (strcmp(token, "din") == 0) {
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      SKIP_TO_EOL;
      if (v == 32) {
	printf("%02x\n", int_io_read() & 0xff);
      } else if (v == 33) {
	printf("%02x\n", int_io_read() >> 8);
      }
    } else if (strcmp(token, "dout") == 0) {
      static uint8_t dout;
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      addr = v;
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      dout = v;
      SKIP_WHITE;
      v = 0xff; /* Default mask */
      if (c != '\n' && c != '\r') {
	PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      }
      SKIP_TO_EOL;
      if (addr == 32 || addr == 33) {
	uint8_t r;
	if (addr == 32) {
	  int_output_state = (int_output_state & ~v) | (dout & v);
	} else {
	  int_output_state = (int_output_state & ~(v<<8)) | ((dout & v)<<8);
	}
	int_io_write(int_output_state);
	if (addr == 32) {
	  r = int_output_state;
	} else {
	  r = int_output_state>>8;
	}
	printf("%02x\n", r);
      }
    } else if (strcmp(token, "ain") == 0) {
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      SKIP_TO_EOL;
      if (v < 8) {
	int16_t r = adc_get(v);
	printf("%04x\n", r);
      }
    } else if (strcmp(token, "aout") == 0) {
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      addr = v;
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      SKIP_TO_EOL;
      if (addr == 0) {
	dac_set(v);
      }
    } else {
      SKIP_TO_EOL;
      printf("?\n");
    }
    PT_YIELD(&parse_pt);
  }
  PT_END(&parse_pt);
}

static void
parse_init(void)
{
  PT_INIT(&parse_pt);
}

static void
parse_input(char c)
{
  PT_SCHEDULE(parse_thread(c));
}

int 
main()
{
  int_io_init();
  timer_init();
  init_serial_1();
  parse_init();
  EAL = 1;
  while(1) {
    wd_reset();
#if 0
    if (t0_count > 150/2) {
      P1_3 = 0;
    } else {
      P1_3 = 1;
    }
#endif
    {
      char c = getchar();
      if (c) {
	parse_input(c);
      }
    }
  }
}
