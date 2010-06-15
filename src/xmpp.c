#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/commands.h"
#include "../include/xmpp.h"
#include "../include/logs.h"

gboolean xmpp_connect(Xinb *x)
{
    gchar *server = g_hash_table_lookup(x->config, "server");
    gchar *username = g_hash_table_lookup(x->config, "username");
    gchar *password = g_hash_table_lookup(x->config, "password");
    gchar *resource = g_hash_table_lookup(x->config, "resource");
    
    x->conn = lm_connection_new_with_context(server, x->context);

    if(!lm_connection_open_and_block(x->conn, &(x->gerror))) {
        log_record(x, LOGS_ERR, "Couldn't open new connection to '%s': %s",
                   server, x->gerror->message);
        g_free(x->gerror->message);
        return FALSE;
    }

    if(!lm_connection_authenticate_and_block(x->conn,  username, password,
                                             resource, &(x->gerror))) {
        log_record(x, LOGS_ERR, "Couldn't authenticate with '%s', '%s': %s",
                   username, password, x->gerror->message);
        g_free(x->gerror->message);
        return FALSE;
    }

    log_record(x, LOGS_INFO, "'%s@%s/%s' has been connected",
               username, server, resource);

    x->state = LM_CONNECTION_STATE_AUTHENTICATED;
    
    return TRUE;
}

gboolean xmpp_send_presence(Xinb *x, LmMessageSubType subtype)
{
    LmMessage *m;

    if(x->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        log_record(x, LOGS_ERR,
                   "Unable to send presense: not authenticated");
        return FALSE;
    }

    m = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE, subtype);
    if(!lm_connection_send(x->conn, m, &(x->gerror))) {
        log_record(x, LOGS_ERR, "Unable to send presence of type '%d': %s",
                   subtype, x->gerror->message);
        g_free(x->gerror->message);
        return FALSE;
    }

    return TRUE;
}

gboolean xmpp_send_message(Xinb *x, LmMessageSubType subtype)
{
    LmMessage *m;

    if(x->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        log_record(x, LOGS_ERR,
                   "Unable to send message: not authenticated");
        return FALSE;
    }

    m = lm_message_new_with_sub_type(x->to, LM_MESSAGE_TYPE_MESSAGE, subtype);
    lm_message_node_add_child(m->node, "body", x->message);
    if(!lm_connection_send(x->conn, m, &(x->gerror))) {
        log_record(x, LOGS_ERR, "Unable to send message to '%s': %s",
                   x->to, x->gerror->message);
        g_free(x->gerror->message);
        lm_message_unref(m);
        return FALSE;
    }
    
    lm_message_unref(m);
    return TRUE;
}

LmHandlerResult xmpp_receive_message(LmMessageHandler *handler,
                                     LmConnection *conn, LmMessage *m, gpointer udata)
{
    Xinb *x = udata;
    LmMessageNode *body;
    
    if(x->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        log_record(x, LOGS_ERR,
                   "Unable to receive message: not authenticated");
        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
    }

    body = lm_message_node_find_child(m->node, "body");
    
    if(lm_message_get_type(m) == LM_MESSAGE_TYPE_MESSAGE &&
       lm_message_get_sub_type(m) == LM_MESSAGE_SUB_TYPE_CHAT) {
        lm_message_node_set_raw_mode(body, TRUE);
        log_record(x, LOGS_INFO,
                   "The command has been received: '%s'", body->value);
        
        if(!command_run(x, body->value))
            log_record(x, LOGS_ERR, "Command failure: '%s'", body->value);
    } else {
        log_record(x, LOGS_INFO, "The message has been received");
    }

    lm_message_unref(m);
    
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

void xmpp_send_stream(Xinb *x, FILE *stream)
{
    gchar *in_buf = NULL;
    size_t in_buf_len = 0;

    while(!feof(stream)) {
        gchar buf[256];
        size_t buf_len;
        gint i;
        
        buf_len = fread(buf, sizeof(gchar), 255, stream);
        if(!buf_len) {
            if(ferror(stream)) {
                log_record(x, LOGS_ERR,
                           "Error has been occured during reading stream");
            }
            break;
        }
        buf[buf_len] = '\0';
        
        in_buf = g_realloc(in_buf, in_buf_len + buf_len);
        for(i = 0; buf[i] != '\0'; i++) {
            in_buf[in_buf_len++] = buf[i];
        }
    }

    in_buf = g_realloc(in_buf, in_buf_len + 1);
    in_buf[in_buf_len] = '\0';

    x->to = g_strdup(g_hash_table_lookup(x->config, "owner"));
    x->message = g_strdup_printf("Incoming message:\n%s", in_buf);
    xmpp_send_message(x, LM_MESSAGE_SUB_TYPE_CHAT);

    g_free(x->to);
    g_free(x->message);
    if(in_buf)
        g_free(in_buf);
    
    return;
}
