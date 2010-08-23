#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/xmpp.h"
#include "../include/logs.h"
#include "../include/error.h"
#include "../include/commands.h"

#define COMMAND_MAX_LENGTH 1024

static int command_get_type(Xinb *x, gchar *c)
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
            type = COMMAND_TYPE_SERVICE;
            break;
        default:
            log_record(x, LOGS_ERR, "Unknown command type: '%c'", c[1]);
            g_set_error(&(x->gerror), XINB_ERROR, XINB_COMMAND_GET_TYPE_ERROR,
                        "Unknown command type: '%c'", c[1]);
            type = COMMAND_TYPE_NONE;
        }
    } else {
        /* TODO: implement :h command. */
        log_record(x, LOGS_ERR, "The command syntax error");
        g_set_error(&(x->gerror), XINB_ERROR, XINB_COMMAND_GET_TYPE_ERROR,
                    "The command syntax error, use ':h' for getting help");
        type = COMMAND_TYPE_NONE;
    }
    
    return type;
}

/* TODO: get stderr from popen. */
static gboolean command_exec(Xinb *x, gchar *command)
{
    FILE *s;

    s = popen(command, "r");
    if(!s) {
        g_set_error(&(x->gerror), 
                    XINB_ERROR,
                    XINB_COMMAND_EXEC_ERROR,
                    "Couldn't exec command: %s", strerror(errno));
                                        
        return FALSE;
    }

    xmpp_send_stream(x, s);
    
    pclose(s);
    return TRUE;
}

static gchar *command_service_uptime(Xinb *x)
{
    gchar *ret;
    gint uptime = time(NULL) - x->start_time;
    gint hours = 0, minutes = 0, seconds = 0;

    if(uptime - 3600 >= 0) {
        do {
            uptime -= 3600;
            hours++;
        } while(uptime - 3600 >= 0);
    }
    if(uptime - 60 >= 0) {
        do {
            uptime -= 60;
            minutes++;
        } while(uptime - 60 >= 0);
    }

    seconds = uptime;
    
    ret = g_strdup_printf("%dh %dm %ds", hours, minutes, seconds);
    
    return ret;
}

static gchar *command_service_help(void)
{
    gchar *ret;
    ret = g_strdup("Avaliable commands:\n    uptime\n");
    
    return ret;
}

static gboolean command_service(Xinb *x, gchar *command)
{
    if(!g_strcmp0("uptime", command)) {
        x->message = command_service_uptime(x);
    } else {
        x->message = command_service_help();
    }

    x->to = g_strdup(g_hash_table_lookup(x->config, "owner"));
    xmpp_send_message(x, LM_MESSAGE_SUB_TYPE_CHAT);
    g_free(x->to);
    g_free(x->message);
    
    return TRUE;
}

gboolean command_run(Xinb *x, gchar *c)
{
    gint type;

    log_record(x, LOGS_INFO, "Trying to run command: '%s'", c);
    
    if(strlen(c) > COMMAND_MAX_LENGTH) {
        log_record(x, LOGS_ERR,
                   "The possible length of the command is exceeded: '%d'",
                   strlen(c));
        x->message = g_strdup("The possible length of the command is exceeded");
        goto send_error;
    }
    
    type = command_get_type(x, c);
    if(type == COMMAND_TYPE_NONE) {
        x->message = g_strdup(x->gerror->message);
        g_clear_error(&(x->gerror));
        goto send_error;
    }

    /* This is an ugly hack for removing command type:
       :e command args
         ^
    */
    c = &c[3];

    if(type == COMMAND_TYPE_EXEC) {
        if(!command_exec(x, c)) {
            log_record(x, LOGS_ERR, x->gerror->message);
            x->message = g_strdup(x->gerror->message);
            g_clear_error(&(x->gerror));
            goto send_error;
        }
    } else if(type == COMMAND_TYPE_SERVICE) {
        command_service(x, c);
    }

    return TRUE;
    
    send_error:
    x->to = g_strdup(g_hash_table_lookup(x->config, "owner"));
    xmpp_send_message(x, LM_MESSAGE_SUB_TYPE_CHAT);
    g_free(x->to);
    g_free(x->message);
    return FALSE;
}
