#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/xmpp.h"
#include "../include/logs.h"

#define XINB_CONFIG_PATH XINB_DIR "/.xinb.conf"
#define XINB_CONFIG_GROUP "xinb"

Xinb *xinb = NULL;
static struct option long_opts[] = {
    {"server", 1, NULL, 's'},
    {"username", 1, NULL, 'u'},
    {"password", 1, NULL, 'p'},
    {"resource", 1, NULL, 'r'},
    {"owner", 1, NULL, 'o'},
    {"verbose", 0, NULL, 'v'},
    {"daemon", 0, NULL, 'd'},
    {0, 0, 0, 0}
};

void xinb_release(Xinb *x)
{
    if(x != NULL) {
        if(x->conn &&
           !lm_connection_close(x->conn, &(x->gerror))) {
            g_printerr("Couldn't close connection: %s.\n",
                       x->gerror->message);
            g_free(x->gerror->message);
        }

        log_close(x);
        
        if(x->conn)
            lm_connection_unref(x->conn);
        if(x->loop)
            g_main_loop_unref(x->loop);
        if(x->context)
            g_main_context_unref(x->context);
        if(x->gerror)
            g_error_free(x->gerror);
        if(x->config)
            g_hash_table_destroy(x->config);
        
        g_free(x);
    }

    return;
}

static gboolean xinb_read_config(Xinb *x)
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
        g_free(x->gerror->message);
        goto out;
    }

    if(!g_key_file_has_group(key_file, XINB_CONFIG_GROUP)) {
        g_printerr("Group '%s' not exists in config file '%s'.\n",
                   XINB_CONFIG_GROUP, config_file);
        ret = FALSE;
        g_free(x->gerror->message);
        goto out;
    }

    keys = g_key_file_get_keys(key_file, XINB_CONFIG_GROUP, NULL, &(x->gerror));
    
    while(keys[i]) {
        val = g_key_file_get_value(key_file, XINB_CONFIG_GROUP,
                                   keys[i], &(x->gerror));
        if(val == NULL) {
            g_printerr("Couldn't get value from key '%s': %s.\n",
                       keys[i], x->gerror->message);
            g_free(x->gerror->message);
        }

        if(!g_hash_table_lookup(x->config, keys[i])) {
            g_hash_table_insert(x->config, g_strdup(keys[i]), g_strdup(val));
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
    gint opt;
    gint long_opts_len = 0;

    signal(SIGINT, signals_handler);
    
    xinb = g_malloc0(sizeof(Xinb));
    xinb->config = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    /* What I suppose to do with this shit? */
    while((opt = getopt_long_only(argc, argv, "", long_opts, NULL)) != -1) {
        switch(opt) {
        case 's':
            g_hash_table_insert(xinb->config, "server", g_strdup(optarg));
            break;
        case 'u':
            g_hash_table_insert(xinb->config, "username", g_strdup(optarg));
            break;
        case 'p':
            g_hash_table_insert(xinb->config, "password", g_strdup(optarg));
            break;
        case 'r':
            g_hash_table_insert(xinb->config, "resource", g_strdup(optarg));
            break;
        case 'o':
            g_hash_table_insert(xinb->config, "owner", g_strdup(optarg));
            break;
        case 'v':
            g_hash_table_insert(xinb->config, "verbose", g_strdup("on"));
            break;
        case 'd':
            g_hash_table_insert(xinb->config, "daemon", g_strdup("on"));
            break;
        default:
            xinb_release(xinb);
            exit(EXIT_FAILURE);
        }
        long_opts_len++;
    }
    
    if(!xinb_read_config(xinb)) {
        log_record(xinb, LOGS_ERR, "Config was not loaded");
    }

    if(!g_hash_table_lookup(xinb->config, "verbose"))
        g_hash_table_insert(xinb->config, "verbose", "off");
    if(!g_hash_table_lookup(xinb->config, "daemon"))
        g_hash_table_insert(xinb->config, "daemon", "off");
    
    xinb->logfd = log_open(xinb);
    if(!xinb->logfd) {
        xinb_release(xinb);
        exit(EXIT_FAILURE);
    }

    log_record(xinb, LOGS_INFO, "Starting...");

    if(!g_hash_table_lookup(xinb->config, "server") ||
       !g_hash_table_lookup(xinb->config, "username") ||
       !g_hash_table_lookup(xinb->config, "password") ||
       !g_hash_table_lookup(xinb->config, "resource") ||
       !g_hash_table_lookup(xinb->config, "owner")) {
        log_record(xinb, LOGS_ERR,
                   "Not enough information about account to start working");
        xinb_release(xinb);
        exit(EXIT_FAILURE);
    }
    log_record(xinb, LOGS_INFO, "Config has been loaded");
    
    xinb->context = g_main_context_default();
    
    if(!xmpp_connect(xinb) ||
       !xmpp_send_presence(xinb, LM_MESSAGE_SUB_TYPE_AVAILABLE)) {
        xinb_release(xinb);
        exit(EXIT_FAILURE);
    }

    /* For sending messages from stdin if it's present. */
    if(argc - 1 > long_opts_len * 2) {
        FILE *stream;
        gchar *input = argv[argc - 1];

        stream = g_strcmp0(input, "-") ?
            fopen(input, "r") : stdin;
        if(!stream) {
            log_record(xinb, LOGS_ERR, "Couldn't open input stream: %s",
                       strerror(errno));
        } else 
            xmpp_send_stream(xinb, stream);
        
        xinb_release(xinb);
        exit(EXIT_SUCCESS);
    }
    
    lm_connection_register_message_handler(xinb->conn,
                    lm_message_handler_new(xmpp_receive_message, xinb, NULL),
                    LM_MESSAGE_TYPE_MESSAGE,
                    LM_HANDLER_PRIORITY_NORMAL);

    xinb->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(xinb->loop);

    log_record(xinb, LOGS_INFO, "Screw you guys I am going home");
    
    xinb_release(xinb);
    return 0;
}
