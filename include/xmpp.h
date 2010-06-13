#ifndef __XMPP_H__
#define __XMPP_H__

gboolean xmpp_connect(struct xinb*);
gboolean xmpp_send_message(struct xinb*, LmMessageSubType);
LmHandlerResult xmpp_receive_message(LmMessageHandler*, LmConnection*, LmMessage*, gpointer);
gboolean xmpp_send_presence(struct xinb*, LmMessageSubType);

#endif
