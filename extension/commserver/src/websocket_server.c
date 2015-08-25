#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include "websocket_server_private.h"
 #include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>

/* #include "httpd_marshal.h" */

GQuark
websocket_server_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("websocket-server-error-quark");
  return error_quark;
}


enum {
  LAST_SIGNAL
};

#if 0
static guint websocket_server_signals[LAST_SIGNAL] = {0 };
#endif

enum
{
  PROP_0 = 0,
  PROP_PORT,
  PROP_USER,
  PROP_PASSWORD,
  PROP_HTTP_ROOT,
  PROP_ROOT_FILE,
  N_PROPERTIES
};




G_DEFINE_TYPE (WebsocketServer, websocket_server, G_TYPE_OBJECT)

static void
dispose(GObject *gobj)
{
  WebsocketServer *server = WEBSOCKET_SERVER(gobj);

  if (server->ws_context) {
    libwebsocket_context_destroy(server->ws_context);
    server->ws_context = NULL;
  }
  if (server->source) {
    g_source_destroy(&server->source->source);
    g_source_unref(&server->source->source);
    server->source = NULL;
  }

  g_free(server->user);
  server->user = NULL;
  g_free(server->password);
  server->password = NULL;
  g_free(server->http_root);
  server->http_root = NULL;
  g_free(server->root_file);
  server->root_file = NULL;

  if (server->ws_info) {
    g_free(server->ws_info->protocols);
    g_free(server->ws_info);
    server->ws_info = NULL;
  }
  
  G_OBJECT_CLASS(websocket_server_parent_class)->dispose(gobj);
}

static void
finalize(GObject *gobj)
{
  /* WebsocketServer *server = WEBSOCKET_SERVER(gobj); */
  G_OBJECT_CLASS(websocket_server_parent_class)->finalize(gobj);
}

