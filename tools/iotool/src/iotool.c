#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <serial.h>
#include <PLC_comm.h>


#define IOTOOL_ERROR (iotool_error_quark())

enum {
  IOTOOL_ERROR_FAILED = 1,
  IOTOOL_ERROR_CREATE_WIDGET_FAILED
};

GQuark
iotool_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("iotool-error-quark");
  return error_quark;
}




#define NPORTS 9
typedef struct _SerialContext SerialContext;

typedef struct
{
  GtkApplication *application;
  GMenuModel *app_menu;
  GMenuModel *win_menu;
  gchar *serial_device;
  PLCComm *comm;
  GCancellable *cancel;
  GtkWidget *bit_indicators[NPORTS][8];
  guint8 output_state[NPORTS];
} AppContext;

static void
app_init(AppContext *app)
{
  int i;
  app->application = NULL;
  app->app_menu = NULL;
  app->win_menu = NULL;
  app->comm = NULL;
  app->serial_device = g_strdup("/dev/ttyUSB0");
  app->cancel = g_cancellable_new();
  for(i = 0; i < NPORTS; i++) {
    app->output_state[i] = 0xff;
  }
}

static void
app_destroy(AppContext *app)
{
  g_cancellable_cancel(app->cancel);
  g_clear_object(&app->app_menu);
  g_clear_object(&app->win_menu);
  g_clear_object(&app->comm);
  g_clear_object(&app->cancel);
  g_free(app->serial_device);

}

static void
activate_quit (GSimpleAction *simple,
               GVariant      *parameter,
               gpointer user_data)
{
  AppContext *app_ctxt = user_data;
  g_application_quit(G_APPLICATION(app_ctxt->application));
}

  
#if 0
static void
print_widget_tree(GtkWidget *widget, guint indent)
{
  guint i;
  for (i = 0; i < indent; i++) putchar(' ');
  printf("Class: '%s' Name '%s'\n", G_OBJECT_TYPE_NAME(widget), gtk_widget_get_name(widget));
  if (GTK_IS_CONTAINER(widget)) {
    GList *child = gtk_container_get_children(GTK_CONTAINER(widget));
    while(child) {
      print_widget_tree(child->data, indent + 2);
      child = child->next;
    }
  }
}
#endif

static void
print_widget_tree(GtkWidget *widget, guint indent)
{
  GParamSpec **props;
  guint nprops;
  guint i;
  for (i = 0; i < indent; i++) putchar(' ');
  printf("Class: '%s' Name '%s'\n", G_OBJECT_TYPE_NAME(widget), gtk_widget_get_name(widget));
  props = gtk_widget_class_list_style_properties(GTK_WIDGET_GET_CLASS(widget),
						 &nprops);
  for (i = 0; i < nprops; i++) {
    printf("Prop: '%s'\n", props[i]->name);
  }
  g_free(props);
  if (GTK_IS_CONTAINER(widget)) {
    GList *child = gtk_container_get_children(GTK_CONTAINER(widget));
    while(child) {
      print_widget_tree(child->data, indent + 2);
      child = child->next;
    }
  }
}
static GQuark port_bit_quark = 0;

static void
bit_toggled(GtkToggleButton *togglebutton,
	    AppContext *app)
{
  gsize port_bit =
    GPOINTER_TO_SIZE(g_object_get_qdata(G_OBJECT(togglebutton),
					port_bit_quark));
  guint port = port_bit >> 3;
  guint bit = port_bit & 0x07;
  gchar buffer[7];
  if (gtk_toggle_button_get_active(togglebutton)) {
    app->output_state[port] |= 1<<bit;
  } else {
    app->output_state[port] &= ~(1<<bit);
  }
  if (app->comm) {
    g_snprintf(buffer, sizeof(buffer), "o%d %02x\n", port,
	       app->output_state[port]);
    PLC_comm_send(app->comm, buffer, NULL);
  }
}

