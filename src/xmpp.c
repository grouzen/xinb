#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/commands.h"
#include "../include/xmpp.h"

gboolean xmpp_connect(struct xinb *xinb)
{
    xinb->conn = lm_connection_new_with_context(g_hash_table_lookup(xinb->account, "server"),
                                                xinb->context);

    if(!lm_connection_open_and_block(xinb->conn, &(xinb->gerror))) {
        g_printerr("Couldn't open new connection to '%s': %s.\n",
                   g_hash_table_lookup(xinb->account, "server"), xinb->gerror->message);
        return FALSE;
    }

    if(!lm_connection_authenticate_and_block(xinb->conn,
                                             g_hash_table_lookup(xinb->account, "username"),
                                             g_hash_table_lookup(xinb->account, "password"),
                                             g_hash_table_lookup(xinb->account, "resource"),
                                             &(xinb->gerror))) {
        g_printerr("Couldn't authenticate with '%s', '%s': %s.\n",
                   g_hash_table_lookup(xinb->account, "username"),
                   g_hash_table_lookup(xinb->account, "password"), xinb->gerror->message);
        return FALSE;
    }

    g_print("'%s@%s/%s' has been connected.\n",
            g_hash_table_lookup(xinb->account, "username"),
            g_hash_table_lookup(xinb->account, "server"),
            g_hash_table_lookup(xinb->account, "resource"));

    xinb->state = LM_CONNECTION_STATE_AUTHENTICATED;
    
    return TRUE;
}

gboolean xmpp_send_presence(struct xinb *xinb, LmMessageSubType subtype)
{
    LmMessage *m;

    if(xinb->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        g_printerr("Unable to send presense: connection is not ready.\n");
        return FALSE;
    }

    m = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE, subtype);
    if(!lm_connection_send(xinb->conn, m, &(xinb->gerror))) {
        g_printerr("Unable to send presence of type '%d': %s.\n",
                   subtype, xinb->gerror->message);
        return FALSE;
    }
}

gboolean xmpp_send_message(struct xinb *xinb, LmMessageSubType subtype)
{
    LmMessage *m;

    if(xinb->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        g_printerr("Unable to send message: connection is not ready.\n");
        return FALSE;
    }
    
    m = lm_message_new_with_sub_type(xinb->to, LM_MESSAGE_TYPE_MESSAGE, subtype);
    lm_message_node_add_child(m->node, "body", xinb->message);
    if(!lm_connection_send(xinb->conn, m, &(xinb->gerror))) {
        g_printerr("Unable to send message to '%s': %s.\n",
                   xinb->to, xinb->gerror->message);
        lm_message_unref(m);
        return FALSE;
    }
    
    lm_message_unref(m);

    return TRUE;
}

LmHandlerResult xmpp_receive_message(LmMessageHandler *handler,
                  LmConnection *conn, LmMessage *m, gpointer udata)
{
    struct xinb *x = udata;
    LmMessageNode *node_root, *node_body;
    
    if(x->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        g_printerr("Unable to receive message: connection is not ready.\n");
        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
    }

    node_root = lm_message_get_node(m);
    node_body = lm_message_node_find_child(node_root, "body");
    
    if(lm_message_get_type(m) == LM_MESSAGE_TYPE_MESSAGE &&
       lm_message_get_sub_type(m) == LM_MESSAGE_SUB_TYPE_CHAT) {
        g_print("The command has been received: '%s'.\n", node_body->value);
        if(!command_run(x, node_body->value)) {
            g_printerr("Command failure: '%s'.\n", node_body->value);
        }
    } else {
        g_print("The message has been received.\n");
    }

    lm_message_unref(m);
    
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}
