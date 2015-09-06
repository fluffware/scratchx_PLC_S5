#include "PLC_IO_comm.h"
#include <string.h>

GQuark
plc_io_comm_error_quark(void)
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("df1-protocol-error-quark");
  return error_quark;
}

enum
{
  PROP_0 = 0,
  PROP_MAIN_CONTEXT,
  PROP_SERIAL_DEVICE,
  N_PROPERTIES
};

  
struct _PlcIoComm
{
  GObject parent_instance;
  GIOStream *io;
  GCancellable *cancel;
  
  gboolean pending; /* A command is pending */
  gchar line[20];
  gssize line_len;
  GThread *thread;
  GCond cond;
  GMutex mutex;
  GMainContext *main_context; /* Main context for signals */
  GAsyncQueue *cmd_queue;
  GAsyncQueue *reply_queue;
  GSource *idle;
};

struct _PlcIoCommClass
{
  GObjectClass parent_class;
  
  /* class members */

  /* Signals */
  void (*reply_ready)(PlcIoComm *comm, PLCMsg *msg);
};

enum {
  REPLY_READY,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0 };

G_DEFINE_TYPE (PlcIoComm, plc_io_comm, G_TYPE_OBJECT)

static void
stop_thread(PlcIoComm *comm);
  
static void
start_thread(PlcIoComm *comm);
  

static void
dispose(GObject *gobj)
{
  PlcIoComm *comm = PLC_IO_COMM(gobj);
  stop_thread(comm);
  g_io_stream_close(comm->io, NULL, NULL);
  g_clear_object(&comm->io);
  if (comm->cmd_queue) {
    g_async_queue_unref(comm->cmd_queue);
    comm->cmd_queue = NULL;
  }
  if (comm->reply_queue) {
    g_async_queue_unref(comm->reply_queue);
    comm->reply_queue = NULL;
  }
  if (comm->main_context) {
    g_main_context_unref(comm->main_context);
    comm->main_context = NULL;
  }
  
  G_OBJECT_CLASS(plc_io_comm_parent_class)->dispose(gobj);
}

static void
finalize(GObject *gobj)
{
  PlcIoComm *comm = PLC_IO_COMM(gobj);
  g_mutex_clear(&comm->mutex);
  g_cond_clear(&comm->cond);
  G_OBJECT_CLASS(plc_io_comm_parent_class)->finalize(gobj);
}

static void
set_property (GObject *object, guint property_id,
	      const GValue *gvalue, GParamSpec *pspec)
{
  PlcIoComm *comm = PLC_IO_COMM(object);
  switch (property_id) {
  case PROP_MAIN_CONTEXT:
    if (comm->main_context) {
      g_main_context_unref(comm->main_context);
    }
    comm->main_context = g_value_get_pointer(gvalue);
    if (comm->main_context) {
      g_main_context_ref(comm->main_context);
    }
    break;
  default:
    /* We don't have any other property... */
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
       break;
  }
}

