
#include "AS511_comm.h"

#define STX 0x02
#define ETX 0x03
#define EOT 0x04
#define ACK 0x06
#define DLE 0x10
#define NAK 0x15

GQuark
AS511_connection_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("AS511-connection-error-quark");
  return error_quark;
}

struct _AS511Connection
{
  GObject parent_instance;
  GIOStream *io;
  GInputStream *in;
  GOutputStream *out;
  GCancellable *cancel_io;
  GMutex io_mutex; /* Only one operation at a time */
};

struct _AS511ConnectionClass
{
  GObjectClass parent_class;
};
  
G_DEFINE_TYPE (AS511Connection, AS511_connection, G_TYPE_OBJECT)

static void
dispose(GObject *gobj)
{
  AS511Connection *conn = AS511_CONNECTION(gobj);
  g_mutex_lock(&conn->io_mutex);
  g_clear_object(&conn->io);
  g_clear_object(&conn->cancel_io);
  g_mutex_unlock(&conn->io_mutex);
}

static void
finalize(GObject *gobj)
{
  AS511Connection *conn = AS511_CONNECTION(gobj);
  g_mutex_clear(&conn->io_mutex);
  G_OBJECT_CLASS(AS511_connection_parent_class)->finalize(gobj);
}

static void
AS511_connection_class_init (AS511ConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = dispose;
  gobject_class->finalize = finalize;
}

static void
AS511_connection_init(AS511Connection *conn)
{
  g_mutex_init(&conn->io_mutex);
  conn->io = NULL;
  conn->in = NULL;
  conn->out = NULL;
  conn->cancel_io = NULL;
}

static gboolean
read_data(AS511Connection *conn, void *buffer, gsize count, GError **error)
{
  gsize bytes_read;
  if (!g_input_stream_read_all(conn->in, buffer, count, &bytes_read,
			       conn->cancel_io, error)) {
    return FALSE;
  }
#ifdef PRINT_DATA
  {
    gint i;
    GString *str = g_string_new("AS511 recv:");
    for (i = 0; i < bytes_read; i++) {
      g_string_append_printf(str," %02x", ((guint8*)buffer)[i]);
    }
    g_debug(str->str);
    g_string_free(str, TRUE);
  }
#endif
  if (count != bytes_read) {
    g_set_error(error, 
		AS511_CONNECTION_ERROR, AS511_CONNECTION_ERROR_SHORT_READ,
		"Failed to read as many bytes as requested");
    return FALSE;
  }
  return TRUE;
}


static gboolean
write_data(AS511Connection *conn, const void *buffer, gsize count, GError **error)
{
  gsize bytes_written;
#ifdef PRINT_DATA
  {
    gint i;
    GString *str = g_string_new("AS511 send:");
    for (i = 0; i < count; i++) {
      g_string_append_printf(str," %02x", ((guint8*)buffer)[i]);
    }
    g_debug(str->str);
    g_string_free(str, TRUE);
  }
#endif
  if (!g_output_stream_write_all(conn->out, buffer, count, &bytes_written,
			       conn->cancel_io, error)) {
    return FALSE;
  }
  if (count != bytes_written) {
    g_set_error(error, 
		AS511_CONNECTION_ERROR, AS511_CONNECTION_ERROR_SHORT_WRITE,
		"Failed to write as many bytes as requested");
    return FALSE;
  }
  return TRUE;
}

static gboolean
write_data_escaped(AS511Connection *conn, const guint8 *buffer, gsize count, 
	   GError **error)
{
  static const guint8 dle_dle[2] = {DLE, DLE};
  const guint8 *end = buffer + count;
  while(buffer < end) {
    const guint8 *p = buffer;
    while(p < end && *p != DLE) p++;
    if (!write_data(conn, buffer, p - buffer, error)) return FALSE;
    if (p == end) return TRUE;
    if (!write_data(conn, dle_dle, 2, error)) return FALSE;
    buffer = p + 1;
  }
  return TRUE;
}

