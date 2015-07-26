#include "PLC_IO_server.h"
#include <websocket_server_private.h>
#include <json-glib/json-glib.h>

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
  PlcIoComm *comm;
  GQueue *requests;
  GQueue *replies;
  struct CmdRequest *pending_request; /* The command being executed */
  JsonParser *json;
};

struct _PlcIoServerClass
{
  WebsocketServerClass parent_class;

  /* class members */

  /* Signals */
};


G_DEFINE_TYPE (PlcIoServer, plc_io_server, WEBSOCKET_SERVER_TYPE)

struct CmdRequest;

static void
cmd_request_free(struct CmdRequest *);

static void
dispose(GObject *gobj)
{
  PlcIoServer *server = PLC_IO_SERVER(gobj);
  if (server->requests) {
    g_queue_free_full(server->requests,  (GDestroyNotify)cmd_request_free);
    server->requests = NULL;
  }
  if (server->replies) {
    g_queue_free_full(server->replies,  (GDestroyNotify)cmd_request_free);
    server->replies = NULL;
  }
  g_clear_object(&server->comm);
  if (server->pending_request) {
    cmd_request_free(server->pending_request);
    server->pending_request = NULL;
  }
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

static const struct libwebsocket_protocols protocols[] = 
{
  {"PLC_IO", plc_io_callback, 0, 100, .owning_server = NULL, .protocol_index = 0},
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
  server->requests = g_queue_new();
  server->replies = g_queue_new();
  server->json = json_parser_new();
}

static void
reply_handler(PlcIoComm *comm, PLCCmd *cmd, PlcIoServer *server);

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

struct CmdRequest
{
  struct libwebsocket *wsi;
  char *cmd;
  guint16 addr;
  guint16 mask;
  guint16 value;
  guint16 reply;
};

static void
cmd_request_free(struct CmdRequest *req)
{
  g_free(req->cmd);
  g_free(req);
}

static char *
build_command(const struct CmdRequest *req)
{
  if (strcmp(req->cmd, "dout") == 0) {
    return g_strdup_printf("dout %x %x %x", req->addr, req->value, req->mask);
  } else if (strcmp(req->cmd, "din") == 0) {
     return g_strdup_printf("din %x", req->addr);
  }
  return NULL;
}

void
parse_reply(const char *reply, struct CmdRequest *req)
{
  char *end;
  if (reply) {
    long v = strtoul(reply, &end, 16);
    if (reply != end) {
      req->reply = v;
    }
  } else {
    req->reply = 0;
  }
}

static void
try_send(PlcIoServer *server)
{
  if (!server->pending_request) {
    struct CmdRequest *req = g_queue_pop_head(server->requests);
    if (req) {
      char *cmd;
      g_debug("Request: %s",req->cmd);
      cmd = build_command(req);
      if (cmd) {
	plc_io_comm_send(server->comm, cmd, NULL);
	g_free(cmd);
	server->pending_request = req;
      } else {
	cmd_request_free(req);
      }
    }
  }
}

static void
reply_handler(PlcIoComm *comm, PLCCmd *cmd, PlcIoServer *server)
{
  g_debug("reply_handler");
  if (server->pending_request) {
    parse_reply(cmd->reply, server->pending_request);
    g_queue_push_tail(server->replies, server->pending_request);
    libwebsocket_callback_on_writable(WEBSOCKET_SERVER(server)->ws_context,
				      server->pending_request->wsi);
    server->pending_request = NULL;
  }
  try_send(server);
}

static void
cmd_request_send(PlcIoServer *server,
		 struct libwebsocket *wsi,
		 const gchar *cmd, gssize len)
{
  GError *err = NULL;
  if (json_parser_load_from_data(server->json, cmd, len, &err)) {
    JsonNode *value_node;
    JsonNode *root = json_parser_get_root (server->json);
    if (JSON_NODE_HOLDS_OBJECT(root)) {
      struct CmdRequest *req = g_new(struct CmdRequest, 1);
      JsonObject *root_obj = json_node_get_object(root);
      value_node = json_object_get_member(root_obj, "cmd");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	req->cmd = json_node_dup_string(value_node);
      } else {
	g_free(req);
	return;
      }
      req->addr = 0;
      value_node = json_object_get_member(root_obj, "addr");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	req->addr = json_node_get_int(value_node);
      }
      req->value = 0;
      value_node = json_object_get_member(root_obj, "value");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	req->value = json_node_get_int(value_node);
      }
      req->mask = 0xffff;
      value_node = json_object_get_member(root_obj, "mask");
      if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
	req->mask = json_node_get_int(value_node);
      }
      req->wsi = wsi;
      
      g_queue_push_tail(server->requests, req);
      try_send(server);
    }
  } else {
    g_warning("Failed to parse JSON request: %s", err->message);
    g_clear_error(&err);
  }
}

static gint
request_wsi_compare(gconstpointer a, gconstpointer b)
{
  return (char*)((struct CmdRequest*)a)->wsi - (char*)b;
}
static struct CmdRequest *
cmd_request_get(PlcIoServer *server,
		struct libwebsocket *wsi)
{
  struct CmdRequest *req;
  GList *li = g_queue_find_custom(server->replies, wsi, request_wsi_compare);
  if (!li) return NULL;
  req = li->data;
  g_queue_delete_link(server->replies, li);
  return req;
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
  PlcIoServer *server = libwebsocket_context_user (context);
  switch(reason) {
  case LWS_CALLBACK_ESTABLISHED:
    g_debug("PLC_IO established");
    break;
  case LWS_CALLBACK_CLOSED:
    {
      struct CmdRequest *req;
      if (server->requests) {
	/* Discard all pending requests for this connection */
	while(TRUE) {
	  GList *li =
	    g_queue_find_custom(server->requests, wsi, request_wsi_compare);
	  if (!li) break;;
	  req = li->data;
	  cmd_request_free(req);
	  g_queue_delete_link(server->requests, li);
	}
      }
      
      if (server->replies) {
	/* Discard all pending replies */
	while((req = cmd_request_get(server, wsi))) {
	  cmd_request_free(req);
	}
      }
      if (server->pending_request && server->pending_request->wsi == wsi) {
	cmd_request_free(server->pending_request);
	server->pending_request = NULL;
      }
      g_debug("PLC_IO closed");
    }
    break;
  case LWS_CALLBACK_PROTOCOL_DESTROY:
    g_debug("PLC_IO protocol destroy");
    break;
    
  case  LWS_CALLBACK_SERVER_WRITEABLE:
    {
      struct CmdRequest *req;
      while((req = cmd_request_get(server, wsi))) {
	GString *reply = g_string_new_len(padding, LWS_SEND_BUFFER_PRE_PADDING);
	g_string_append_printf(reply, "{cmd:\"%s\",", req->cmd);
	g_string_append_printf(reply, "addr:%d,", req->addr);
	g_string_append_printf(reply, "mask:%d,", req->mask);
	g_string_append_printf(reply, "value:%d,", req->value);
	g_string_append_printf(reply, "reply:%d}", req->reply);
	g_string_append_len(reply, padding, LWS_SEND_BUFFER_POST_PADDING);
	libwebsocket_write(wsi,((unsigned char*)reply->str 
				+ LWS_SEND_BUFFER_PRE_PADDING),
			   (reply->len - LWS_SEND_BUFFER_PRE_PADDING 
			    - LWS_SEND_BUFFER_POST_PADDING),
			   LWS_WRITE_TEXT);
	g_string_free(reply, TRUE);
	cmd_request_free(req);
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
