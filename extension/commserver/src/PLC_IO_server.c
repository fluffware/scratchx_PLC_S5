#include "PLC_IO_server.h"
#include <websocket_server_private.h>
#include <json-glib/json-glib.h>
#include <plc_protocol.h>

GQuark
plc_io_server_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("plc_io-server-error-quark");
  return error_quark;
}


enum {
  LAST_SIGNAL
};

/* static guint plc_io_server_signals[LAST_SIGNAL] = {0 }; */

enum
{
  PROP_0 = 0,
  N_PROPERTIES
};

struct _PlcIoServer
{
  WebsocketServer parent_instance;
  const struct libwebsocket_protocols *protocol;
  PlcIoComm *comm;
  struct QueueHead *replies;
  JsonParser *json;
};

struct _PlcIoServerClass
{
  WebsocketServerClass parent_class;

  /* class members */

  /* Signals */
};


G_DEFINE_TYPE (PlcIoServer, plc_io_server, WEBSOCKET_SERVER_TYPE)


static void
dispose(GObject *gobj)
{
  PlcIoServer *server = PLC_IO_SERVER(gobj);
  g_clear_object(&server->comm);
  g_clear_object(&server->json);
  G_OBJECT_CLASS(plc_io_server_parent_class)->dispose(gobj);
}

static void
finalize(GObject *gobj)
{
  /* PlcIoServer *server = PLC_IO_SERVER(gobj); */
  G_OBJECT_CLASS(plc_io_server_parent_class)->finalize(gobj);
}

static void
set_property (GObject *object, guint property_id,
	      const GValue *value, GParamSpec *pspec)
{
  /* PlcIoServer *server = PLC_IO_SERVER(object); */
  switch (property_id)
    {
    default:
       /* We don't have any other property... */
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
get_property (GObject *object, guint property_id,
	      GValue *value, GParamSpec *pspec)
{
  /* PlcIoServer *server = PLC_IO_SERVER(object); */
  switch (property_id) {
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static int
plc_io_callback(struct libwebsocket_context *context,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, 
		void *user,
		void *in, size_t len);

struct SessionData
{
  struct QueueHead *replies;
};

static const struct libwebsocket_protocols protocols[] = 
{
  {"PLC_IO", plc_io_callback, sizeof(struct SessionData), 100, .owning_server = NULL, .protocol_index = 0},
};

static guint
get_protocols(WebsocketServer *server,
	       guint allocate_extra,
	       struct libwebsocket_protocols **protocolsp)
{
  return websocket_server_add_protocols(server, allocate_extra,
					protocolsp,
					protocols, 1);
}

static void
plc_io_server_class_init (PlcIoServerClass *klass)
{
  /* GParamSpec *properties[N_PROPERTIES]; */
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  WebsocketServerClass *server_class = WEBSOCKET_SERVER_CLASS(klass);

  gobject_class->dispose = dispose;
  gobject_class->finalize = finalize;
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;

  server_class->get_protocols = get_protocols;
  /* properties[0] = NULL; */
}

static void
plc_io_server_init(PlcIoServer *server)
{
  server->replies = NULL;
  server->json = json_parser_new();
  server->protocol = NULL;
}

static void
reply_handler(PlcIoComm *comm, PLCMsg *msg, PlcIoServer *server);

PlcIoServer *
plc_io_server_new(PlcIoComm *comm)
{
  PlcIoServer *server = g_object_new (PLC_IO_SERVER_TYPE, NULL);
  server->comm = comm;
  g_object_ref(server->comm);
  g_signal_connect_object (comm, "reply-ready", G_CALLBACK(reply_handler),
			   server, 0);
  return server;
}

struct MsgQueue
{
  struct MsgQueue *next;
   guint8 *msg;
};

struct QueueHead
{
  struct QueueHead *next;
  struct QueueHead **prevp;
  struct MsgQueue **in;
  struct MsgQueue *out;
};

static struct QueueHead *
msg_queue_head_add(struct QueueHead **list)
{
  struct QueueHead *head = g_new(struct QueueHead, 1);
  head->next = *list;
  head->prevp = list;
  head->in = &head->out;
  head->out = NULL;
  *list = head;
  return head;
}

static void
msg_queue_head_remove(struct QueueHead *head)
{
  struct MsgQueue *mq;
  *head->prevp = head->next;
  mq = head->out;
  while(mq) {
    struct MsgQueue *next = mq->next;
    g_free(mq->msg);
    g_free(mq);
    mq = next;
  }
}

static void
msg_queue_push(struct QueueHead *list, const guint8 *msg)
{
  int len = msg[0] >>6;
  if (len == 3) len = msg[1] + 1;
  len += 2;
  if (!list) return;
  while(list) {
    struct MsgQueue *mq = g_new(struct MsgQueue, 1);
    mq->msg = g_new(guint8, len);
    memcpy(mq->msg, msg, len);
    mq->next = NULL;
    *list->in = mq;
    list->in = &mq->next;
    list = list->next;
  }
}

static guint8 *
msg_queue_pop(struct QueueHead *q)
{
  guint8 *msg;
  struct MsgQueue *mq;
  if (!q->out) return NULL;
  mq = q->out;
  msg = mq->msg;
  q->out = mq->next;
  if (!q->out) q->in = &q->out;
  g_free(mq);
  return msg;
}

static void
reply_handler(PlcIoComm *comm, PLCMsg *msg, PlcIoServer *server)
{
  g_debug("reply_handler");
  if (!server->protocol) return;
  msg_queue_push(server->replies, msg->msg);

  libwebsocket_callback_on_writable_all_protocol(server->protocol);
}

static void
send_cmd(PlcIoServer *server, guint8 *cmd)
{
  plc_io_comm_send(server->comm, cmd, NULL);
}

static void
cmd_request_send(PlcIoServer *server,
		 struct libwebsocket *wsi,
		 const gchar *cmd, gssize len)
{
  GError *err = NULL;
  if (json_parser_load_from_data(server->json, cmd, len, &err)) {
    const gchar *cmd;
    guint8 addr;
    guint16 value;
    guint16 mask;
    JsonNode *value_node;
    JsonNode *root = json_parser_get_root (server->json);
    if (JSON_NODE_HOLDS_OBJECT(root)) {
      JsonObject *root_obj = json_node_get_object(root);
      value_node = json_object_get_member(root_obj, "cmd");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	cmd = json_node_dup_string(value_node);
      } else {
	return;
      }
      addr = 0;
      value_node = json_object_get_member(root_obj, "addr");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	addr = json_node_get_int(value_node);
      }
      value = 0;
      value_node = json_object_get_member(root_obj, "value");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	value = json_node_get_int(value_node);
      }
      mask = 0xffff;
      value_node = json_object_get_member(root_obj, "mask");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	mask = json_node_get_int(value_node);
      }

      if (strcmp(cmd, "din") == 0) {
	guint8 cmd[2];
	cmd[0] = PLC_CMD_READ_DIGITAL_INPUT;
	cmd[1] = addr;
	send_cmd(server, cmd);
      } else if (strcmp(cmd, "dout") == 0) {
	guint8 cmd[2];
	cmd[0] = PLC_CMD_WRITE_DIGITAL_OUTPUT;
	cmd[1] = 3;
	cmd[2] = addr;
	cmd[3] = value & mask;
	cmd[4] = mask;
	send_cmd(server, cmd);
      } else if (strcmp(cmd, "ain") == 0) {
	guint8 cmd[2];
	cmd[0] = PLC_CMD_READ_ANALOG_INPUT;
	cmd[1] = addr;
	send_cmd(server, cmd);
      } else if (strcmp(cmd, "aout") == 0) {
	guint8 cmd[2];
	cmd[0] = PLC_CMD_WRITE_ANALOG_OUTPUT;
	cmd[1] = 3;
	cmd[2] = addr;
	cmd[3] = value;
	cmd[4] = value>>8;
	send_cmd(server, cmd);
      }
    }
  } else {
    g_warning("Failed to parse JSON request: %s", err->message);
    g_clear_error(&err);
  }
}


