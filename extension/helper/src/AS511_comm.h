#ifndef __AS511_COMM_H__2DQHIHEEUZ__
#define __AS511_COMM_H__2DQHIHEEUZ__

#include <glib-object.h>
#include <gio/gio.h>

#define AS511_CONNECTION_ERROR (AS511_connection_error_quark())
enum {
  AS511_CONNECTION_OK = 0,
  AS511_CONNECTION_ERROR_SHORT_READ,
  AS511_CONNECTION_ERROR_SHORT_WRITE,
  AS511_CONNECTION_ERROR_RECEIVED_NAK,
  AS511_CONNECTION_ERROR_UNEXPECTED_REPLY,
  AS511_CONNECTION_ERROR_BUFFER_OVERFLOW,
  AS511_CONNECTION_ERROR_SHORT_BLOCK
};

#define AS511_CONNECTION_TYPE                  (AS511_connection_get_type ())
#define AS511_CONNECTION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), AS511_CONNECTION_TYPE, AS511Connection))
#define IS_AS511_CONNECTION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AS511_CONNECTION_TYPE))
#define AS511_CONNECTION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), AS511_CONNECTION_TYPE, AS511ConnectionClass))
#define IS_AS511_CONNECTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), AS511_CONNECTION_TYPE))
#define AS511_CONNECTION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), AS511_CONNECTION_TYPE, AS511ConnectionClass))

typedef struct _AS511Connection AS511Connection;
typedef struct _AS511ConnectionClass AS511ConnectionClass;

GQuark
AS511_connection_error_quark();

AS511Connection *
AS511_connection_new(GIOStream *io);

#define AS511_ID_DB 1
#define AS511_ID_SB 2
#define AS511_ID_PB 4
#define AS511_ID_FB 8
#define AS511_ID_OB 48
#define AS511_ID_FX 76
#define AS511_ID_DX 144

void
AS511_connection_dir_async(AS511Connection *conn,
			   guint8 id,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data);

guint16 *
AS511_connection_dir_finish(AS511Connection *conn, 
			    GAsyncResult *result,
			    GError **error);

typedef struct _AS511SysPar AS511SysPar;
struct _AS511SysPar
{
  guint16 PAE;
  guint16 PAA;
  guint16 M;
  guint16 T;
  guint16 Z;
  guint16 IAData;
  guint8 PLCType;
};

void
AS511_connection_sys_par_async(AS511Connection *conn, 
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data);

AS511SysPar *
AS511_connection_sys_par_finish(AS511Connection *conn, 
				GAsyncResult *result,
				GError **error);

typedef struct _AS511ReadResult AS511ReadResult;

struct _AS511ReadResult
{
  guint16 first; /* Address of first byte */
  guint16 length; /* Length in bytes */
  guint8 *data;
};

void
AS511_connection_db_read_async(AS511Connection *conn,
			       guint16 first,
			       guint16 last,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data);

AS511ReadResult *
AS511_connection_db_read_finish(AS511Connection *conn, 
				GAsyncResult *result,
				GError **error);

void
AS511_connection_db_write_async(AS511Connection *conn,
				guint16 first,
				guint16 length, 
				const guint8 *data,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data);

gboolean
AS511_connection_db_write_finish(AS511Connection *conn, 
				GAsyncResult *result,
				GError **error);

#endif /* __AS511_COMM_H__2DQHIHEEUZ__ */
