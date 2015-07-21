#ifndef __SERIAL_H__0KRZCEO37V__
#define __SERIAL_H__0KRZCEO37V__

#include <gio/gio.h>
enum SerialDerviceParity
  {
    ParityNone,
    ParityEven,
    ParityOdd
  };
typedef enum SerialDerviceParity SerialDerviceParity;
GIOStream *
serial_device_open(const char *device, unsigned int rate,
		   unsigned int bits, SerialDerviceParity parity, GError **err);


#endif /* __SERIAL_H__0KRZCEO37V__ */