static void
comm_reply(PLCComm *comm, PLCCmd *cmd, AppContext *app)
{
  g_debug("Cmd: '%s' Reply: '%s'", cmd->cmd, cmd->reply);
  if (cmd->cmd[0] == 'i' && cmd->reply) {
    gint port = cmd->cmd[1] - '0';
    if (port >= 0 && port < NPORTS) {
      int b;
      long bits = strtoul(cmd->reply, NULL, 16);
      g_debug("Bits: %02x", bits);
      for (b = 0; b < 8; b++) {
	if (app->bit_indicators[port][b]) {
	  gtk_label_set_text(GTK_LABEL(app->bit_indicators[port][b]),
			     (bits & (1<<b)) ? "1" : "0");
	}
      }
    }
  }
}

static void
populate_window(AppContext *app, GtkWindow *win)
{
  
  static const int pins[NPORTS] = {8,8,8,8,8,8,8,8,4};
  static const gboolean output[NPORTS] =
    {TRUE,TRUE,TRUE,TRUE,TRUE,TRUE,TRUE,FALSE,FALSE};
  int p;
  int b;
  GtkWidget *grid = gtk_grid_new();
  g_object_set(grid, "row-spacing", 10, "column-spacing", 10, NULL);
  for (p = 0; p < NPORTS; p++) {
    gchar buffer[3];
    GtkWidget *label;
    g_snprintf(buffer, sizeof(buffer), "P%d", p); 
    label = gtk_label_new(buffer);
    gtk_grid_attach(GTK_GRID(grid), label, 0, p+1, 1,1);
    gtk_widget_show(label);
  }
  for (b = 0; b < 8; b++) {
    gchar buffer[3];
    GtkWidget *label;
    g_snprintf(buffer, sizeof(buffer), ".%d", b); 
    label = gtk_label_new(buffer);
    gtk_grid_attach(GTK_GRID(grid), label, b+1, 0, 1,1);
    gtk_widget_show(label);
  }
  if (port_bit_quark == 0) {
    port_bit_quark = g_quark_from_static_string ("port_bit");
  }
  for (p = 0; p < NPORTS; p++) {
    for (b = 0; b < pins[p]; b++) {
      GtkWidget *toggle;
      GtkWidget *label;
      gchar buffer[5];
      toggle = gtk_check_button_new();
      label = gtk_label_new("1");
      gtk_container_add(GTK_CONTAINER(toggle), label);
      gtk_widget_show(label);
      gtk_widget_set_sensitive(toggle, output[p]);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), TRUE);
      g_snprintf(buffer, sizeof(buffer), "P%d.%d", p, b);
      gtk_widget_set_name(toggle,buffer);
      gtk_grid_attach(GTK_GRID(grid), toggle, b+1, p+1, 1,1);
      gtk_widget_show(toggle);
      g_object_set_qdata(G_OBJECT(toggle), port_bit_quark,
			 GSIZE_TO_POINTER((p<<3)|b));
      app->bit_indicators[p][b] = label;
      g_signal_connect(toggle, "toggled", G_CALLBACK(bit_toggled), app);
    }
    for (;b < 8; b++) {
      app->bit_indicators[p][b] = NULL;
    }
  }
  gtk_container_add(GTK_CONTAINER(win), grid);
  gtk_widget_show(grid);
}

static void
main_win_destroyed(GtkWidget *object,
		   AppContext *app_ctxt)
{
}

gboolean
create_main_window(AppContext *app, GError **err)
{

  GtkApplication *application = GTK_APPLICATION(app->application);
  GtkWidget *win = gtk_application_window_new(application);
  
  /* Don't hold a reference to the window, this object should be destroyed
     when the windows is destroyed */
  gtk_application_add_window(application, GTK_WINDOW(win));
  populate_window(app, GTK_WINDOW(win));
  gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(win), TRUE); 
 
  gtk_widget_show(win);
  g_signal_connect(win, "destroy", G_CALLBACK(main_win_destroyed), app);
  /* print_widget_tree(win, 2); */

  return TRUE;
}

static struct 
{
  gchar *serial_device;
  
} opt_values = {
  NULL,
};

    

static GOptionEntry options[] =
  {
    {"serial-device", 'd',0, G_OPTION_ARG_STRING, &opt_values.serial_device,
     "Serial device to communicate through", "DEV"},
    {NULL}
  };

