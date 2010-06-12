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

struct xinb *xinb;
struct xinb_account *account;

void xinb_release(struct xinb *xinb, struct xinb_account *account)
{
    if(xinb != NULL) {
        if(!lm_connection_close(xinb->conn, &(xinb->gerror))) {
            g_printerr("Couldn't close connection: %s.\n",
                       xinb->gerror->message);
        }

        lm_connection_unref(xinb->conn);
        g_main_context_unref(xinb->context);
        g_main_loop_unref(xinb->loop);
        if(xinb->gerror)
            g_error_free(xinb->gerror);
        if(xinb->message)
            g_free(xinb->message);
        if(xinb->to)
            g_free(xinb->to);

        g_free(xinb);
        
    }

    if(account) g_free(account);
    
    return;
}

gboolean xinb_read_config(struct xinb *xinb, struct xinb_account *account)
{
    GKeyFile *key_file;
    GHashTable *table;
    gchar *config_file, **keys, *val;
    gboolean ret;
    gsize i;
    
    key_file = g_key_file_new();
    config_file = g_strdup_printf("%s/%s", getenv("HOME"), XINB_CONFIG_PATH);
    if(!g_key_file_load_from_file(key_file, config_file,
                                  G_KEY_FILE_NONE, &(xinb->gerror))) {
        g_printerr("Couldn't load config file '%s': %s.\n",
                   config_file, xinb->gerror->message);
        ret = FALSE;
        goto out;
    }

    if(!g_key_file_has_group(key_file, XINB_CONFIG_GROUP)) {
        g_printerr("Group '%s' not exists in config file '%s'.\n",
                   XINB_CONFIG_GROUP, config_file);
        ret = FALSE;
        goto out;
    }

    table = g_hash_table_new(NULL, NULL);
    keys = g_key_file_get_keys(key_file, XINB_CONFIG_GROUP, NULL, &(xinb->gerror));
    
    while(keys[i]) {
        val = g_key_file_get_value(key_file, XINB_CONFIG_GROUP,
                                   keys[i], &(xinb->gerror));
        if(val == NULL) {
            g_printerr("Couldn't get value from key '%s': %s.\n",
                       keys[i], xinb->gerror->message);
        }
        g_hash_table_insert(table, keys[i], val);
        
        i++;
    }    
    
    g_strfreev(keys);
    
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
    account = g_malloc0(sizeof(struct xinb_account));

    while((opt = getopt_long_only(argc, argv, "", long_opts, NULL)) != -1) {
        switch(opt) {
        case 's':
            account->server = optarg;
            break;
        case 'u':
            account->username = optarg;
            break;
        case 'p':
            account->password = optarg;
            break;
        case 'r':
            account->resource = optarg;
            break;
        case 'o':
            account->owner = optarg;
            break;
        default:
            g_free(xinb);
            g_free(account);
            exit(EXIT_FAILURE);
        }
    }
    
    xinb->context = g_main_context_default();
    
    if(!xmpp_connect(xinb, account)) {
        g_main_context_unref(xinb->context);
        lm_connection_unref(xinb->conn);
        g_free(xinb);
        g_free(account);
        exit(EXIT_FAILURE);
    }

    lm_connection_register_message_handler(xinb->conn,
                    lm_message_handler_new(xmpp_receive_message, xinb, NULL),
                    LM_MESSAGE_TYPE_MESSAGE,
                    LM_HANDLER_PRIORITY_NORMAL);

    xinb->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(xinb->loop);

    xinb_release(xinb, account);

    g_print("Screw you guys I am going home.\n");
    
    return 0;
}
