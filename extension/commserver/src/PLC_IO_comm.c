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
  void (*reply_ready)(PlcIoComm *comm, PLCCmd *cmd);
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
serial_cmd_free(PLCCmd *cmd)
{
  g_free(cmd->cmd);
  g_free(cmd->reply);
  g_free(cmd);
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
  comm->cmd_queue = g_async_queue_new_full((GDestroyNotify)serial_cmd_free);
  comm->reply_queue = g_async_queue_new_full((GDestroyNotify)serial_cmd_free);
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
static gpointer
serial_read_thread(gpointer data)
{
  GError *err = NULL;
  PlcIoComm *comm = data;
  gchar buffer[20];
  GInputStream *inp = g_io_stream_get_input_stream(comm->io);
  while(TRUE) {
    gssize read;
    read = g_input_stream_read (inp, buffer, sizeof(buffer), comm->cancel,
				&err);
    if (read < 0) {
      if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
	g_printerr("Failed to read from serial device: %s\n", err->message);
      }
      g_clear_error(&err);
      break;
    } 
    if (read > 0) {
      gchar *p = buffer;
      gchar *end = buffer + read;
      g_mutex_lock(&comm->mutex);
      if (comm->pending) {
	while(p < end) {
	  if (*p == '\n' || *p == '\r') {
	    comm->line[comm->line_len] = '\0';
	    comm->pending = FALSE;
	    g_cond_signal(&comm->cond);
	    break;
	  }
	  if (comm->line_len < sizeof(comm->line) - 1) {
	    comm->line[comm->line_len++] = *p;
	  }
	  p++;
	}
      } 
      g_mutex_unlock(&comm->mutex);
      g_usleep(100000); 
    }
  }
  return NULL;
}

#define NPORTS 9

static gboolean
send_reply_signal(gpointer user_data)
{
  PlcIoComm *comm = user_data;
  g_mutex_lock(&comm->mutex);
  if (!g_source_is_destroyed (g_main_current_source ())) {
    g_debug("Send signal");
    while(TRUE) {
      PLCCmd *cmd = g_async_queue_try_pop(comm->reply_queue);
      if (!cmd) break;
      g_signal_emit(comm, signals[REPLY_READY], 0, cmd);
      serial_cmd_free(cmd);
    }
    comm->idle = NULL;
  }
  g_mutex_unlock(&comm->mutex);
  return G_SOURCE_REMOVE;
}

static gpointer
serial_thread(gpointer data)
{
  GError *err = NULL;
  gint64 end_time;
  gsize written;
  GThread *read_thread;
  PlcIoComm *comm = data;
  GOutputStream *out = g_io_stream_get_output_stream(comm->io);
  g_debug("Thread started");
  read_thread = g_thread_new("Serial read", serial_read_thread, comm);

  /* Wait for ready signal. If not received within the timeout then go
     ahead and send the command anyway. */
  while(!g_cancellable_is_cancelled(comm->cancel)) {
    PLCCmd *pending_cmd;
        
    end_time = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;
    
    pending_cmd = g_async_queue_timeout_pop(comm->cmd_queue, 1000000);
    if  (!pending_cmd) continue;
    
    g_mutex_lock(&comm->mutex);
    comm->pending = TRUE;
    comm->line_len = 0;
    g_mutex_unlock(&comm->mutex);
    
    g_debug("Sending command: %s", pending_cmd->cmd);
    if (!g_output_stream_write_all(out, 
				   pending_cmd->cmd, strlen(pending_cmd->cmd),
				   &written,  comm->cancel,
				   &err)
	|| !g_output_stream_write_all(out, 
				      "\n",
				      1,
				      &written,  comm->cancel,
				      &err)) {
      if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
	break;
      }
      g_printerr("Failed to write to serial device: %s\n", err->message);
      g_clear_error(&err);
    }
    
    /* Wait for reply */
    end_time = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;
    g_mutex_lock(&comm->mutex);
    while(comm->pending) {
      if (!g_cond_wait_until(&comm->cond, &comm->mutex, end_time))
	break;
    }
    if (!comm->pending) {
      g_debug("Line: '%s'", comm->line);
      pending_cmd->reply = g_strdup(comm->line);
    }
      
    comm->pending = FALSE;
    g_mutex_unlock(&comm->mutex);
    g_async_queue_push(comm->reply_queue, pending_cmd);
    
    g_mutex_lock(&comm->mutex);
    if (comm->idle == NULL) {
      comm->idle = g_idle_source_new();
      g_source_set_callback (comm->idle, send_reply_signal, comm, NULL);
      g_source_attach(comm->idle, comm->main_context);
    }
    g_mutex_unlock(&comm->mutex);
    
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
plc_io_comm_send(PlcIoComm *comm,  const gchar *cmd_str, GError **err)
{
  PLCCmd *cmd = g_new(PLCCmd,1);
  cmd->cmd = g_strdup(cmd_str);
  cmd->reply = NULL;
  g_async_queue_push(comm->cmd_queue, cmd);
  return TRUE;
}
