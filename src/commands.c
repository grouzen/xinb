#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/xmpp.h"
#include "../include/logs.h"
#include "../include/commands.h"

#define COMMAND_MAX_LENGTH 1024

static int command_get_type(gchar *c)
{
    gint type;
    
    /* Commands examples:
       :e netstat -tuna
       :m xxx@xmpp.com hello!
    */
    if(strlen(c) > 1 && c[0] == ':') {
        switch(c[1]) {
        case 'e':
            type = COMMAND_TYPE_EXEC;
            break;
        case 'm':
            type = COMMAND_TYPE_MESSAGE;
            break;
        case 's':
            type = COMMAND_TYPE_SHEDULE;
            break;
        default:
            g_printerr("Unknown command type: '%c'.\n", c[1]);
            type = COMMAND_TYPE_NONE;
        }
    } else
        type = COMMAND_TYPE_NONE;
    
    return type;
}

static gboolean command_exec(Xinb *x, gchar *command)
{
    FILE *s;

    s = popen(command, "r");
    if(!s) {
        log_record(x, LOGS_ERR, "Couldn't exec command: %s", strerror(errno));
        return FALSE;
    }

    xmpp_send_stream(x, s);

    pclose(s);
    return TRUE;
}

gboolean command_run(Xinb *x, gchar *c)
{
    gint type;

    if(strlen(c) > COMMAND_MAX_LENGTH) {
        log_record(x, LOGS_ERR,
                   "The possible length of the command is exceeded: '%d'",
                   strlen(c));
        x->message = g_strdup("The possible length of the command is exceeded");
        goto send_error;
    }
    
    type = command_get_type(c);
    if(type == COMMAND_TYPE_NONE) {
        log_record(x, LOGS_ERR, "Unknown command type");
        x->message = g_strdup("Unknown command type");
        goto send_error;
    }

    /* This is an ugly hack for removing command type:
       :e command args
         ^
    */
    c = &c[3];

    if(type == COMMAND_TYPE_EXEC) {
        if(!command_exec(x, c)) {
            x->message = g_strdup("The errors occured during exec command");
            goto send_error;
        }
    }

    return TRUE;
    
    send_error:
    x->to = g_strdup(g_hash_table_lookup(x->config, "owner"));
    xmpp_send_message(x, LM_MESSAGE_SUB_TYPE_CHAT);
    g_free(x->to);
    g_free(x->message);
    return FALSE;
}
