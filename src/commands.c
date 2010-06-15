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
#define COMMAND_MAX_ARG_LENGTH 64
#define COMMAND_ARG_ARG1 1
#define COMMAND_ARG_REST 0

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

#if 0
static gchar **command_get_arg(gchar *c, gint which)
{
    GArray *args;
    gchar **ret;
    gchar arg[COMMAND_MAX_ARG_LENGTH];
    gint i = 0, j = 0, argn = which;

    /* Skipping spaces on left. */
    while(c[i++] == ' ');
    c = &c[i - 1];

    args = g_array_new(FALSE, FALSE, sizeof(gchar *));

    i = 0;
    while(1) {
        if(c[i] == ' ' || c[i] == '\n' || c[i] == '\0') {
            if(j) {
                gchar *args_arg;
                
                arg[j++] = '\0';
                args_arg = g_strdup(arg);
                g_array_append_val(args, args_arg);
                
                if(which == COMMAND_ARG_ARG1)
                    break;
                j = 0;
            }

            if(c[i] == '\0')
                break;
            argn = 1;
        } else {
            if(argn > 0) {
                if(j > COMMAND_MAX_ARG_LENGTH)
                    j = 0;
                arg[j++] = c[i];
            }
        }
        i++;
    }

    if(args->len) {
        gchar **ret = g_malloc0(sizeof(gchar*) * (args->len + 1));

        for(i = 0; i < args->len; i++) {
            ret[i] = g_strdup(g_array_index(args, gchar*, i));
        }

        /* The argv[] must be null terminated. */
        if(which == COMMAND_ARG_REST)
            ret[i] = NULL;
    } else {
        ret = NULL;
    }

    g_array_free(args, TRUE);

    return ret;
}
#endif

static gchar *command_usage(gint type)
{
    gchar *usage;
    
    switch(type) {
    case COMMAND_TYPE_EXEC:
        usage = ":e command [arg1 [arg2 [... argn]]]";
        break;
    case COMMAND_TYPE_MESSAGE:
        usage = ":m jid message";
        break;
    default:
        usage = "";
        break;
    }

    return usage;
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
