#ifndef __XMPP_H__
#define __XMPP_H__

gboolean xmpp_connect(struct xinb*, struct xinb_account*);
gboolean xmpp_send_message(struct xinb*);
LmHandlerResult xmpp_receive_message(LmMessageHandler*, LmConnection*,
                                     LmMessage*, gpointer);

#endif