static void
set_property (GObject *object, guint property_id,
	      const GValue *value, GParamSpec *pspec)
{
  WebsocketServer *server = WEBSOCKET_SERVER(object);
  switch (property_id)
    {
    case PROP_PORT:
      server->port = g_value_get_uint(value);
      break;
    case PROP_USER:
      g_free(server->user);
      server->user = g_value_dup_string(value);
      break;
    case PROP_PASSWORD:
      g_free(server->password);
      server->password = g_value_dup_string(value);
      break;
    case PROP_HTTP_ROOT:
      g_free(server->http_root);
      server->http_root = g_value_dup_string(value);
      break;
    case PROP_ROOT_FILE:
      g_free(server->root_file);
      server->root_file = g_value_dup_string(value);
      break;
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
  WebsocketServer *server = WEBSOCKET_SERVER(object);
  switch (property_id) {
  case PROP_PORT:
    g_value_set_uint(value, server->port);
    break;
  case PROP_USER:
    g_value_set_string(value,server->user);
    break;
  case PROP_PASSWORD:
    g_value_set_string(value, server->password);
    break;
  case PROP_HTTP_ROOT:
    g_value_set_string(value, server->http_root);
    break;
  case PROP_ROOT_FILE:
    g_value_set_string(value, server->root_file);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

guint
websocket_server_add_protocols(WebsocketServer *server,
			       guint allocate_extra,
			       struct libwebsocket_protocols **protocols,
			       const struct libwebsocket_protocols 
			       *added_protocols,
			       guint n_added_protocols)
{
  guint n;
  WebsocketServerClass *sclass = WEBSOCKET_SERVER_GET_CLASS(server);
  WebsocketServerClass *parent = g_type_class_peek_parent (sclass);
  allocate_extra += n_added_protocols;
  n = parent->get_protocols(server, allocate_extra,
			    protocols);
  memcpy((*protocols) + (n - allocate_extra), added_protocols,
	 sizeof(struct libwebsocket_protocols) * n_added_protocols);
  return n;
}

static int
http_callback(struct libwebsocket_context *context,
	      struct libwebsocket *wsi,
	      enum libwebsocket_callback_reasons reason, 
	      void *user,
	      void *in, size_t len);

static const struct libwebsocket_protocols protocols[] = 
{
  {"HTTP", http_callback, 0, 100, .owning_server = NULL, .protocol_index = 0},
};

static guint
get_protocols(WebsocketServer *server,
	       guint allocate_extra,
	       struct libwebsocket_protocols **protocolsp)
{
  /* Add one extra for null termination */
  *protocolsp = g_new0(struct libwebsocket_protocols, allocate_extra + 2);
  memcpy(*protocolsp, protocols, sizeof(struct libwebsocket_protocols));
  return allocate_extra + 1;
}

static void
websocket_server_class_init (WebsocketServerClass *klass)
{
  GParamSpec *properties[N_PROPERTIES];
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  WebsocketServerClass *server_class = WEBSOCKET_SERVER_CLASS(klass);
  gobject_class->dispose = dispose;
  gobject_class->finalize = finalize;
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;
  server_class->get_protocols = get_protocols;
  properties[0] = NULL;
  properties[PROP_USER]
    =  g_param_spec_string("user", "User", "User for athentication",
			   NULL, G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS);
  properties[PROP_PASSWORD]
    =  g_param_spec_string("password", "Password", "Password for athentication",
			   NULL, G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS);
  properties[PROP_PORT]
    = g_param_spec_uint("http-port", "HTTP port", "HTTP port number",
			1, 65535, 8080,
			G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS);
  properties[PROP_HTTP_ROOT]
    =  g_param_spec_string("http-root", "HTTP root", "Root directory",
			   NULL, G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS);
  properties[PROP_ROOT_FILE]
    =  g_param_spec_string("root-file", "Root file",
			   "File served for root access.",
			   NULL, G_PARAM_READWRITE |G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties(gobject_class, N_PROPERTIES, properties);
}

static void
websocket_server_init(WebsocketServer *server)
{
  server->user = NULL;
  server->password = NULL;
  server->port = 8080;
  server->http_root = NULL;
  server->root_file = NULL;
  server->ws_info = NULL;
  server->ws_context = NULL;
  server->source = NULL;
}

WebsocketServer *
websocket_server_new(void)
{
  WebsocketServer *server = g_object_new (WEBSOCKET_SERVER_TYPE, NULL);
  return server;
}

struct WebsocketServerPollList
{
  GPollFD pollfd;
  struct WebsocketServerPollList *next;
};

static void
add_poll_fd(WebsocketServer *server, struct libwebsocket_pollargs *poll)
{
  struct WebsocketServerPollList *pl = server->source->poll_fds;
  /* Look for an empty slot */
  while(pl) {
    if (pl->pollfd.fd < 0) {
      break;
    }
    pl = pl->next;
  }
  if (!pl) {
    pl = g_new(struct WebsocketServerPollList, 1);
    pl->next = server->source->poll_fds;
    server->source->poll_fds = pl;
  }
  pl->pollfd.fd = poll->fd;
  pl->pollfd.events = poll->events;
  pl->pollfd.revents = 0;
  g_source_add_poll(&server->source->source, &pl->pollfd);
  g_debug("Added fd %d", pl->pollfd.fd);
}

static void
del_poll_fd(WebsocketServer *server, struct libwebsocket_pollargs *poll)
{
  struct WebsocketServerPollList *pl = server->source->poll_fds;
   while(pl) {
    if (pl->pollfd.fd == poll->fd) {
      g_source_remove_poll(&server->source->source, &pl->pollfd);
      g_debug("Removed fd %d", pl->pollfd.fd);
      pl->pollfd.fd = -1;
      break;
    }
    pl = pl->next;
  }
}

static void
change_poll_fd(WebsocketServer *server, struct libwebsocket_pollargs *poll)
{
  struct WebsocketServerPollList *pl = server ->source->poll_fds;
  while(pl) {
    if (pl->pollfd.fd == poll->fd) {
      pl->pollfd.events = poll->events;
      break;
    }
    pl = pl->next;
  }
}

static const char *reason_str[] = {
  "ESTABLISHED",
  "CLIENT_CONNECTION_ERROR",
  "CLIENT_FILTER_PRE_ESTABLISH",
  "CLIENT_ESTABLISHED",
  "CLOSED",
  "CLOSED_HTTP",
  "RECEIVE",
  "CLIENT_RECEIVE",
  "CLIENT_RECEIVE_PONG",
  "CLIENT_WRITEABLE",
  "SERVER_WRITEABLE",
  "HTTP",
  "HTTP_BODY",
  "HTTP_BODY_COMPLETION",
  "HTTP_FILE_COMPLETION",
  "HTTP_WRITEABLE",
  "FILTER_NETWORK_CONNECTION",
  "FILTER_HTTP_CONNECTION",
  "SERVER_NEW_CLIENT_INSTANTIATED",
  "FILTER_PROTOCOL_CONNECTION",
  "OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS",
  "OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS",
  "OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION",
  "CLIENT_APPEND_HANDSHAKE_HEADER",
  "CONFIRM_EXTENSION_OKAY",
  "CLIENT_CONFIRM_EXTENSION_SUPPORTED",
  "PROTOCOL_INIT",
  "PROTOCOL_DESTROY",
  "WSI_CREATE", /* always protocol[0] */
  "WSI_DESTROY", /* always protocol[0] */
  "GET_THREAD_ID",

  /* external poll() management support */
  "ADD_POLL_FD",
  "DEL_POLL_FD",
  "CHANGE_MODE_POLL_FD",
  "LOCK_POLL",
  "UNLOCK_POLL",

  "OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY",
};

static struct
{
  char *ext;
  char *mime_type;
} ext_mime_type_map [] =
  {
    {"ico", "image/x-icon"},
     {"png", "image/png"},
    {"html", "text/html"},
    {"css", "text/css"},
    {"xhtml", "application/xhtml+xml"},
    {"js", "application/x-javascript"}
  };

static const char*
get_mimetype_from_ext(const char *file)
{
  int i;
  char *ext = rindex(file, '.');
  if (!ext) return NULL;
  ext++;
  for (i = 0; i < G_N_ELEMENTS(ext_mime_type_map); i++) {
    if (strcmp(ext, ext_mime_type_map[i].ext) == 0) {
      return ext_mime_type_map[i].mime_type;
    }
  }
  return NULL;
}
static int
http_callback(struct libwebsocket_context *context,
	      struct libwebsocket *wsi,
	      enum libwebsocket_callback_reasons reason, 
	      void *user,
	      void *in, size_t len)
{
  WebsocketServer *server = libwebsocket_context_user (context);
  switch(reason) {
  case LWS_CALLBACK_HTTP:
    {
      char *url = in;
      char *path;
      const char *mime_type;
      int n;
      if (len < 1) {
	libwebsockets_return_http_status(context, wsi,
					 HTTP_STATUS_BAD_REQUEST, NULL);
	goto try_to_reuse;
      }
      if (url[0] == '/') url++;
      if (index(url, '/')) {
	libwebsockets_return_http_status(context, wsi,
					 HTTP_STATUS_FORBIDDEN,
					 "Subdirectories not allowed");
	goto try_to_reuse;
      }
      g_debug("GET '%s'", url);
      if (url[0] == '\0') { /* Use default file*/
	url = "index.xhtml";
      }
      mime_type = get_mimetype_from_ext(url);
      if (!mime_type) {
	libwebsockets_return_http_status(context, wsi,
					 HTTP_STATUS_FORBIDDEN,
					 "Unhandled file type");
	goto try_to_reuse;
      }
      path = g_strdup_printf("%s/%s", server->http_root, url);
      n = libwebsockets_serve_http_file(context, wsi, path,
					mime_type, NULL, 0);
      g_free(path);
      if (n < 0 || ((n > 0) && lws_http_transaction_completed(wsi)))
	return -1;
      goto try_to_reuse;
    }
    break;
  case LWS_CALLBACK_ADD_POLL_FD:
    add_poll_fd(server, (struct libwebsocket_pollargs*)in);
    break;
  case LWS_CALLBACK_DEL_POLL_FD:
    del_poll_fd(server, (struct libwebsocket_pollargs*)in);
    break;
  case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
    change_poll_fd(server, (struct libwebsocket_pollargs*)in);
    break;
  case LWS_CALLBACK_LOCK_POLL:
      break;
  case LWS_CALLBACK_UNLOCK_POLL:
    break;
  default:
    if (reason < G_N_ELEMENTS(reason_str)) {
      g_debug("Unhandled reason: %d %s",reason,reason_str[reason]);
    } else {
      g_debug("Unknown reason %d", reason);
    }
  }
  return 0;
 try_to_reuse:
  if (lws_http_transaction_completed(wsi)) {
    return -1;
  }
  g_debug("Reuse");
  
  return 0;
}



static gboolean source_prepare (GSource    *source,
				gint       *timeout_)
{
  return FALSE;
}

static gboolean source_check(GSource    *source)
{
  WebsocketSource *ws_source = (WebsocketSource*)source;
  struct WebsocketServerPollList *pl = ws_source->poll_fds;
  while(pl) {
    if (pl->pollfd.fd >= 0 && pl->pollfd.revents != 0) {
      g_debug("source_check: match %d", pl->pollfd.fd);
      return TRUE;
    }
    pl = pl->next;
  }
  return FALSE;
}

static gboolean
source_dispatch(GSource    *source,
		GSourceFunc callback,
		gpointer    user_data)
{
  WebsocketSource *ws_source = (WebsocketSource*)source;
  struct WebsocketServerPollList *pl;
  pl = ws_source->poll_fds;
  while(pl) {
    if (pl->pollfd.fd >= 0 && pl->pollfd.revents != 0) {
      g_debug("source_dispatch: %d", pl->pollfd.fd);
      libwebsocket_service_fd(ws_source->server->ws_context, 
			      (struct pollfd*)&pl->pollfd);
    }
    pl = pl->next;
  }
  return TRUE;
}

static void
source_finalize(GSource    *source)
{
  WebsocketSource *ws_source = (WebsocketSource*)source;
  if (ws_source->poll_fds) {
    struct WebsocketServerPollList *pl = ws_source->poll_fds;
    while(pl) {
      struct WebsocketServerPollList *next = pl->next;
      g_free(pl);
      pl = next;
    }
    ws_source->poll_fds = NULL;
  }
}

static GSourceFuncs source_funcs = {
  source_prepare,
  source_check,
  source_dispatch,
  source_finalize
};


gboolean
websocket_server_start(WebsocketServer *server, GError **err)
{
  if (!server->source) {
    WebsocketSource *ws_source = 
      (WebsocketSource*)g_source_new(&source_funcs, sizeof(WebsocketSource));
    ws_source->poll_fds = NULL;
    ws_source->server = server;
    server->source = ws_source; 
  }

  server->ws_info = g_new0(struct lws_context_creation_info, 1);
  server->ws_info->port = server->port;
  server->ws_info->iface = NULL;
  server->ws_info->protocols = g_new(struct libwebsocket_protocols,
				     G_N_ELEMENTS(protocols));
  WEBSOCKET_SERVER_GET_CLASS(server)->get_protocols(server,
						0,
						&server->ws_info->protocols);
  
  server->ws_info->gid = -1;
  server->ws_info->uid = -1;
  server->ws_info->options = 0;
  server->ws_info->user = server;
  server->ws_info->ka_time = 5; 
  server->ws_info->ka_probes = 3;
  server->ws_context = libwebsocket_create_context(server->ws_info);
  if (server->ws_context == NULL) {
    g_set_error(err, 
		WEBSOCKET_SERVER_ERROR, WEBSOCKET_SERVER_ERROR_START_FAILED,
		"libwebsocket init failed\n");
    return FALSE;
  }
#if 0
  g_debug("Starting service");
  libwebsocket_service(server->ws_context,0);
  g_debug("Ended service");
#endif
  g_source_attach(&server->source->source, NULL);
  return TRUE;
}
