

#include <glib-object.h>
#include <gio/gio.h>


#define PLC_IO_COMM_ERROR (plc_io_comm_error_quark())

#define PLC_IO_COMM_TYPE                  (plc_io_comm_get_type ())
#define PLC_IO_COMM(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PLC_IO_COMM_TYPE, PlcIoComm))
#define IS_PLC_IO_COMM(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PLC_IO_COMM_TYPE))
#define PLC_IO_COMM_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), PLC_IO_COMM_TYPE, PlcIoCommClass))
#define IS_PLC_IO_COMM_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), PLC_IO_COMM_TYPE))
#define PLC_IO_COMM_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), PLC_IO_COMM_TYPE, PlcIoCommClass))

typedef struct _PlcIoComm PlcIoComm;
typedef struct _PlcIoCommClass PlcIoCommClass;

typedef struct _PLCMsg PLCMsg;
struct _PLCMsg
{
  GError *err;
  guint8 *msg;
};

GType
plc_io_comm_get_type(void);

PlcIoComm *
plc_io_comm_new(GIOStream *io, GError **err);

/* Calculates CRC for cmd before sending */
gboolean
plc_io_comm_send(PlcIoComm *comm,  const guint8 *cmd, GError **err);
