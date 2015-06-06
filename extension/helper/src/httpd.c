#include <config.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#define MHD_PLATFORM_H
#include "microhttpd.h"
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include "httpd.h"
 #include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "httpd_marshal.h"

GQuark
http_server_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("http-server-error-quark");
  return error_quark;
}


enum {
  CMD_RECEIVED,
  LAST_SIGNAL
};

static guint http_server_signals[LAST_SIGNAL] = {0 };

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

struct _HTTPServer
{
  GObject parent_instance;
  GMainContext *signal_context;
  struct MHD_Daemon *daemon;
  gchar *user;
  gchar *password;
  guint port;
  gchar *http_root;
  gchar *root_file; /* File served when accessing the root */
  
  GRWLock value_lock; /* Protects any access to values in this instance */

  GMutex signal_lock;
  GSource *signal_idle_source;
  GData *pending_changes;
};

struct _HTTPServerClass
{
  GObjectClass parent_class;

  /* class members */

  /* Signals */
  gchar * (*cmd_received)(HTTPServer *server, const char *cmd, gpointer *params);
};


G_DEFINE_TYPE (HTTPServer, http_server, G_TYPE_OBJECT)

static void
dispose(GObject *gobj)
{
  HTTPServer *server = HTTP_SERVER(gobj);
  if (server->daemon) {
    MHD_stop_daemon(server->daemon);
    server->daemon = NULL;
  }


  g_free(server->user);
  server->user = NULL;
  g_free(server->password);
  server->password = NULL;
  g_free(server->http_root);
  server->http_root = NULL;
  g_free(server->root_file);
  server->root_file = NULL;
  g_datalist_clear(&server->pending_changes);
  if (server->signal_idle_source) {
    g_source_destroy(server->signal_idle_source);
    g_source_unref(server->signal_idle_source);
    server->signal_idle_source= NULL;
  }
  if (server->signal_context) {
    g_main_context_unref(server->signal_context);
    server->signal_context = NULL;
  }
  
  G_OBJECT_CLASS(http_server_parent_class)->dispose(gobj);
}

static void
finalize(GObject *gobj)
{
  HTTPServer *server = HTTP_SERVER(gobj);
  g_rw_lock_clear(&server->value_lock);
  g_mutex_clear(&server->signal_lock);
  G_OBJECT_CLASS(http_server_parent_class)->finalize(gobj);
}

