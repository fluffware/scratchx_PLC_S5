#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib-unix.h>
#include "httpd.h"
#include "serial.h"
#include "AS511_comm.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define S5_TOOL_ERROR (S5_tool_error_quark())
enum {
  S5_TOOL_ERROR_FAILED = 1
};

GQuark
S5_tool_error_quark(void)
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("S5-tool-error-quark");
  return error_quark;
}


typedef struct AppContext AppContext;
struct AppContext
{
  GCancellable *cancel;
  gchar *serial_device;
  GIOStream *ser;
  AS511Connection *AS511_conn;
  GMainLoop *loop;
};
  
AppContext app_ctxt  = {
  NULL,
  NULL,
  NULL,
  NULL
};



static void
app_init(AppContext *app)
{
  app->cancel = g_cancellable_new();
  app->serial_device = g_strdup("/dev/ttyUSB0");
}


static void
app_cleanup(AppContext* app)
{
  g_cancellable_cancel(app->cancel);
  g_clear_object(&app->cancel);
  g_clear_object(&app->AS511_conn);
  g_clear_object(&app->ser);
  g_free(app->serial_device);
}

static gboolean
sigint_handler(gpointer user_data)
{
  AppContext *app = user_data;
  g_cancellable_cancel(app->cancel);
  return TRUE;
}

/* Command DIR */

static void
dir_done(GObject *source_object,
	 GAsyncResult *res,
	 gpointer user_data)
{
  AppContext *app = user_data;
  GError *err = NULL;
  guint16 *db_addr = 
    AS511_connection_dir_finish((AS511Connection*)source_object, res, &err);
  if (!db_addr) {
    g_printerr("Failed to read DB dir: %s\n", err->message);
    g_error_free(err);
  } else {
    g_free(db_addr);
    
    g_debug("DIR done");
  }
  g_main_loop_quit(app->loop);
}

static const struct
{
  gchar *name;
  guint8 id;
} block_id[] =
  {
    {"db", AS511_ID_DB},
    {"sb", AS511_ID_SB},
    {"pb", AS511_ID_PB},
    {"fb", AS511_ID_FB},
    {"ob", AS511_ID_OB},
    {"fx", AS511_ID_FX},
    {"dx", AS511_ID_DX}
  };
  
static gboolean 
cmd_dir(AppContext *app,int argc, char *argv[], GError **err)
{
  gint8 id = AS511_ID_DB;
  if (argc >= 1) {
    int c;
    for (c = 0; c < G_N_ELEMENTS(block_id); c++) {
      if (strcmp(argv[0], block_id[c].name) == 0) {
	id = block_id[c].id;
	break;
      }
    }
  }
  AS511_connection_dir_async(app_ctxt.AS511_conn,
			     id,
			     app_ctxt.cancel,
			     dir_done,
			     app);
  return TRUE;
}

/* Command SYS_PAR */
static void
sys_par_done(GObject *source_object,
	     GAsyncResult *res,
	     gpointer user_data)
{
  AppContext *app = user_data;
  GError *err = NULL;
  AS511SysPar *sys = 
    AS511_connection_sys_par_finish((AS511Connection*)source_object, res, &err);
  if (!sys) {
    g_printerr("Failed to read system parameters: %s\n", err->message);
    g_error_free(err);
  } else {
    printf("PAE: 0x%04x\n", sys->PAE);
    printf("PAA: 0x%04x\n", sys->PAE);
    printf("M: 0x%04x\n", sys->M);
    printf("T: 0x%04x\n", sys->T);
    printf("Z: 0x%04x\n", sys->Z);
    printf("IAData: 0x%04x\n", sys->IAData);
    printf("PLCType: 0x%02x\n", sys->PLCType);
    g_free(sys);
    g_debug("SYS_PAR done");
  }
  g_main_loop_quit(app->loop);
}
static gboolean 
cmd_sys_par(AppContext *app,int argc, char *argv[], GError **err)
{
  AS511_connection_sys_par_async(app_ctxt.AS511_conn, 
				 app_ctxt.cancel,
				 sys_par_done,
				 app);
  return TRUE;
}

/* Command DB_READ */
static void
db_read_done(GObject *source_object,
	     GAsyncResult *res,
	     gpointer user_data)
{
  AppContext *app = user_data;
  GError *err = NULL;
  AS511ReadResult *read_res = 
    AS511_connection_db_read_finish((AS511Connection*)source_object, res, &err);
  if (!read_res) {
    g_printerr("Failed to read memory: %s\n", err->message);
    g_error_free(err);
  } else {
    int i;
    gboolean nl = TRUE;
    for (i = 0; i < read_res->length; ) {
      if (nl) {
	printf("%04x", i + read_res->first);
	nl = FALSE;
      }
      printf(" %02x", read_res->data[i]);
      i++;
      if (((i+read_res->first) % 16) == 0) {
	putchar('\n');
	nl = TRUE;
      }
    }
    if (((i+read_res->first) % 16) != 0) {
      putchar('\n');
    }
    g_free(read_res);
    g_free(read_res->data);
    g_debug("DB_READ done");
  }
  g_main_loop_quit(app->loop);
}

