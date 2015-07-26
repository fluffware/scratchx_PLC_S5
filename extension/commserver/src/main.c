#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib-unix.h>
#include <PLC_IO_server.h>
#include <json-glib/json-glib.h>
#include <syslog.h>
#include <serial.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static gboolean
sigint_handler(gpointer user_data)
{
  g_main_loop_quit(user_data);
  return TRUE;
}

typedef struct AppContext AppContext;
struct AppContext
{
  char *config_filename;
  gboolean daemon;
  GLogLevelFlags log_level_mask;
  char *pid_filename;
  WebsocketServer *websocket_server;
  int http_port;
  char *http_root;
  char *serial_device;
};
  
AppContext app_ctxt  = {
  NULL,
  FALSE,
  ~G_LOG_LEVEL_DEBUG,
  NULL,
  NULL,
  8080,
  NULL,
  NULL
};


static void
app_init(AppContext *app)
{
  /*  values_quark = g_quark_from_static_string("c2ip-connection-values"); */
  app->serial_device = g_strdup("/dev/ttyUSB0");
}


static void
app_cleanup(AppContext* app)
{
  g_clear_object(&app->websocket_server);
  g_free(app->config_filename);
  g_free(app->http_root);
  if (app->daemon) {
    closelog();
  }
  g_free(app->serial_device);
}


static gboolean
read_config_file(AppContext *app, GError **err)
{
  JsonNode *root;
  JsonParser *parser;
  parser = json_parser_new ();
  if (!json_parser_load_from_file (parser, app->config_filename, err)) {
    g_object_unref(parser);
    return FALSE;
  }
  root = json_parser_get_root(parser);
  if (JSON_NODE_HOLDS_OBJECT(root)) {
    JsonNode *value_node;
    JsonObject *root_obj = json_node_get_object(root);
    value_node = json_object_get_member(root_obj, "extensionPort");
    if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
      app->http_port = json_node_get_int(value_node);
    }
    value_node = json_object_get_member(root_obj, "httpRootPath");
    if (value_node && JSON_NODE_HOLDS_VALUE(value_node)) {
      GFile *base;
      GFile *http_root;
      GFile *config;
      char *path = json_node_dup_string(value_node);
      config = g_file_new_for_path(app->config_filename);
      base = g_file_get_parent(config);
      g_object_unref(config);
      g_free(app->http_root);
      http_root = g_file_resolve_relative_path (base, path);
      g_free(path);
      g_object_unref(base);
      if (http_root) {
	app->http_root = g_file_get_path(http_root);
	g_debug("HTTP root: %s", app->http_root);
	g_object_unref(http_root);
      }
    }
  }
  g_object_unref(parser);
  return TRUE;
}


gint
g_value_get_as_int(const GValue *gvalue)
{
  GValue t = G_VALUE_INIT;
  gint v;
  g_value_init(&t, G_TYPE_INT);
  if (!g_value_transform(gvalue, &t)) {
    g_critical("Value can't be transformed to int");
    return 0;
  }
  v = g_value_get_int(&t);
  g_value_unset(&t);
  return v;
}


static void
configure_websocket_server(AppContext *app)
{
  /* GError *err = NULL; */
  g_object_set(app->websocket_server, "http-port", app->http_port, NULL);
  g_object_set(app->websocket_server, "http-root", app->http_root, NULL);
  
}



#define SYSLOG_IDENTITY "PLCS2_helper"

static void
syslog_handler(const gchar *log_domain, GLogLevelFlags log_level,
	      const gchar *message, gpointer user_data)
{
  AppContext *app = user_data;
  int pri = LOG_INFO;
  if ((log_level & app->log_level_mask) != log_level) return;
  switch(log_level)
    {
    case G_LOG_LEVEL_ERROR:
      pri = LOG_CRIT;
      break;
    case G_LOG_LEVEL_CRITICAL:
      pri = LOG_ERR;
      break;
    case G_LOG_LEVEL_WARNING:
      pri = LOG_WARNING;
      break;
    case G_LOG_LEVEL_MESSAGE:
      pri = LOG_NOTICE;
      break;
    case G_LOG_LEVEL_INFO:
      pri = LOG_INFO;
      break;
    case G_LOG_LEVEL_DEBUG:
      pri = LOG_DEBUG;
      break;
    default:
      break;
    }
  syslog(pri, "%s", message);
}

