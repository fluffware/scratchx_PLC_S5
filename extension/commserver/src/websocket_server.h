#ifndef __WEBSOCKET_SERVER_H__T152E062N9__
#define __WEBSOCKET_SERVER_H__T152E062N9__

#include <glib-object.h>

#define WEBSOCKET_SERVER_ERROR (websocket_server_error_quark())
enum {
  WEBSOCKET_SERVER_ERROR_START_FAILED = 1,
  WEBSOCKET_SERVER_ERROR_INTERNAL
};

#define WEBSOCKET_SERVER_TYPE                  (websocket_server_get_type ())
#define WEBSOCKET_SERVER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBSOCKET_SERVER_TYPE, WebsocketServer))
#define IS_WEBSOCKET_SERVER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBSOCKET_SERVER_TYPE))
#define WEBSOCKET_SERVER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), WEBSOCKET_SERVER_TYPE, WebsocketServerClass))
#define IS_WEBSOCKET_SERVER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBSOCKET_SERVER_TYPE))
#define WEBSOCKET_SERVER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBSOCKET_SERVER_TYPE, WebsocketServerClass))

typedef struct _WebsocketServer WebsocketServer;
typedef struct _WebsocketServerClass WebsocketServerClass;

GType
websocket_server_get_type(void);

WebsocketServer *
websocket_server_new(void);
gboolean
websocket_server_start(WebsocketServer *server, GError **err);

#endif /* __WEBSOCKET_SERVER_H__T152E062N9__ */