static void
get_property (GObject *object, guint property_id,
	      GValue *gvalue, GParamSpec *pspec)
{
  PlcIoComm *comm = PLC_IO_COMM(object);
  switch (property_id) {
  case PROP_MAIN_CONTEXT:
    g_value_set_pointer(gvalue, comm->main_context);
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
plc_io_comm_class_init (PlcIoCommClass *klass)
{
  GParamSpec *properties[N_PROPERTIES];
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->dispose = dispose;
  gobject_class->finalize = finalize;
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;
  
  properties[0] = NULL;
  properties[PROP_MAIN_CONTEXT] =
    g_param_spec_pointer ("main-context",
			  "Main context",
			  "MainContext used for signaling from the main loop",
			  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_SERIAL_DEVICE] = 
    g_param_spec_pointer ("Serial-device",
			  "Serial device",
			  "Serial port device used for communication",
			  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_properties(gobject_class, N_PROPERTIES, properties);

  signals[REPLY_READY] =
    g_signal_new("reply-ready",
                 G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(PlcIoCommClass, reply_ready),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__POINTER,
                 G_TYPE_NONE, 1, G_TYPE_POINTER);

}

static void
serial_msg_free(PLCMsg *msg)
{
  g_clear_error(&msg->err);
  g_free(msg->msg);
  g_free(msg);
}

static void
plc_io_comm_init(PlcIoComm *comm)
{
  comm->io = NULL;

  comm->thread = NULL;
  comm->main_context = NULL;
  
  comm->cancel = NULL;
  comm->pending = FALSE;
  comm->line_len = 0;
  g_cond_init(&comm->cond);
  g_mutex_init(&comm->mutex);
  comm->cmd_queue = g_async_queue_new_full((GDestroyNotify)serial_msg_free);
  comm->reply_queue = g_async_queue_new_full((GDestroyNotify)serial_msg_free);
  comm->idle = NULL;
}

PlcIoComm *
plc_io_comm_new(GIOStream *io, GError **err)
{
  PlcIoComm *comm = g_object_new (PLC_IO_COMM_TYPE, NULL);
  comm->io = io;
  g_object_ref(comm->io);
  start_thread(comm);
  return comm;
}


static void
stop_thread(PlcIoComm *comm)
{
  if (comm->thread) {
    g_cancellable_cancel(comm->cancel);
    g_mutex_lock(&comm->mutex);
    if (comm->idle) {
      g_source_destroy(comm->idle);
      comm->idle = NULL;
    }
    g_mutex_unlock(&comm->mutex);

    g_thread_join(comm->thread);
    g_clear_object(&comm->cancel);
    comm->thread = NULL;
  }
}

static const guint8 crc_table[] = {
  /* 00 */ 0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 
  /* 08 */ 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d, 
  /* 10 */ 0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
  /* 18 */ 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
  /* 20 */ 0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
  /* 28 */ 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
  /* 30 */ 0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
  /* 38 */ 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
  /* 40 */ 0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
  /* 48 */ 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
  /* 50 */ 0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
  /* 58 */ 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
  /* 60 */ 0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
  /* 68 */ 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
  /* 70 */ 0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
  /* 78 */ 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
  /* 80 */ 0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
  /* 88 */ 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
  /* 90 */ 0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
  /* 98 */ 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
  /* a0 */ 0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
  /* a8 */ 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
  /* b0 */ 0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
  /* b8 */ 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
  /* c0 */ 0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
  /* c8 */ 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
  /* d0 */ 0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
  /* d8 */ 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13, 
  /* e0 */ 0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
  /* e8 */ 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
  /* f0 */ 0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
  /* f8 */ 0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
};

static guint8
calc_crc(guint8 *data, gsize len)
{
  guint8 crc = 0xff;
  while(len-- > 0) {
    crc = crc_table[crc ^ *data++];
  }
  return crc;
}

static gboolean
send_reply_signal(gpointer user_data)
{
  PlcIoComm *comm = user_data;
  g_mutex_lock(&comm->mutex);
  if (!g_source_is_destroyed (g_main_current_source ())) {
    g_debug("Send signal");
    while(TRUE) {
      PLCMsg *msg = g_async_queue_try_pop(comm->reply_queue);
      if (!msg) break;
      g_signal_emit(comm, signals[REPLY_READY], 0, msg);
      serial_msg_free(msg);
    }
    comm->idle = NULL;
  }
  g_mutex_unlock(&comm->mutex);
  return G_SOURCE_REMOVE;
}

static gpointer
serial_read_thread(gpointer data)
{
  GError *err = NULL;
  PlcIoComm *comm = data;
  guint8 read_buffer[20];
  guint8 reply_buffer[258];
  guint reply_len = 0;
  guint reply_left = sizeof(reply_buffer);
  GInputStream *inp = g_io_stream_get_input_stream(comm->io);
  while(TRUE) {
    gssize read;
    gsize read_len;
    gint64 read_time;
    gint64 timeout = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;
    read_len = sizeof(read_buffer);

    /* Never read more than is necessary for the reply */
    if (read_len > reply_left) {
      read_len = reply_left;
    }
    
    read = g_input_stream_read (inp, read_buffer, read_len,
				comm->cancel,
				&err);
    if (read < 0) {
      if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
	g_printerr("Failed to read from serial device: %s\n", err->message);
      }
      g_clear_error(&err);
      break;
    }
    read_time = g_get_monotonic_time ();
    if (read > 0) {
      reply_left -= read;
      if (read_time > timeout) {
	/* Reset buffer */
	reply_len = 0;
	reply_left = sizeof(reply_buffer);
      }
      memcpy(reply_buffer + reply_len, read_buffer, read);
      reply_len += read;
      if (reply_len >= 2) {
	guint len = reply_buffer[0] >> 6;
	if (len == 3) len = reply_buffer[1] + 1;
	len += 2;
	if (reply_len >= len) {
	  if (calc_crc(reply_buffer, reply_len) == 0) {
	    PLCMsg *msg = g_new(PLCMsg,1);
	    g_debug("Got reply: %d", reply_len);

	    msg->err = NULL;
	    msg->msg = g_new(guint8, len);
	    memcpy(msg->msg, reply_buffer, len);
	    g_async_queue_push(comm->reply_queue, msg);
	    
	    g_mutex_lock(&comm->mutex);
	    if (comm->idle == NULL) {
	      comm->idle = g_idle_source_new();
	      g_source_set_callback (comm->idle, send_reply_signal, comm, NULL);
	      g_source_attach(comm->idle, comm->main_context);
	    }
	    g_mutex_unlock(&comm->mutex);
	    
	  } else {
	    g_debug("CRC error");
	  }
	  reply_len = 0;
	  reply_left = sizeof(reply_buffer);
	} else {
	  reply_left = len - reply_len;
	}
      }
    }
  }
  return NULL;
}

#define NPORTS 9

static gpointer
serial_thread(gpointer data)
{
  GError *err = NULL;
  gsize written;
  GThread *read_thread;
  PlcIoComm *comm = data;
  GOutputStream *out = g_io_stream_get_output_stream(comm->io);
  g_debug("Thread started");
  read_thread = g_thread_new("Serial read", serial_read_thread, comm);

  /* Wait for ready signal. If not received within the timeout then go
     ahead and send the command anyway. */
  while(!g_cancellable_is_cancelled(comm->cancel)) {
    PLCMsg *msg;
    guint len;    
    
    msg = g_async_queue_timeout_pop(comm->cmd_queue,1000000);
    if  (!msg) continue;
    len = msg->msg[0]>>6;
    if (len == 3) len = msg->msg[1] + 1;
    len += 2;
    g_debug("Sending command");
    if (!g_output_stream_write_all(out, msg->msg, len,
				   &written,  comm->cancel,
				   &err)) {
      if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
	break;
      }
      g_printerr("Failed to write to serial device: %s\n", err->message);
      g_clear_error(&err);
    }
  }
  g_thread_join(read_thread);
  g_debug("Thread ended");
  return NULL;
}

static void
start_thread(PlcIoComm *comm)
{
  if (!comm->thread) {
    comm->cancel = g_cancellable_new();
    comm->thread = g_thread_new("PLC comm", serial_thread, comm);
  }
}

gboolean
plc_io_comm_send(PlcIoComm *comm,  const guint8 *cmd, GError **err)
{
  PLCMsg *msg = g_new(PLCMsg,1);
  guint len = cmd[0]>>6;
  if (len == 3) len = cmd[1] + 1;
  len += 2;
  msg->msg = g_new(guint8, len);
  msg->err = NULL;
  memcpy(msg->msg, cmd, len - 1);
  msg->msg[len - 1] = calc_crc(msg->msg, len - 1);
  g_async_queue_push(comm->cmd_queue, msg);
  return TRUE;
}