static gssize
read_block(AS511Connection *conn, guint8 *buffer, gsize count, GError **error)
{
  gboolean escape = FALSE;
  gboolean done = FALSE;
  gboolean overflow = FALSE;
  gint p;
  guint8 rbuf[16];
  gssize bytes_read;
  guint8 *end = buffer + count;
  while(!done) {
    bytes_read = g_input_stream_read(conn->in, rbuf, sizeof(rbuf),
				     conn->cancel_io, error);
    if (bytes_read < 0)
      return -1;

#ifdef PRINT_DATA
    {
      gint i;
      GString *str = g_string_new("AS511 recv:");
      for (i = 0; i < bytes_read; i++) {
	g_string_append_printf(str," %02x", ((guint8*)rbuf)[i]);
      }
      g_debug(str->str);
      g_string_free(str, TRUE);
    }
#endif
    for (p = 0; p < bytes_read; p++) {

      if (escape) {
	if (rbuf[p] == DLE) {
	  if (buffer == end) {
	    overflow = TRUE;
	    done = TRUE;
	    break;
	  }
	  *buffer++ = DLE;
	  escape = FALSE;
	} else {
	  done = TRUE;
	  break;
	}
      } else {
	if (rbuf[p] == DLE) {
	  escape = TRUE;
	} else {
	  if (buffer == end) {
	    overflow = TRUE;
	    done = TRUE;
	    break;
	  }
	  *buffer++ = rbuf[p];
	}
      }
    }
  }
  if (overflow) {
    g_set_error(error, AS511_CONNECTION_ERROR,
		AS511_CONNECTION_ERROR_BUFFER_OVERFLOW,
		"Data block bigger than expected");
    return -1;
  }
  return buffer - (end - count);
}

static gboolean
AS511_connection_wait_ack(AS511Connection *conn, GError **err)
{
  guint8 response[2];
  if (!read_data(conn, response, 2, err)) return FALSE;
  if (response[0] != DLE) {
    g_set_error(err, AS511_CONNECTION_ERROR,
		AS511_CONNECTION_ERROR_UNEXPECTED_REPLY,
		"Expected DLE got 0x%02x", response[0]);
    return FALSE;
  }
  if (response[1] == NAK) {
     g_set_error(err, AS511_CONNECTION_ERROR,
		AS511_CONNECTION_ERROR_RECEIVED_NAK,
		"Received NAK");
    return FALSE;
  } else if (response[1] != ACK) {
    g_set_error(err, AS511_CONNECTION_ERROR,
		AS511_CONNECTION_ERROR_UNEXPECTED_REPLY,
		"Expected ACK or NAK got 0x%02x", response[0]);
    return FALSE;
  }
  return TRUE;
}


static gboolean
AS511_connection_send_ack(AS511Connection *conn, GError **err)
{
  static guint8 dle_ack[] = {DLE, ACK};
  return write_data(conn, dle_ack, 2, NULL);
}

static gboolean
AS511_connection_send_nak(AS511Connection *conn, GError **err)
{
  static guint8 dle_nak[] = {DLE, NAK};
  return   write_data(conn, dle_nak, 2, NULL);
}

static gboolean
AS511_connection_wait_stx(AS511Connection *conn, GError **err)
{
  guint8 response[1];
  if (!read_data(conn, response, 1, err)) return FALSE;
  if (response[0] != STX) {
    g_set_error(err, AS511_CONNECTION_ERROR,
		AS511_CONNECTION_ERROR_UNEXPECTED_REPLY,
		"Expected STX got 0x%02x", response[0]);
    AS511_connection_send_nak(conn, NULL);
    return FALSE;
  }
  return AS511_connection_send_ack(conn, err);
}

static gboolean
AS511_connection_wait_etx(AS511Connection *conn, GError **err)
{
  guint8 response[2];
  if (!read_data(conn, response, 2, err)) return FALSE;
  if (response[0] != DLE) {
    g_set_error(err, AS511_CONNECTION_ERROR,
		AS511_CONNECTION_ERROR_UNEXPECTED_REPLY,
		"Expected DLE got 0x%02x", response[0]);
    AS511_connection_send_nak(conn,NULL);
    return FALSE;
  }
  if (response[1] != ETX) {
    g_set_error(err, AS511_CONNECTION_ERROR,
		AS511_CONNECTION_ERROR_UNEXPECTED_REPLY,
		"Expected ETX got 0x%02x", response[0]);
    AS511_connection_send_nak(conn,NULL);
    return FALSE;
  }
  return AS511_connection_send_ack(conn,err);
}

static gboolean
AS511_connection_send_stx(AS511Connection *conn, GError **err)
{
  const guint8 stx = STX;
  if (!write_data(conn, &stx, 1, err)) return FALSE;
  return AS511_connection_wait_ack(conn, err);
}

