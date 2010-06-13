#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/xmpp.h"

#define XINB_CONFIG_PATH ".xinb.ini"
#define XINB_CONFIG_GROUP "xinb"

struct xinb *xinb = NULL;

void xinb_release(struct xinb *xinb)
{
    if(xinb != NULL) {
        if(xinb->conn &&
           !lm_connection_close(xinb->conn, &(xinb->gerror))) {
            g_printerr("Couldn't close connection: %s.\n",
                       xinb->gerror->message);
        }
        
        if(xinb->conn)
            lm_connection_unref(xinb->conn);
        if(xinb->context)
            g_main_context_unref(xinb->context);
        if(xinb->loop)
            g_main_loop_unref(xinb->loop);
        if(xinb->gerror)
            g_error_free(xinb->gerror);
        if(xinb->message)
            g_free(xinb->message);
        if(xinb->to)
            g_free(xinb->to);
        if(xinb->account)
            g_hash_table_destroy(xinb->account);
        
        g_free(xinb);        
    }

    return;
}

static gboolean xinb_read_config(struct xinb *x)
{
    GKeyFile *key_file;
    gchar *config_file, **keys, *val;
    gboolean ret;
    gsize i = 0;
    
    key_file = g_key_file_new();
    config_file = g_strdup_printf("%s/%s", getenv("HOME"), XINB_CONFIG_PATH);
    if(!g_key_file_load_from_file(key_file, config_file,
                                  G_KEY_FILE_NONE, &(x->gerror))) {
        g_printerr("Couldn't load config file '%s': %s.\n",
                   config_file, x->gerror->message);
        ret = FALSE;
        goto out;
    }

    if(!g_key_file_has_group(key_file, XINB_CONFIG_GROUP)) {
        g_printerr("Group '%s' not exists in config file '%s'.\n",
                   XINB_CONFIG_GROUP, config_file);
        ret = FALSE;
        goto out;
    }

    keys = g_key_file_get_keys(key_file, XINB_CONFIG_GROUP, NULL, &(x->gerror));
    
    while(keys[i]) {
        val = g_key_file_get_value(key_file, XINB_CONFIG_GROUP,
                                   keys[i], &(x->gerror));
        if(val == NULL) {
            g_printerr("Couldn't get value from key '%s': %s.\n",
                       keys[i], x->gerror->message);
        }

        if(!g_hash_table_lookup(x->account, keys[i])) {
            g_hash_table_insert(x->account, g_strdup(keys[i]), g_strdup(val));
        }
        
        i++;
    }
    
    g_strfreev(keys);
    ret = TRUE;
    
    out:
    g_key_file_free(key_file);
    g_free(config_file);
            
    return ret;
}

void signals_handler(int signum)
{
    g_main_loop_quit(xinb->loop);

    return;
}

int main(int argc, char *argv[])
{
    int opt;
    static struct option long_opts[] = {
        {"server", 1, NULL, 's'},
        {"username", 1, NULL, 'u'},
        {"password", 1, NULL, 'p'},
        {"resource", 1, NULL, 'r'},
        {"owner", 1, NULL, 'o'},
        {0, 0, 0, 0}
    };

    signal(SIGINT, signals_handler);
    
    xinb = g_malloc0(sizeof(struct xinb));
    xinb->account = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    /* What I suppose to do with this shit? */
    while((opt = getopt_long_only(argc, argv, "", long_opts, NULL)) != -1) {
        switch(opt) {
        case 's':
            g_hash_table_insert(xinb->account, "server", g_strdup(optarg));
            break;
        case 'u':
            g_hash_table_insert(xinb->account, "username", g_strdup(optarg));
            break;
        case 'p':
            g_hash_table_insert(xinb->account, "password", g_strdup(optarg));
            break;
        case 'r':
            g_hash_table_insert(xinb->account, "resource", g_strdup(optarg));
            break;
        case 'o':
            g_hash_table_insert(xinb->account, "owner", g_strdup(optarg));
            break;
        default:
            xinb_release(xinb);
            exit(EXIT_FAILURE);
        }
    }

    if(!xinb_read_config(xinb)) {
        g_printerr("Config was not loaded.\n");
    }
    
    if(!g_hash_table_lookup(xinb->account, "server") ||
       !g_hash_table_lookup(xinb->account, "username") ||
       !g_hash_table_lookup(xinb->account, "password") ||
       !g_hash_table_lookup(xinb->account, "resource") ||
       !g_hash_table_lookup(xinb->account, "owner")) {
        g_printerr("Not enough information about account to start working.\n");
        xinb_release(xinb);
    }
    g_print("Config has been loaded.\n");
    
    xinb->context = g_main_context_default();
    
    if(!xmpp_connect(xinb) ||
       !xmpp_send_presence(xinb, LM_MESSAGE_SUB_TYPE_AVAILABLE)) {
        xinb_release(xinb);
        exit(EXIT_FAILURE);
    }

    lm_connection_register_message_handler(xinb->conn,
                    lm_message_handler_new(xmpp_receive_message, xinb, NULL),
                    LM_MESSAGE_TYPE_MESSAGE,
                    LM_HANDLER_PRIORITY_NORMAL);

    xinb->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(xinb->loop);

    xinb_release(xinb);

    g_print("Screw you guys I am going home.\n");
    
    return 0;
}