static gboolean
go_daemon(AppContext *app, GError **err)
{
  pid_t sid;
  pid_t pid;
  int pid_file;
  openlog(SYSLOG_IDENTITY, LOG_NDELAY | LOG_PID, LOG_USER);
  g_log_set_default_handler(syslog_handler, app);
  g_message("Logging started");
  
  pid = fork();
  if (pid < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"fork failed: %s", strerror(errno));
    return FALSE;
  }
  
  umask(S_IWGRP | S_IWOTH);

  if (pid > 0) {
    if (app->pid_filename) {
      pid_file = open(app->pid_filename, O_WRONLY | O_CREAT | O_TRUNC,
		      S_IRUSR | S_IWUSR | S_IRGRP|S_IROTH);
      if (pid_file >= 0) {
	char buffer[10];
	snprintf(buffer, sizeof(buffer), "%d\n", pid);
	if (write(pid_file, buffer, strlen(buffer)) <= 0) {
	  g_critical("Failed to write pid file\n");
	}
	close(pid_file);
      } else {
	g_critical("Failed to open pid file: %s\n", strerror(errno));
      }
    }
    exit(EXIT_SUCCESS);
  }
  

  sid = setsid();
  if (sid < 0) {
    syslog(LOG_ERR, "Could not create process group\n");
    exit(EXIT_FAILURE);
  }
  
  if ((chdir("/")) < 0) {
    syslog(LOG_ERR, "Could not change working directory to /\n");
    exit(EXIT_FAILURE);
  }

  
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  g_message("Daemon detached");
  
 
  return TRUE;
}


const GOptionEntry app_options[] = {
  {"config-file", 'c', 0, G_OPTION_ARG_FILENAME,
   &app_ctxt.config_filename, "Configuration file", "FILE"},
  {"daemon", 'd', 0, G_OPTION_ARG_NONE,
   &app_ctxt.daemon, "Detach and use syslog", NULL},
  {"pid-file", 'p', 0, G_OPTION_ARG_FILENAME,
   &app_ctxt.pid_filename, "Create a pid file when running as daemon",
   "FILE"},
  {"serial-device", 'd', 0, G_OPTION_ARG_STRING,
   &app_ctxt.serial_device, "Serial device for connection to PLC", "DEV"},
  {NULL}
};


int
main(int argc, char *argv[])
{
  GError *err = NULL;
  GOptionContext *opt_ctxt;
  GMainLoop *loop;
  PlcIoComm *io_comm;
  GIOStream *ser_io;
  
#ifdef MEMPROFILE
  g_mem_set_vtable (glib_mem_profiler_table);
#endif
  app_init(&app_ctxt);
/*g_type_init();*/
  opt_ctxt = g_option_context_new (" - Scratch helper for PLCS2 extension");
  g_option_context_add_main_entries(opt_ctxt, app_options, NULL);
  if (!g_option_context_parse(opt_ctxt, &argc, &argv, &err)) {
    g_printerr("Failed to parse options: %s\n", err->message);
    app_cleanup(&app_ctxt);
    return EXIT_FAILURE;
  }
  g_option_context_free(opt_ctxt);
  if (!app_ctxt.config_filename) {
    g_printerr("No configuration file\n");
    app_cleanup(&app_ctxt);
    return EXIT_FAILURE;
  }
  if (!read_config_file(&app_ctxt, &err)) {
    g_printerr("Failed to read config file: %s\n", err->message);
    g_clear_error(&err);
    app_cleanup(&app_ctxt);
    return EXIT_FAILURE;
  }


  if (app_ctxt.daemon) {
    if (!go_daemon(&app_ctxt, &err)) {
      g_printerr("Failed to start as daemon: %s\n", err->message);
      g_clear_error(&err);
      app_cleanup(&app_ctxt);
      return EXIT_FAILURE;
    }
  }

  ser_io = serial_device_open(app_ctxt.serial_device, 38400, 8, ParityNone, &err);
  if (!ser_io) {
    g_critical("Failed to open serial device: %s\n", err->message);
    app_cleanup(&app_ctxt);
    g_clear_error(&err);
    return EXIT_FAILURE;
  }
  
  io_comm = plc_io_comm_new(ser_io, &err);
  g_object_unref(ser_io);
  if (!io_comm) {
    g_critical("Failed to setup communication with PLC: %s\n", err->message);
    app_cleanup(&app_ctxt);
    g_clear_error(&err);
    return EXIT_FAILURE;
  }
  app_ctxt.websocket_server = (WebsocketServer*)plc_io_server_new(io_comm);
  g_object_unref(io_comm);
  
  
  configure_websocket_server(&app_ctxt);
  if (!websocket_server_start(app_ctxt.websocket_server, &err)) {
    g_critical("Failed to setup server: %s\n", err->message);
    app_cleanup(&app_ctxt);
    g_clear_error(&err);
    return EXIT_FAILURE;
  }
  
  loop = g_main_loop_new(NULL, FALSE);
  g_unix_signal_add(SIGINT, sigint_handler, loop);
  g_unix_signal_add(SIGTERM, sigint_handler, loop);
  g_debug("Starting");
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  g_message("Exiting");
  app_cleanup(&app_ctxt);
#ifdef MEMPROFILE
  g_mem_profile ();
#endif
  return EXIT_SUCCESS;
}
