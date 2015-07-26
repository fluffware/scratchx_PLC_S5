#ifndef __PLC_IO_SERVER_H__1I1XEIOGY9__
#define __PLC_IO_SERVER_H__1I1XEIOGY9__

#include <glib-object.h>
#include <websocket_server.h>
#include <PLC_IO_comm.h>

#define PLC_IO_SERVER_ERROR (plc_io_server_error_quark())
enum {
  PLC_IO_SERVER_ERROR_START_FAILED = 1,
  PLC_IO_SERVER_ERROR_INTERNAL
};

#define PLC_IO_SERVER_TYPE                  (plc_io_server_get_type ())
#define PLC_IO_SERVER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PLC_IO_SERVER_TYPE, PlcIoServer))
#define IS_PLC_IO_SERVER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PLC_IO_SERVER_TYPE))
#define PLC_IO_SERVER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), PLC_IO_SERVER_TYPE, PlcIoServerClass))
#define IS_PLC_IO_SERVER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), PLC_IO_SERVER_TYPE))
#define PLC_IO_SERVER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), PLC_IO_SERVER_TYPE, PlcIoServerClass))

typedef struct _PlcIoServer PlcIoServer;
typedef struct _PlcIoServerClass PlcIoServerClass;


PlcIoServer *
plc_io_server_new(PlcIoComm *comm);


#endif /* __PLC_IO_SERVER_H__1I1XEIOGY9__ */
