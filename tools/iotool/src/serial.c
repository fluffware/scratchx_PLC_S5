#include "serial.h"
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <fcntl.h>
#include <errno.h>
#include <asm/termios.h>
#include <asm/ioctls.h>
#include <string.h>

#ifndef BOTHER
#define    BOTHER CBAUDEX
#endif
extern int ioctl(int d, int request, ...);

#define SERIAL_DEVICE_TYPE (serial_device_get_type())
#define SERIAL_DEVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SERIAL_DEVICE_TYPE, SerialDevice))

GQuark
serial_device_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("serial-device-error-quark");
  return error_quark;
}

typedef struct _SerialDevice SerialDevice;
typedef struct _SerialDeviceClass SerialDeviceClass;

struct _SerialDevice
{
  GIOStream parent_instance;
  int fd;
  GInputStream *input;
  GOutputStream *output;
};

struct _SerialDeviceClass
{
  GIOStreamClass parent_class;
  
  /* class members */
  
  /* Signals */
};

G_DEFINE_TYPE (SerialDevice, serial_device, G_TYPE_IO_STREAM)

static void
dispose(GObject *gobj)
{
  SerialDevice *ser = SERIAL_DEVICE(gobj);
  if (ser->fd >= 0) {
    close(ser->fd);
    ser->fd = -1;
  }
  G_OBJECT_CLASS(serial_device_parent_class)->dispose(gobj);
}

static void
finalize(GObject *gobj)
{
  SerialDevice *ser = SERIAL_DEVICE(gobj);
  g_clear_object(&ser->input);
  g_clear_object(&ser->output);
  G_OBJECT_CLASS(serial_device_parent_class)->finalize(gobj);
}
static GInputStream *get_input_stream(GIOStream *iostream);
static GOutputStream *get_output_stream(GIOStream *iostream);

static void
serial_device_class_init (SerialDeviceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GIOStreamClass *ioclass = G_IO_STREAM_CLASS(klass);
  gobject_class->dispose = dispose;
  gobject_class->finalize = finalize;
  ioclass->get_input_stream = get_input_stream;
  ioclass->get_output_stream = get_output_stream;
}

static void
serial_device_init(SerialDevice *ser)
{
  ser->fd = -1;
  ser->input = 0;
  ser->output = 0;
}

static GInputStream *
get_input_stream(GIOStream *iostream)
{
  SerialDevice *ser = SERIAL_DEVICE(iostream);
  return ser->input;
}

static GOutputStream *get_output_stream(GIOStream *iostream)
{
  SerialDevice *ser = SERIAL_DEVICE(iostream);
  return ser->output;
}

GIOStream *
serial_device_open(const char *device, unsigned int rate,
		   unsigned int bits, SerialDerviceParity parity, GError **err)
{
  struct termios2 settings;
  int fd;
  SerialDevice *ser;
  fd = open(device, O_RDWR);
  if (fd < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"open failed: %s", strerror(errno));
    return NULL;
  }
  if (ioctl(fd, TCGETS2, &settings) < 0) {
      g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		  "ioctl TCGETS2 failed: %s", strerror(errno));
    close(fd);
    return NULL;
  }
  settings.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | INPCK | ISTRIP
			| INLCR | IGNCR | ICRNL | IXON | PARMRK);
  settings.c_iflag |= IGNBRK | IGNPAR;
  settings.c_oflag &= ~OPOST;
  settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  settings.c_cflag &= ~(CSIZE | PARODD | CBAUD | PARENB);
  settings.c_cflag |= CS8 | BOTHER | CREAD;
  if (parity != ParityNone) {
    settings.c_cflag |= PARENB;
    if (parity == ParityOdd) {
      settings.c_cflag |= PARODD;
    }
  }
  settings.c_ispeed = rate;
  settings.c_ospeed = rate;
  if (ioctl(fd, TCSETS2, &settings) < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"ioctl TCSETS2 failed: %s", strerror(errno));
    close(fd);
    return NULL;
  }
  ser = g_object_new(SERIAL_DEVICE_TYPE, NULL);
  ser->fd = fd;
  ser->input = g_unix_input_stream_new(ser->fd, FALSE);
  ser->output = g_unix_output_stream_new(ser->fd, FALSE);
  return G_IO_STREAM(ser);
}
