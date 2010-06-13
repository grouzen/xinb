#include <stdio.h>
#include <string.h>
#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/xmpp.h"
#include "../include/commands.h"

#define COMMAND_MAX_LENGTH 256
#define COMMAND_MAX_ARG_LENGTH 64
#define COMMAND_ARG_ARG1 0
#define COMMAND_ARG_REST -1

static void command_free(struct command *c, gint type)
{
    if(c) {
        switch(type) {
        case COMMAND_TYPE_EXEC:
            if(c->value.exec) {
                if(c->value.exec->file)
                    g_free(c->value.exec->file);
                if(c->value.exec->args)
                    g_strfreev(c->value.exec->args);
                g_free(c->value.exec);
            }
            break;
        case COMMAND_TYPE_MESSAGE:
            if(c->value.message) {
                if(c->value.message->to)
                    g_free(c->value.message->to);
                if(c->value.message->body)
                    g_free(c->value.message->body);
                g_free(c->value.message);
            }
            break;
        case COMMAND_TYPE_SHEDULE:
            break;
        default:
            break;
        }
        g_free(c);
    }
    
    return;
}

static struct command *command_new(gint type)
{
    struct command *com;

    com = g_malloc0(sizeof(struct command));
    com->type = type;
    
    switch(type) {
    case COMMAND_TYPE_EXEC:
        com->value.exec = g_malloc0(sizeof(struct command_exec));
        break;
    case COMMAND_TYPE_MESSAGE:
        com->value.message = g_malloc0(sizeof(struct command_message));
        break;
    case COMMAND_TYPE_SHEDULE:
        /* TODO: */
        break;
    }

    return com;
}

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
            type = -1;
        }

        return type;
    }

    return -1;
}

static gchar **command_get_arg(gchar *c, gint which)
{
    GArray *args;
    gchar **ret;
    gchar arg[COMMAND_MAX_ARG_LENGTH];
    gint i, j = 0, argn = which;

    args = g_array_new(FALSE, FALSE, sizeof(gchar *));
    
    for(i = 0; c[i] != '\0'; i++) {
        if(c[i] == ' ' || c[i] == '\n') {
            if(j) {
                gchar *args_arg;
                
                arg[j++] = '\0';
                args_arg = g_strdup(arg);
                g_array_append_val(args, args_arg);
                
                if(which == COMMAND_ARG_REST) {
                    j = 0;
                    continue;
                }
                
                break;
            }
            argn = (!argn) ? 1 : 0;
        } else {
            if(argn > 0) {
                if(j > COMMAND_MAX_ARG_LENGTH)
                    j = 0;
                arg[j++] = c[i];
            }
        }
    }

    if(args->len) {
        gchar **normal_args = g_malloc0(sizeof(gchar) * args->len);

        for(i = 0; i < args->len; i++) {
            normal_args[i] = g_strdup(g_array_index(args, gchar*, i));
        }
        
        ret = normal_args;
    } else {
        ret = NULL;
    }

    g_array_free(args, TRUE);

    return ret;
}

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

gboolean command_run(struct xinb *x, gchar *c)
{
    struct command *com;
    gint type;
    gchar **arg1, **rest;
    
    x->to = g_strdup(g_hash_table_lookup(x->account, "owner"));

    if(strlen(c) > COMMAND_MAX_LENGTH) {
        g_printerr("The possible length of the command is exceeded: '%d'.\n",
                   strlen(c));
        x->message = g_strdup("The possible length of the command is exceeded.");
        goto send_error;
    }
    
    type = command_get_type(c);
    if(type < 0) {
        x->message = g_strdup("Unknown command type.");
        goto send_error;
    }

    arg1 = command_get_arg(c, COMMAND_ARG_ARG1);
    if(!arg1) {
        g_printerr("The command syntax error: first argument doesn't exist.\n");
        x->message = g_strdup_printf("Syntax error, please use - %s.\n",
                                     command_usage(type));
        goto send_error;
    }

    rest = command_get_arg(c, COMMAND_ARG_REST);
    if(!rest && type != COMMAND_TYPE_EXEC) {
        g_printerr("The command syntax error: rest arguments doesn't exist.\n");
        x->message = g_strdup_printf("Syntax error, please use - %s.\n",
                                     command_usage(type));
        goto send_error;
    }

    g_strfreev(arg1);
    g_strfreev(rest);
    return TRUE;
    
    send_error:
    xmpp_send_message(x, LM_MESSAGE_SUB_TYPE_CHAT);
    return FALSE;
}