gboolean
AS511_connection_send_start(AS511Connection *conn, guint8 function, 
			    guint8 *answer, GError **err)
{
  if (!AS511_connection_send_stx(conn, err)) return FALSE;
  if (!write_data_escaped(conn, &function, 1, err)) return FALSE;
  if (!AS511_connection_wait_stx(conn, err)) return FALSE;
  if (!read_data(conn, answer, 1, err)) return FALSE; 
  if (!AS511_connection_wait_etx(conn, err)) return FALSE;
  return TRUE;
}

/* DIR function */

static guint16 *
AS511_connection_dir(AS511Connection *conn, guint8 id, GError **err)
{
  gsize i;
  gssize dir_size;
  guint16 *block_dir;
  guint8 buf[3];
  guint8 answer;
  if (!AS511_connection_send_start(conn, 0x1b, &answer, err)) return NULL;
  buf[0] = id;
  buf[1] = DLE;
  buf[2] = EOT;
  if (!write_data(conn,buf,3,err)) return NULL;
  if (!AS511_connection_wait_ack(conn, err)) return NULL;
  if (!AS511_connection_wait_stx(conn, err)) return NULL;
  block_dir =  g_new(guint16, 257);
  dir_size = read_block(conn,(guint8*)block_dir,sizeof(guint16)*257,err);
  if (dir_size < 0) {
    g_free(block_dir);
    return NULL;
  }
  g_debug("dir_size: %"G_GSSIZE_FORMAT, dir_size);
  for (i = 0; i < dir_size/sizeof(guint16); i++) {
    block_dir[i] = GUINT16_FROM_BE(block_dir[i]);
  }
  if (!AS511_connection_send_ack(conn, err)) return NULL;
  if (!AS511_connection_wait_stx(conn, err)) return NULL;
  if (!read_data(conn, &answer, 1, err)) return NULL;
  if (!AS511_connection_wait_etx(conn, err)) return NULL;
  return block_dir;
}

static void 
dir_thread(GTask         *task,
	   gpointer       source_object,
	   gpointer       task_data,
	   GCancellable  *cancellable)
{
  guint16 *db_addr;
  GError *err = NULL;
  AS511Connection *conn = source_object;
  g_mutex_lock(&conn->io_mutex);
  g_assert(conn->cancel_io == NULL);
  conn->cancel_io = cancellable;
  db_addr = AS511_connection_dir(conn, GPOINTER_TO_SIZE(task_data), &err);
  conn->cancel_io = NULL;
  g_mutex_unlock(&conn->io_mutex);
  if (db_addr) {
    g_debug("dir_thread: done");
    g_task_return_pointer (task, db_addr, g_free);
  } else {
    g_task_return_error (task, err);
  }
}

void
AS511_connection_dir_async(AS511Connection *conn,
			   guint8 id,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
  GTask *task;
  task = g_task_new(conn, cancellable, callback, user_data);
  g_task_set_task_data(task, GSIZE_TO_POINTER(id), NULL);
  g_task_run_in_thread(task, dir_thread);
  g_object_unref(task);
}

guint16 *
AS511_connection_dir_finish(AS511Connection *conn, 
			    GAsyncResult *result,
			    GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, conn), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

/* SYS_PAR function */

#define GET_UINT16(ptr) GUINT16_FROM_BE(*((gint16*)(ptr)))
static AS511SysPar *
AS511_connection_sys_par(AS511Connection *conn, GError **err)
{
  AS511SysPar *sys;
  gssize sys_size;
  guint8 *block_sys;
  guint8 buf[4];
  guint8 answer;
  if (!AS511_connection_send_start(conn, 0x18, &answer, err)) return NULL;
  buf[0] = 0;
  buf[1] = 0;
  buf[2] = DLE;
  buf[3] = EOT;
  if (!write_data_escaped(conn,buf,2,err)) return NULL;
  if (!write_data(conn,buf+2,2,err)) return NULL;
  if (!AS511_connection_wait_ack(conn, err)) return NULL;
  if (!AS511_connection_wait_stx(conn, err)) return NULL;
  block_sys =  g_new(guint8, 64);
  sys_size = read_block(conn,(guint8*)block_sys,sizeof(guint16)*64,err);
  if (sys_size < 0) {
    g_free(block_sys);
    return NULL;
  }
  g_debug("sys_size: %"G_GSSIZE_FORMAT, sys_size);
  sys = g_new(AS511SysPar,1);
  sys->PAE = GET_UINT16(block_sys + 5);
  sys->PAA = GET_UINT16(block_sys + 7);
  sys->M = GET_UINT16(block_sys + 9);
  sys->T = GET_UINT16(block_sys + 11);
  sys->Z = GET_UINT16(block_sys + 13);
  sys->IAData = GET_UINT16(block_sys + 15);
  sys->PLCType = block_sys[17+26];
  g_free(block_sys);
  if (!AS511_connection_send_ack(conn, err)) return NULL;
  if (!AS511_connection_wait_stx(conn, err)) return NULL;
  if (!read_data(conn, &answer, 1, err)) return NULL;
  if (!AS511_connection_wait_etx(conn, err)) return NULL;
  return sys;
}

