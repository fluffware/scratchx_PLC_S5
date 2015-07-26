#include <websocket_server.h>
#include <libwebsockets.h>

typedef struct
{
  GSource source;
  WebsocketServer *server;
  struct WebsocketServerPollList *poll_fds;
} WebsocketSource;

struct _WebsocketServer
{
  GObject parent_instance;
  gchar *user;
  gchar *password;
  guint port;
  gchar *http_root;
  gchar *root_file; /* File served when accessing the root */
  struct lws_context_creation_info *ws_info;
  struct libwebsocket_context *ws_context;

  WebsocketSource *source;
};

struct _WebsocketServerClass
{
  GObjectClass parent_class;
  

  /* class members */
  /* Get a null terminated array of protocols for this class.
     The return value is the number of protocols. */
  guint (*get_protocols)(WebsocketServer *server,
			 guint allocate_extra,
			 struct libwebsocket_protocols **protocols);
  /* Signals */
};

/* Get protocols from the parent and add additional protocols.
   Intended for use in the get_protocols method.    
*/
guint
websocket_server_add_protocols(WebsocketServer *server,
			       guint allocate_extra,
			       struct libwebsocket_protocols **protocols,
			       const struct libwebsocket_protocols *added_protocols,
			       guint n_added_protocols);
