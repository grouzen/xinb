#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/commands.h"
#include "../include/xmpp.h"
#include "../include/logs.h"

static gchar *xmpp_get_jid(const gchar *jid)
{
    gchar **parts;
    gchar *nick_server;

    parts = g_strsplit(jid, "/", 0);
    nick_server = g_strdup(parts[0]);
    g_strfreev(parts);

    return nick_server;
}

gboolean xmpp_connect(Xinb *x)
{
    gchar *server = g_hash_table_lookup(x->config, "server");
    gchar *username = g_hash_table_lookup(x->config, "username");
    gchar *password = g_hash_table_lookup(x->config, "password");
    gchar *resource = g_hash_table_lookup(x->config, "resource");
    
    x->conn = lm_connection_new_with_context(server, x->context);
    x->state = LM_CONNECTION_STATE_OPENING;
    
    if(!lm_connection_open_and_block(x->conn, &(x->gerror))) {
        log_record(x, LOGS_ERR, "Couldn't open new connection to '%s': %s",
                   server, x->gerror->message);
        g_clear_error(&(x->gerror));
        return FALSE;
    }
    x->state = LM_CONNECTION_STATE_OPEN;

    if(!lm_connection_authenticate_and_block(x->conn,  username, password,
                                             resource, &(x->gerror))) {
        log_record(x, LOGS_ERR, "Couldn't authenticate with '%s', '%s': %s",
                   username, password, x->gerror->message);
        g_clear_error(&(x->gerror));
        return FALSE;
    }
    x->state = LM_CONNECTION_STATE_AUTHENTICATED;
    
    log_record(x, LOGS_INFO, "'%s@%s/%s' has been connected",
               username, server, resource);
    
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
        g_clear_error(&(x->gerror));
        return FALSE;
    }

    return TRUE;
}

/* TODO: large messages are not sent. (?)
         maybe I'll be splitting messages.
 */
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
        g_clear_error(&(x->gerror));
        lm_message_unref(m);
        return FALSE;
    }
    
    lm_message_unref(m);
    return TRUE;
}

/* TODO: read more about 'iq'. */
LmHandlerResult xmpp_receive_iq(LmMessageHandler *handler,
                        LmConnection *conn, LmMessage *m, gpointer udata)
{
    Xinb *x = udata;
    //g_print("IQ get query received...\n");
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}
                                
LmHandlerResult xmpp_receive_command(LmMessageHandler *handler,
                        LmConnection *conn, LmMessage *m, gpointer udata)
{
    Xinb *x = udata;
    LmMessageNode *body;
    gchar *jid;
    const gchar *from;
    
    if(x->state != LM_CONNECTION_STATE_AUTHENTICATED) {
        log_record(x, LOGS_ERR,
                   "Unable to receive message: not authenticated");
        goto out;
    }
    
    from = lm_message_node_get_attribute(m->node, "from");
    jid = xmpp_get_jid(from);
    if(g_strcmp0(jid, g_hash_table_lookup(x->config, "owner"))) {
        log_record(x, LOGS_ERR, "To command may only owner: '%s'", from);
        x->to = jid;
        x->message = g_strdup("You don't have permission to command me");
        xmpp_send_message(x, LM_MESSAGE_SUB_TYPE_CHAT);
        g_free(x->message);
        goto out;
    }
    
    body = lm_message_node_find_child(m->node, "body");
    if(lm_message_get_sub_type(m) != LM_MESSAGE_SUB_TYPE_CHAT) {
        log_record(x, LOGS_ERR, "Invalid subtype of the command");
        goto out;
    }
    
    if(command_run(x, body->value)) {
        log_record(x, LOGS_INFO, "The command was successfully executed");
    }

    out:
    g_free(jid);
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
    
    if(!in_buf)
        return;
    
    x->to = g_strdup(g_hash_table_lookup(x->config, "owner"));
    x->message = g_strdup(in_buf);
    xmpp_send_message(x, LM_MESSAGE_SUB_TYPE_CHAT);

    g_free(x->to);
    g_free(x->message);
    g_free(in_buf);

    return;
}