static void 
sys_par_thread(GTask         *task,
	       gpointer       source_object,
	       gpointer       task_data,
	       GCancellable  *cancellable)
{
  AS511SysPar *sys;
  GError *err = NULL;
  AS511Connection *conn = source_object;
  g_mutex_lock(&conn->io_mutex);
  g_assert(conn->cancel_io == NULL);
  conn->cancel_io = cancellable;
  sys = AS511_connection_sys_par(conn, &err);
  conn->cancel_io = NULL;
  g_mutex_unlock(&conn->io_mutex);
  if (sys) {
    g_debug("sys_par_thread: done");
    g_task_return_pointer (task, sys, g_free);
  } else {
    g_task_return_error (task, err);
  }
}

void
AS511_connection_sys_par_async(AS511Connection *conn, 
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
  GTask *task;
  task = g_task_new(conn, cancellable, callback, user_data);
  g_task_run_in_thread(task, sys_par_thread);
  g_object_unref(task);
}

AS511SysPar *
AS511_connection_sys_par_finish(AS511Connection *conn, 
				GAsyncResult *result,
				GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, conn), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

/* DB_READ function */

static guint8 *
AS511_connection_db_read(AS511Connection *conn, 
		     guint16 first, guint16 last,
		     GError **err)
{
  guint8 *data;
  gssize read_size;
  guint8 buf[6];
  guint8 answer;
  gssize len = last - first + 1;
  if (!AS511_connection_send_start(conn, 0x04, &answer, err)) return NULL;
  buf[0] = first>>8;
  buf[1] = first;
  buf[2] = last>>8;
  buf[3] = last;
  buf[4] = DLE;
  buf[5] = EOT;
  if (!write_data_escaped(conn,buf,4,err)) return NULL;
  if (!write_data(conn,buf+4,2,err)) return NULL;
  if (!AS511_connection_wait_ack(conn, err)) return NULL;
  if (!AS511_connection_wait_stx(conn, err)) return NULL;
  /* Skip the first 5 bytes of data */
  if (!read_data(conn, buf, 5, err)) return NULL;
  data =  g_new(guint8, len);
  read_size = read_block(conn,data,len,err);
  if (read_size < 0) {
    g_free(data);
    return NULL;
  }
  g_debug("read_size: %"G_GSSIZE_FORMAT, read_size);
  if (read_size != len) {
    g_set_error(err, 
		AS511_CONNECTION_ERROR, AS511_CONNECTION_ERROR_SHORT_BLOCK,
		"Failed to receive as many bytes as requested"
		" (requestaed %d, got %d)", (int)len, (int)read_size);
    return FALSE;
  }
  if (!AS511_connection_send_ack(conn, err)) return NULL;
  if (!AS511_connection_wait_stx(conn, err)) return NULL;
  if (!read_data(conn, &answer, 1, err)) return NULL;
  if (!AS511_connection_wait_etx(conn, err)) return NULL;
  return data;
}

typedef struct
{
  guint16 first;
  guint16 last;
} ReadParams;
  