static gboolean 
cmd_db_read(AppContext *app,int argc, char *argv[], GError **err)
{
  guint16 first;
  guint16 last;
  if (argc >= 1) {
    char *end;
    first = strtoul(argv[0], &end, 0);
    if (argc >= 2) {
      last = strtoul(argv[1], &end, 0);
    } else {
      last = first;
    }
    if (last < first) {
       g_set_error(err, S5_TOOL_ERROR, S5_TOOL_ERROR_FAILED,
		"First address must be smaller than last");
       return FALSE;
    }
    
  } else {
    g_set_error(err, S5_TOOL_ERROR, S5_TOOL_ERROR_FAILED,
		"Missing first address");
    return FALSE;
  }
  
  AS511_connection_db_read_async(app_ctxt.AS511_conn,
				 first,
				 last,
				 app_ctxt.cancel,
				 db_read_done,
				 app);
  return TRUE;
}

/* Command DB_WRITE */
static void
db_write_done(GObject *source_object,
	     GAsyncResult *res,
	     gpointer user_data)
{
  AppContext *app = user_data;
  GError *err = NULL;
  gboolean ret; 
  ret = AS511_connection_db_write_finish((AS511Connection*)source_object,
					res, &err);
  if (!ret) {
    g_printerr("Failed to write memory: %s\n", err->message);
    g_error_free(err);
  } else {
    g_debug("DB_WRITE done");
  }
  g_main_loop_quit(app->loop);
}

static gboolean 
cmd_db_write(AppContext *app,int argc, char *argv[], GError **err)
{
  guint16 first;
  GByteArray *bytes;
  if (argc >= 1) {
    char *end;
    first = strtoul(argv[0], &end, 0);
    argv++;
    argc--;
    bytes = g_byte_array_new();
    while(argc > 0) {
      guint8 b = strtoul(argv[0], &end, 0);
      g_byte_array_append(bytes,&b,1);
      argc--;
      argv++;
    }
    
  } else {
    g_set_error(err, S5_TOOL_ERROR, S5_TOOL_ERROR_FAILED,
		"Missing first address");
    return FALSE;
  }
  
  AS511_connection_db_write_async(app_ctxt.AS511_conn,
				 first,
				  bytes->len,
				  bytes->data,
				 app_ctxt.cancel,
				 db_write_done,
				 app);
  return TRUE;
}


static const struct {
  gchar *name;
  gboolean (*func)(AppContext *app,int argc, char *argv[], GError **err);
} commands[] = {
  {"dir", cmd_dir},
  {"sys_par", cmd_sys_par},
  {"db_read", cmd_db_read},
  {"db_write", cmd_db_write}
};

static gboolean
run_command(AppContext *app, int argc, char *argv[], GError **err)
{
  int c;
  if (argc < 2) {
    g_set_error(err, S5_TOOL_ERROR, S5_TOOL_ERROR_FAILED,
		"No command");
    return FALSE;
  }
  for (c = 0; c < G_N_ELEMENTS(commands); c++) {
    if (strcmp(argv[1], commands[c].name) == 0) {
      return commands[c].func(app, argc - 2, &argv[2], err);
    }
  }
  g_set_error(err, S5_TOOL_ERROR, S5_TOOL_ERROR_FAILED,
	      "Unknown command");
  return FALSE;   
}
    
const GOptionEntry app_options[] = {
  {"serial-device", 'd', 0, G_OPTION_ARG_STRING,
   &app_ctxt.serial_device, "Serial device", "DEVICE"},
  {NULL}
};


int
main(int argc, char *argv[])
{
  GError *err = NULL;
  GOptionContext *opt_ctxt;
  
#ifdef MEMPROFILE
  g_mem_set_vtable (glib_mem_profiler_table);
#endif
  app_init(&app_ctxt);

  opt_ctxt = g_option_context_new ("COMMAND - Siemens S5 communication tool");
  g_option_context_add_main_entries(opt_ctxt, app_options, NULL);
  if (!g_option_context_parse(opt_ctxt, &argc, &argv, &err)) {
    g_printerr("Failed to parse options: %s\n", err->message);
    app_cleanup(&app_ctxt);
    return EXIT_FAILURE;
  }
  g_option_context_free(opt_ctxt);

  app_ctxt.ser =
    serial_device_open(app_ctxt.serial_device, 9600, 8, ParityEven, &err);
  if (!app_ctxt.ser) {
    g_printerr("Failed to open serial port: %s\n", err->message);
    app_cleanup(&app_ctxt);
    return EXIT_FAILURE;
  }
  app_ctxt.AS511_conn = AS511_connection_new(app_ctxt.ser);

  app_ctxt.loop = g_main_loop_new(NULL, FALSE);
  if (!run_command(&app_ctxt, argc, argv, &err)) {
    g_printerr("Command failed: %s\n", err->message);
    app_cleanup(&app_ctxt);
    return EXIT_FAILURE;
  }
  g_unix_signal_add(SIGINT, sigint_handler, &app_ctxt);
  g_unix_signal_add(SIGTERM, sigint_handler, &app_ctxt);
  g_debug("Starting");
  g_main_loop_run(app_ctxt.loop);
  g_main_loop_unref(app_ctxt.loop);
  g_message("Exiting");
  app_cleanup(&app_ctxt);
#ifdef MEMPROFILE
  g_mem_profile ();
#endif
  return EXIT_SUCCESS;
}
