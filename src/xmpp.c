#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/xmpp.h"

gboolean xmpp_connect(struct xinb *xinb, struct xinb_account *account)
{
    xinb->conn = lm_connection_new_with_context(account->server, xinb->context);

    if(!lm_connection_open_and_block(xinb->conn, &(xinb->gerror))) {
        g_printerr("Couldn't open new connection to '%s': %s.\n",
                   account->server, xinb->gerror->message);
        return FALSE;
    }

    if(!lm_connection_authenticate_and_block(xinb->conn, account->username,
                                             account->password, account->resource,
                                             &(xinb->gerror))) {
        g_printerr("Couldn't authenticate with '%s', '%s': %s.\n",
                   account->username, account->password, xinb->gerror->message);
        return FALSE;
    }

    g_print("'%s@%s/%s' has been connected.\n",
            account->username, account->server, account->resource);

    xinb->state = LM_CONNECTION_STATE_AUTHENTICATED;
    
    return TRUE;
}

gboolean xmpp_send_message(struct xinb *xinb)
{
    LmMessage *m;

    if(xinb->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        g_printerr("Unable to send message: connection is not ready.\n");
        return FALSE;
    }
    
    m = lm_message_new(xinb->to, LM_MESSAGE_TYPE_MESSAGE);
    lm_message_node_add_child(m->node, "body", xinb->message);
    if(!lm_connection_send(xinb->conn, m, &(xinb->gerror))) {
        g_printerr("Unable to send message to '%s': %s.\n",
                   xinb->to, xinb->gerror->message);

        return FALSE;
    }

    lm_message_unref(m);

    return TRUE;
}

LmHandlerResult xmpp_receive_message(LmMessageHandler *handler,
                  LmConnection *conn, LmMessage *mes, gpointer udata)
{
    struct xinb *x = (struct xinb *) udata;
    if(x->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        g_printerr("Unable to receive message: connection is not ready.\n");
        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
    }

    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}