static void 
db_read_thread(GTask         *task,
	       gpointer       source_object,
	       gpointer       task_data,
	       GCancellable  *cancellable)
{
  AS511ReadResult *res;
  guint8 *data;
  ReadParams *params = task_data;
  GError *err = NULL;
  AS511Connection *conn = source_object;
  g_mutex_lock(&conn->io_mutex);
  g_assert(conn->cancel_io == NULL);
  conn->cancel_io = cancellable;
  data= AS511_connection_db_read(conn, params->first, params->last,  &err);
  conn->cancel_io = NULL;
  g_mutex_unlock(&conn->io_mutex);
  if (data) {
    res = g_new(AS511ReadResult, 1);
    res->first = params->first;
    res->length = params->last - params->first + 1;
    res->data = data;
    g_debug("db_read_thread: done");
    g_task_return_pointer (task, res, g_free);
  } else {
    g_task_return_error (task, err);
  }
}

void
AS511_connection_db_read_async(AS511Connection *conn,
			       guint16 first,
			       guint16 last,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
  GTask *task;
  ReadParams *params = g_new(ReadParams, 1);
  params->first = first;
  params->last = last;
  task = g_task_new(conn, cancellable, callback, user_data);
  g_task_set_task_data(task, params, g_free);
  g_task_run_in_thread(task, db_read_thread);
  g_object_unref(task);
}

AS511ReadResult *
AS511_connection_db_read_finish(AS511Connection *conn, 
				GAsyncResult *result,
				GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, conn), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

/* DB_WRITE function */

static gboolean
AS511_connection_db_write(AS511Connection *conn, 
			  guint16 first, guint16 length, 
			  const guint8 *data,
			  GError **err)
{
  static const guint8 dle_eot[] = {DLE, EOT};
  guint8 buf[2];
  guint8 answer;
  if (!AS511_connection_send_start(conn, 0x04, &answer, err)) return FALSE;
  buf[0] = first>>8;
  buf[1] = first;
  if (!write_data_escaped(conn,buf,2,err)) return FALSE;
  if (!write_data_escaped(conn,data, length, err)) return FALSE;
  if (!write_data(conn,dle_eot,2,err)) return FALSE;
  if (!AS511_connection_wait_ack(conn, err)) return FALSE;

  if (!AS511_connection_wait_stx(conn, err)) return FALSE;
  if (!read_data(conn, &answer, 1, err)) return FALSE;
  if (!AS511_connection_wait_etx(conn, err)) return FALSE;
  return TRUE;
}

typedef struct
{
  guint16 first;
  guint16 length;
  guint8 *data;
} WriteParams;

static void
free_write_params(gpointer p)
{
  WriteParams *params = p;
  g_free(params->data);
  g_free(params);
}

static void 
db_write_thread(GTask         *task,
	       gpointer       source_object,
	       gpointer       task_data,
	       GCancellable  *cancellable)
{
  gboolean ret;
  WriteParams *params = task_data;
  GError *err = NULL;
  AS511Connection *conn = source_object;
  g_mutex_lock(&conn->io_mutex);
  g_assert(conn->cancel_io == NULL);
  conn->cancel_io = cancellable;
  ret = AS511_connection_db_write(conn, params->first, params->length,
				  params->data,  &err);
  conn->cancel_io = NULL;
  g_mutex_unlock(&conn->io_mutex);
  if (ret) {
    g_debug("db_write_thread: done");
    g_task_return_boolean (task, TRUE);
  } else {
    g_task_return_error (task, err);
  }
}

void
AS511_connection_db_write_async(AS511Connection *conn,
				guint16 first,
				guint16 length, 
				const guint8 *data,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data)
{
  GTask *task;
  WriteParams *params = g_new(WriteParams, 1);
  params->first = first;
  params->length = length;
  params->data = g_memdup(data,length);
  task = g_task_new(conn, cancellable, callback, user_data);
  g_task_set_task_data(task, params, free_write_params);
  g_task_run_in_thread(task, db_write_thread);
  g_object_unref(task);
}

gboolean
AS511_connection_db_write_finish(AS511Connection *conn, 
				 GAsyncResult *result,
				 GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, conn), NULL);
  return g_task_propagate_boolean (G_TASK (result), error);
} 

AS511Connection *
AS511_connection_new(GIOStream *io)
{
  AS511Connection *conn = g_object_new(AS511_CONNECTION_TYPE, NULL);
  /* No need to reference the in and out streams since they stay as long as the
     io stream */
  conn->in = g_io_stream_get_input_stream(io);
  conn->out = g_io_stream_get_output_stream(io);
  conn->io = io;
  g_object_ref(io);
  return conn;
}