static void
set_property (GObject *object, guint property_id,
	      const GValue *value, GParamSpec *pspec)
{
  HTTPServer *server = HTTP_SERVER(object);
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
  HTTPServer *server = HTTP_SERVER(object);
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

static gchar *
cmd_received(HTTPServer *server,const char *cmd, gpointer *params)
{
  return NULL;
}

/* Ignore NULL strings */
static gboolean
cmd_result_accumulator(GSignalInvocationHint *ihint,
                       GValue *return_accu,
                       const GValue *handler_return,
                       gpointer data)
{
  const gchar *res = g_value_get_string(handler_return);
  if (res != NULL) {
    g_value_set_string(return_accu, res);
  }
  return TRUE;
}

static void
http_server_class_init (HTTPServerClass *klass)
{
  GParamSpec *properties[N_PROPERTIES];
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  HTTPServerClass *server_class = HTTP_SERVER_CLASS(klass);
  gobject_class->dispose = dispose;
  gobject_class->finalize = finalize;
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;
  server_class->cmd_received = cmd_received;
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
  http_server_signals[CMD_RECEIVED] =
    g_signal_new("cmd-received",
		 G_OBJECT_CLASS_TYPE (gobject_class),
		 G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
		 G_STRUCT_OFFSET(HTTPServerClass, cmd_received),
		 cmd_result_accumulator, NULL,
		 httpd_marshal_STRING__UINT_POINTER,
		 G_TYPE_STRING, 2, G_TYPE_UINT, G_TYPE_POINTER);
}

static void
http_server_init(HTTPServer *server)
{
  server->daemon = NULL;
  server->user = NULL;
  server->password = NULL;
  server->port = 8080;
  server->http_root = NULL;
  server->root_file = NULL;
  g_rw_lock_init(&server->value_lock);
  server->signal_context = g_main_context_ref_thread_default();
  g_mutex_init(&server->signal_lock);
  server->signal_idle_source = NULL;
  g_datalist_init(&server->pending_changes);
}

static void
signal_each_update(GQuark key_id, gpointer data, gpointer user_data)
{
  /*HTTPServer *server = user_data;*/

}

static gboolean
idle_notify_modification(gpointer user_data)
{
  HTTPServer *server = user_data;
  GData *pending;
  g_mutex_lock(&server->signal_lock);
  /* Make a private copy of the list and clear the list in the object so
     the server can use it for new updates. */
  pending = server->pending_changes;
  g_datalist_init(&server->pending_changes);
  g_source_unref(server->signal_idle_source);
  server->signal_idle_source = NULL;
  /* Don't need the lock anymore since we have a private copy of the list. */
  g_mutex_unlock(&server->signal_lock);
  /* Emit the signal for each node that has changed */
  g_datalist_foreach(&pending, signal_each_update, server);
  g_datalist_clear(&pending);
  return FALSE;
}

void
notify_modification(HTTPServer *server)
{
  g_mutex_lock(&server->signal_lock);
  if (!server->signal_idle_source) {
    server->signal_idle_source = g_idle_source_new();
    g_source_set_callback (server->signal_idle_source,
			   idle_notify_modification, server, NULL);
    g_source_attach(server->signal_idle_source, server->signal_context);
  }
  g_mutex_unlock(&server->signal_lock);
}

static ssize_t
string_content_handler(void *user_data, uint64_t pos, char *buf, size_t max)
{
  memcpy(buf, ((char*)user_data)+pos, max);
  return max;
}

static void
string_content_handler_free(void *user_data)
{
  g_free(user_data);
}

static int
error_response(struct MHD_Connection * connection,
		      int response, const char *response_msg,
		      const char *detail)
{
  gchar *resp_str;
  struct MHD_Response * resp;
  static const char resp_format[] =
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">\n"
    "<html> <head><title>%d %s</title></head>\n"
    "<body><h1>%s</h1>\n"
    "<p>%s</p>"
    "</body> </html>\n";
  if (!response_msg) response_msg = "Error";
  if(!detail) detail = "";
  resp_str = g_strdup_printf(resp_format, response,
			     response_msg, response_msg, detail);

  resp = MHD_create_response_from_callback(strlen(resp_str), 1024, string_content_handler, resp_str, string_content_handler_free);
  MHD_add_response_header(resp, "Content-Type",
			   "text/html;charset=UTF-8");
  MHD_queue_response(connection, response, resp);
  MHD_destroy_response(resp);
  return MHD_YES;
}



typedef struct _ConnectionContext ConnectionContext;

struct _ConnectionContext
{
  /* Set to NULL if this is a new request */
  int (*request_handler)(HTTPServer *server, ConnectionContext *cc,
			 struct MHD_Connection * connection,
			 const char *url, const char *method,
			 const char *version, const char *upload_data,
			 size_t *upload_data_size);
  gchar *content_type;
  GString *post_content;
};


static gboolean
check_auth(HTTPServer *server, struct MHD_Connection *connection)
{
  gboolean  res;
  char *user;
  char *pass = NULL;
  if (!server->user || !server->password) return TRUE;
  user = MHD_basic_auth_get_username_password (connection, &pass);
  res = user && pass && strcmp(user, server->user) == 0 && strcmp(pass,server->password) == 0;
  if (user) free(user);
  if (pass) free(pass);
  return res;
}

static gboolean
check_filename(const char *url)
{
  unsigned int period_count = 0;
  char c = *url++;
  if (!(g_ascii_isalnum(c) || c == '_')) return FALSE;
  while(*url != '\0') {
    c = *url++;
    if (c == '.') period_count++;
    else { 
      if (!(g_ascii_isalnum(c) || c == '_')) return FALSE;
   }
  }
  if (period_count != 1) return FALSE;
  return TRUE;
}

static void
add_mime_type(struct MHD_Response *resp, const char *filename)
{
  const char *mime;
  char *ext = index(filename, '.');
  ext++;
  if (strcmp("html",ext) == 0 || strcmp("htm",ext) == 0) {
    mime = "text/html; charset=UTF-8";
  } else  if (strcmp("txt",ext) == 0) {
    mime = "text/plain; charset=UTF-8";
  } else  if (strcmp("svg",ext) == 0) {
    mime = "image/svg+xml";
  } else  if (strcmp("png",ext) == 0) {
    mime = "image/png";
  } else  if (strcmp("jpg",ext) == 0) {
    mime = "image/jpg";
  } else  if (strcmp("js",ext) == 0) {
    mime = "application/javascript";
  } else  if (strcmp("xml",ext) == 0) {
    mime = "text/xml";
  } else  if (strcmp("xhtml",ext) == 0) {
    mime = "application/xhtml+xml";
  } else  if (strcmp("ico",ext) == 0) {
    mime = "image/x-icon";
  } 
  MHD_add_response_header(resp, "Content-Type", mime);
}

static int
file_response(HTTPServer *server,
	      struct MHD_Connection * connection, const char *url)
{
  if (server->http_root) {
    if (url[0] == '\0') {
      if (server->root_file)
	url = server->root_file;
    }
    if (check_filename(url)) {
      int fd;
      gchar * filename = g_build_filename(server->http_root, url, NULL);
      fd = open(filename, O_RDONLY);
      if (fd < 0) {
	g_warning("Failed to open file %s: %s", filename, strerror(errno));
      } else {
	struct stat status;
	if (fstat(fd, &status) >= 0) {
	  struct MHD_Response * resp;
	  resp = MHD_create_response_from_fd_at_offset(status.st_size, fd, 0);
	  add_mime_type(resp,filename); 
	  MHD_queue_response(connection, MHD_HTTP_OK, resp);
	  MHD_destroy_response(resp);
	  g_free(filename);
	  return MHD_YES;
	} else {
	  close(fd);
	}
      }
      g_free(filename);
    }
  }
  error_response(connection, MHD_HTTP_NOT_FOUND, "Not Found", NULL);
  return MHD_YES;
}

#if 0
static void
g_value_free(gpointer d)
{
  GValue *v = d;
  g_value_unset(v);
  g_free(v);
}


static void
pending_change(HTTPServer *server, const gchar *pathstr, const GValue *value)
{
  GValue *v = g_new0(GValue,1);
  g_value_init(v, G_VALUE_TYPE(value));
  g_value_copy(value, v);
  g_datalist_set_data_full(&server->pending_changes, pathstr, v, g_value_free);
}
#endif

static int
poll_response(HTTPServer *server,
	      struct MHD_Connection * connection)
{
  gchar *resp_str;
  struct MHD_Response * resp;
  g_rw_lock_reader_lock(&server->value_lock);
  resp_str = g_strdup("i0 1");
  resp = MHD_create_response_from_callback(strlen(resp_str), 1024, string_content_handler, resp_str, string_content_handler_free);
  MHD_add_response_header(resp, "Content-Type",
			  "text/plain");
   MHD_add_response_header(resp, "Cache-Control",
			  "no-cache");
  MHD_queue_response(connection, MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  
  g_rw_lock_reader_unlock(&server->value_lock);
  return MHD_YES; 
}

#define MAX_PARAMETERS 10
static int
cmd_response(HTTPServer *server,
	     struct MHD_Connection * connection, const gchar *url)
{
  struct MHD_Response * resp;
  gchar *params[MAX_PARAMETERS];
  gint param_count = 0;
  GQuark cmd_quark = 0;
  gchar *result = NULL;
  gchar *cmd_str = g_strdup(url); /* Make a copy we can modify */
  gchar *cmd = cmd_str;
  while(param_count < MAX_PARAMETERS-1) { /* Leave place for terminating NULL */
    params[param_count++] = cmd;
    while(*cmd != '/' && *cmd != '\0') cmd++;
    if (*cmd == '\0') break;
    *cmd++ = '\0';
  }
  params[param_count] = NULL;  /* Terminate string array */
  cmd_quark = g_quark_from_string(params[0]);
  g_signal_emit(server, http_server_signals[CMD_RECEIVED],
		cmd_quark, cmd_quark, params+1, &result);
  g_free(cmd_str);
  g_debug("Result: %s", result);

  if (!result) {
    result = g_strdup("");
  }
  g_rw_lock_reader_lock(&server->value_lock);
  resp = MHD_create_response_from_callback(strlen(result), 1024, string_content_handler, result, string_content_handler_free);
  MHD_add_response_header(resp, "Content-Type",
			  "text/plain");
  MHD_add_response_header(resp, "Cache-Control",
			  "no-cache");
  MHD_queue_response(connection, MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  
  g_rw_lock_reader_unlock(&server->value_lock);
  return MHD_YES; 
}

static int
crossdomain_response(HTTPServer *server,
		     struct MHD_Connection * connection)
{
  gchar *resp_str;
  struct MHD_Response * resp;
  g_rw_lock_reader_lock(&server->value_lock);
  resp_str =
    g_strdup_printf("<cross-domain-policy>\n"
		    " <allow-access-from domain=\"*\" to-ports=\"%d\"/>\n"
		    "</cross-domain-policy>", server->port);
  resp = MHD_create_response_from_callback(strlen(resp_str)+1, 1024, string_content_handler, resp_str, string_content_handler_free);
  MHD_add_response_header(resp, "Content-Type",
			  "text/xml");
  MHD_queue_response(connection, MHD_HTTP_OK, resp);
  MHD_destroy_response(resp);
  
  g_rw_lock_reader_unlock(&server->value_lock);
  return MHD_YES; 
}

gboolean ScratchCommandFunc(gpointer *user_data, const gchar *parameters, GError **err);

#define TOP_INDEX_FILE "index.xhtml"
static int
handle_GET_request(HTTPServer *server, ConnectionContext *cc,
			 struct MHD_Connection * connection,
			 const char *url, const char *method,
			 const char *version, const char *upload_data,
			 size_t *upload_data_size)
{
  if (*url == '\0' || (url[0] == '/' && url[1] == '\0')) {
    return file_response(server, connection, TOP_INDEX_FILE);
  }
  if (*url == '/') {
    /* g_debug("Path: %s\n", url); */
    url++;
    if (strncmp("files", url, 5) == 0) {
      url+=5;
      if (*url == '/') url++;
      return file_response(server, connection, url);
    } else if (strncmp("poll", url, 4) == 0) {
      return poll_response(server, connection);
    } else if (strcmp("crossdomain.xml",url) == 0) {
      return crossdomain_response(server, connection);
    } else {
      g_debug("Cmd: %s", url);
      cmd_response(server, connection, url);
    }

  }
  return error_response(connection, MHD_HTTP_NOT_FOUND, "Not Found", NULL);
}




static int
handle_POST_request(HTTPServer *server, ConnectionContext *cc,
			 struct MHD_Connection * connection,
			 const char *url, const char *method,
			 const char *version, const char *upload_data,
			 size_t *upload_data_size)
{
  if (!cc->content_type || strncmp(cc->content_type, "application/json",16) != 0) {
    gchar detail[100];
    g_snprintf(detail, sizeof(detail), "Only application/json supported for POST (got %s)", cc->content_type ? cc->content_type : "none");
    g_debug(detail);
    return error_response(connection, MHD_HTTP_UNSUPPORTED_MEDIA_TYPE,
			  "Unsupported Media TYpe",
			  detail);
  }

  return error_response(connection, MHD_HTTP_UNSUPPORTED_MEDIA_TYPE,
			"Unsupported Media TYpe",
			"");
}

void request_completed(void *user_data, struct MHD_Connection *connection,
		       void **con_cls, enum MHD_RequestTerminationCode toe)
{
  ConnectionContext *cc = *con_cls;
  if (cc->post_content) {
    g_string_free(cc->post_content, TRUE);
  }
  g_free(cc->content_type);
  g_free(cc);
  *con_cls = NULL;
  /* g_debug("Request completed"); */
}
		       
static int 
request_handler(void *user_data, struct MHD_Connection * connection,
		const char *url, const char *method,
		const char *version, const char *upload_data,
		size_t *upload_data_size, void **con_cls)
{
  int res = MHD_YES;
  HTTPServer *server = user_data;
  ConnectionContext *conctxt = *con_cls;
  if (!conctxt) {
    /* g_debug("New request"); */
    conctxt = g_new(ConnectionContext,1);
    conctxt->request_handler = NULL;
    conctxt->content_type =
      g_strdup(MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
					   "Content-Type"));
    conctxt->post_content = NULL;
    *con_cls = conctxt;

    if (!check_auth(server, connection)) {
      struct MHD_Response *resp;
      static const char resp_str[] =
	"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">\n"
	"<html> <head><title>Not authorized</title></head>\n"
	"<body><h1>Not authorized</h1>\n"
	"<p>You have supplied the wrong username and/or password</p>"
	"</body> </html>\n";
      
      resp = MHD_create_response_from_buffer(strlen(resp_str), (char*) resp_str,
					     MHD_RESPMEM_PERSISTENT);
      MHD_add_response_header(resp, "Content-Type",
			      "text/html; UTF-8");
      res = MHD_queue_basic_auth_fail_response (connection, "Scratch extension server",
						resp);
      MHD_destroy_response(resp);
      return res;
    }
  }

  if (!conctxt->request_handler) {
    if (strcmp(method, "GET") == 0) {
      conctxt->request_handler = handle_GET_request;
    } else if (strcmp(method, "POST") == 0) {
      conctxt->request_handler = handle_POST_request;
    } else {
      return error_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method Not Allowed", NULL);
    }
  }
  return conctxt->request_handler(server, conctxt, connection,
				  url, method, version,
				  upload_data, upload_data_size);
  }


HTTPServer *
http_server_new(void)
{
  HTTPServer *server = g_object_new (HTTP_SERVER_TYPE, NULL);
  return server;
}

gboolean
http_server_start(HTTPServer *server, GError **err)
{
   static const struct MHD_OptionItem ops[] = {
    { MHD_OPTION_CONNECTION_LIMIT, 10, NULL },
    { MHD_OPTION_CONNECTION_TIMEOUT, 60, NULL },
    { MHD_OPTION_END, 0, NULL }
  };
  
  
   server->daemon =
     MHD_start_daemon((MHD_USE_DEBUG|MHD_USE_THREAD_PER_CONNECTION
		       |MHD_USE_POLL|MHD_USE_PEDANTIC_CHECKS),
		      server->port, NULL, NULL, request_handler, server,
		      MHD_OPTION_ARRAY, ops,
		      MHD_OPTION_NOTIFY_COMPLETED, request_completed, server,
		      MHD_OPTION_END);
   if (!server->daemon) {
     g_set_error(err, HTTP_SERVER_ERROR, HTTP_SERVER_ERROR_START_FAILED,
		 "Failed to start HTTP daemon");
   }
   return TRUE;
}
