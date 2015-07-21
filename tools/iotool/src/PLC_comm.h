#ifndef __PLC_COMM_H__BB0GSNEL8Z__
#define __PLC_COMM_H__BB0GSNEL8Z__


#include <glib-object.h>
#include <gio/gio.h>


#define PLC_COMM_ERROR (PLC_comm_error_quark())

#define PLC_COMM_TYPE                  (PLC_comm_get_type ())
#define PLC_COMM(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PLC_COMM_TYPE, PLCComm))
#define IS_PLC_COMM(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PLC_COMM_TYPE))
#define PLC_COMM_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), PLC_COMM_TYPE, PLCCommClass))
#define IS_PLC_COMM_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), PLC_COMM_TYPE))
#define PLC_COMM_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), PLC_COMM_TYPE, PLCCommClass))

typedef struct _PLCComm PLCComm;
typedef struct _PLCCommClass PLCCommClass;

typedef struct _PLCCmd PLCCmd;
struct _PLCCmd
{
  gchar *cmd;
  gchar *reply;
};

GType
PLC_comm_get_type(void);

PLCComm *
PLC_comm_new(GIOStream *io, GError **err);

gboolean
PLC_comm_send(PLCComm *comm,  const gchar *cmd, GError **err);

#endif /* __PLC_COMM_H__BB0GSNEL8Z__ */