static const gchar padding[MAX(LWS_SEND_BUFFER_POST_PADDING,
			       LWS_SEND_BUFFER_PRE_PADDING)];

static int
plc_io_callback(struct libwebsocket_context *context,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, 
		void *user,
		void *in, size_t len)
{
  struct SessionData *session = user;
  PlcIoServer *server = libwebsocket_context_user (context);
  switch(reason) {
  case LWS_CALLBACK_PROTOCOL_INIT:
    break;
  case LWS_CALLBACK_ESTABLISHED:
    g_debug("PLC_IO established");
    server->protocol = libwebsockets_get_protocol(wsi);
    session->replies = msg_queue_head_add(&server->replies);
    break;
  case LWS_CALLBACK_CLOSED:
    {
      msg_queue_head_remove(session->replies);
    }
    break;
  case LWS_CALLBACK_PROTOCOL_DESTROY:
    server->protocol = NULL;
    g_debug("PLC_IO protocol destroy");
    break;
    
  case  LWS_CALLBACK_SERVER_WRITEABLE:
    {
      guint8 *msg;
      
      while((msg = msg_queue_pop(session->replies))) {
	gchar *cmd;
	gint addr = -1;
	gint value = -1;
	switch(msg[0]) {
	case PLC_REPLY_DIGITAL_INPUT:
	  cmd = "din";
	  addr = msg[1];
	  value = msg[2];
	  break;
	case PLC_REPLY_DIGITAL_OUTPUT:
	  cmd = "dout";
	  addr = msg[1];
	  value = msg[2];
	  break;
	default:
	  cmd = "error";
	}

	GString *reply = g_string_new_len(padding, LWS_SEND_BUFFER_PRE_PADDING);
	g_string_append_printf(reply, "{");
	if (addr >= 0) {
	  g_string_append_printf(reply, "\"addr\":%d,", addr);
	}
	if (value >= 0) {
	  g_string_append_printf(reply, "\"value\":%d,", value);
	}
	g_string_append_printf(reply, "\"cmd\":\"%s\"}", cmd);
	g_string_append_len(reply, padding, LWS_SEND_BUFFER_POST_PADDING);
	libwebsocket_write(wsi,((unsigned char*)reply->str 
				+ LWS_SEND_BUFFER_PRE_PADDING),
			   (reply->len - LWS_SEND_BUFFER_PRE_PADDING 
			    - LWS_SEND_BUFFER_POST_PADDING),
			   LWS_WRITE_TEXT);
	g_string_free(reply, TRUE);
	
	g_free(msg);
      }
    }
    break;
  case  LWS_CALLBACK_RECEIVE:
    g_debug("PLC_IO receive");
    cmd_request_send(server, wsi, in, len);
    break;
  default:
    break;
  }
  return 0;
}
