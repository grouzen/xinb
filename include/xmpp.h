#ifndef __XMPP_H__
#define __XMPP_H__

gboolean xmpp_connect(Xinb*);
gboolean xmpp_send_message(Xinb*, LmMessageSubType);
LmHandlerResult xmpp_receive_command(LmMessageHandler*, LmConnection*, LmMessage*, gpointer);
LmHandlerResult xmpp_receive_iq(LmMessageHandler*, LmConnection*, LmMessage*, gpointer);
gboolean xmpp_send_presence(Xinb*, LmMessageSubType);
void xmpp_send_stream(Xinb*, FILE*);

#endif