static void
activate(GApplication *application, AppContext *app_ctxt)
{

  g_debug("Activate");
  /* Everything is done in startup */
}

GMenu *
build_app_menu()
{
  GMenu *menu = g_menu_new();
  g_menu_insert(menu, 0,"Quit", "app.quit");
  return menu;
}


static void
startup(GApplication *application, AppContext *app_ctxt)
{
  GError *err = NULL;
  const GActionEntry app_actions[] = {
    {"quit", activate_quit, NULL},
  };
  
  g_set_prgname("CPU95 I/O Tool");
  g_set_application_name("I/O Tool");
  app_init(app_ctxt);
  app_ctxt->application = GTK_APPLICATION(application);


  app_ctxt->app_menu = G_MENU_MODEL (build_app_menu());
  g_assert(app_ctxt->app_menu);
  g_action_map_add_action_entries (G_ACTION_MAP (application),
				   app_actions,G_N_ELEMENTS(app_actions),
				   app_ctxt);
  
  gtk_application_set_app_menu (GTK_APPLICATION(application),
				app_ctxt->app_menu);

#if 0
  app_ctxt->win_menu = G_MENU_MODEL (gtk_builder_get_object(builder,
							    "winmenu"));
  g_assert(app_ctxt->win_menu);
  g_object_ref(app_ctxt->win_menu);
  
  gtk_application_set_menubar(GTK_APPLICATION(application),
			      app_ctxt->win_menu);
  
  g_object_unref(builder);
#endif

  if (!create_main_window(app_ctxt, &err)) {
    g_printerr("Failed to connect to serial port: %s\n", err->message);
    g_error_free(err);
  }
  g_debug("Startup");
 
}

static void
shutdown(GApplication *application, AppContext *app_ctxt)
{
  g_debug("Shutdown");
  app_destroy(app_ctxt);
}


gint
handle_command_line (GApplication            *application,
		     GApplicationCommandLine *command_line,
		     AppContext *app_ctxt)
{
  GError *err = NULL;
  gchar **argv = NULL;
  int argc;
  argv = g_application_command_line_get_arguments (command_line,
						   &argc);
  g_free(app_ctxt->serial_device);
  app_ctxt->serial_device = g_strdup(opt_values.serial_device);
  g_debug("Serial device: %s", app_ctxt->serial_device);
  g_strfreev(argv);
  if (app_ctxt->serial_device) {
    GIOStream *serio;
    serio = serial_device_open(app_ctxt->serial_device,
			       38400, 8, ParityNone, &err);
    if (!serio) {
      g_printerr("Failed to connect to serial port: %s\n", err->message);
      g_error_free(err);
    } else {
      GMainContext *main_context = g_main_context_ref_thread_default ();
      app_ctxt->comm = PLC_comm_new(serio, &err);
      if (!app_ctxt->comm) {
	 g_printerr("Failed to start serial communication: %s\n", err->message);
	 g_error_free(err);
      }
      g_signal_connect(app_ctxt->comm, "reply-ready", 
		       G_CALLBACK(comm_reply), app_ctxt);
      g_main_context_unref(main_context);
      g_object_unref(serio);
    }
  }
  return EXIT_SUCCESS;
}


int
main(int argc, char **argv)
{
  GApplication *gapp;
  int status;
  AppContext app_ctxt;

  gapp = G_APPLICATION(gtk_application_new("se.fluffware.iLED_sim",
					   (G_APPLICATION_HANDLES_COMMAND_LINE)));
  g_application_add_main_option_entries(gapp, options);
  g_signal_connect (gapp, "startup", G_CALLBACK (startup), &app_ctxt);
  g_signal_connect (gapp, "shutdown", G_CALLBACK (shutdown), &app_ctxt);
  g_signal_connect (gapp, "command-line", G_CALLBACK (handle_command_line),
		    &app_ctxt);
  g_signal_connect (gapp, "activate", G_CALLBACK(activate), &app_ctxt);
  status = g_application_run (gapp, argc, argv);

  g_object_unref (gapp);

  return status;
}

