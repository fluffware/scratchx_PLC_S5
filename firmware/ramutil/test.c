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

void
putchar(char c)
{
  while((S1CON & TI1_MASK) == 0); /* Wait for transmitter to become ready */
  S1BUF = c;
  S1CON &= (uint8_t)~TI1_MASK;
}

#define EIO_DATA_OUT P5_3
#define EIO_DATA_IN P5_2
#define EIO_INIT P5_4
#define EIO_LOAD P5_5
#define EIO_CLOCK P5_6
#define EIO_RUN P5_7

#if 0
uint8_t
send_bits(uint8_t b)
{
  uint8_t i;
  for (i = 0; i < 5; i++) {
    EIO_DATA_OUT = b & 0x10;
    EIO_CLOCK = 1;
    b<<=1;
    EIO_CLOCK = 0;
    b |= EIO_DATA_IN;
  }
  return b;
}
#else
#define SHIFT_BIT				\
  rrc a						\
  mov _P5_3,c					\
    setb _P5_6					\
    mov c,_P5_2					\
    clr _P5_6

uint8_t
send_bits(uint8_t b) __naked
{
  (void)b;
  __asm
    mov a,dpl
    clr c
    SHIFT_BIT
    SHIFT_BIT
    SHIFT_BIT
    SHIFT_BIT
    SHIFT_BIT
    rrc a
    swap a
    rl a
    mov dpl,a
    ret
  __endasm;
}
#endif

static void
send_latch_in(void)
{
  EIO_CLOCK = 1;
  EIO_LOAD = 1;
  EIO_CLOCK = 0;
  EIO_LOAD = 0;
}

static void
send_latch_out(void)
{
  EIO_CLOCK = 0;
  EIO_LOAD = 1;
  EIO_CLOCK = 1;
  EIO_LOAD = 0;
}

void
init_ext_io(void)
{
  P5_3 = 0;
  P5_4 = 0;
  P5_5 = 0;
  P5_6 = 0;
  P5_7 = 0;
}

static void
wd_reset(void)
{
  P3_4 = 0;
  P3_4 = 1;
}

#define MAX_BUS_SLOTS 100
int
main()
{
  init_ext_io();
  wd_reset();
  printf("Hello world!\n");
  while(1) {
    uint8_t i;
    wd_reset();
    EIO_RUN = 0;
    EIO_INIT = 1;
    send_bits(0x10);
    for (i = 0; i < MAX_BUS_SLOTS; i++) {
      uint8_t d = send_bits(0x00);
      /* printf(" %02x", d); */
      if (d == 0x10) break;
    }
    if (i == MAX_BUS_SLOTS) {
      puts("No reply from bus");
    } else {
      uint8_t l;
      printf("Found %d slots\n",i);
      send_latch_in();
      printf(" %02x", send_bits(0x15));
      printf(" %02x", send_bits(0x08));
      for (l = 0; l < i+1; l++) {
	printf(" %02x", send_bits(0x00));
      }
      putchar('\n');

      EIO_INIT = 0;
      while(1) {
	wd_reset();
	send_latch_in();
	printf(" %02x", send_bits(0x15));
	printf(" %02x", send_bits(0x02));
	for (l = 0; l < i+1; l++) {
	  printf(" %02x", send_bits(0x12));
	}
	putchar('\n');
	send_latch_out();
      }
    }

    EIO_INIT = 0;
    EIO_RUN = 1;
  }
}
