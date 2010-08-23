/*
                 XINB is a bot ;>
   It's configured only with config file, without command line
   options, because I hate all these switch statements,
   I think it's ugly. And yes, I hate glib's mechanism of
   the options parsing.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/xmpp.h"
#include "../include/logs.h"
#include "../include/error.h"

#define XINB_CONFIG_PATH XINB_DIR "/.xinb.conf"
#define XINB_CONFIG_GROUP "xinb"

Xinb *xinb = NULL;

void xinb_release(Xinb *x)
{
    if(x != NULL) {
        if(x->conn &&
           !lm_connection_close(x->conn, &(x->gerror))) {
            g_printerr("Couldn't close connection: %s\n",
                       x->gerror->message);
            g_clear_error(&(x->gerror));
        }
        x->state = LM_CONNECTION_STATE_CLOSED;

        log_close(x);
        
        if(x->conn)
            lm_connection_unref(x->conn);
        if(x->loop)
            g_main_loop_unref(x->loop);
        if(x->context)
            g_main_context_unref(x->context);
        if(x->config)
            g_hash_table_destroy(x->config);
        if(x->gerror)
            g_clear_error(&(x->gerror));
        
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
        g_printerr("Couldn't load config file '%s': %s\n",
                   config_file, x->gerror->message);
        g_clear_error(&(x->gerror));
        ret = FALSE;
        goto out;
    }

    /* TODO: error handling? */
    keys = g_key_file_get_keys(key_file, XINB_CONFIG_GROUP, NULL, &(x->gerror));
    if(keys == NULL &&
       x->gerror->code == G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
        g_printerr("Error while reading config: %s\n",
                   x->gerror->message);
        g_clear_error(&(x->gerror));
        ret = FALSE;
        goto out;
    }
    
    while(keys[i]) {
        val = g_key_file_get_value(key_file, XINB_CONFIG_GROUP,
                                   keys[i], &(x->gerror));
        if(val == NULL) {
            g_printerr("Couldn't get value from key '%s': %s\n",
                       keys[i], x->gerror->message);
            g_clear_error(&(x->gerror));
        }
        g_hash_table_insert(x->config, g_strdup(keys[i]), g_strdup(val));
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
    /* Say bye-bye, good bot. */
    xinb->to = g_strdup(g_hash_table_lookup(xinb->config, "owner"));
    if(signum == SIGINT)
        xinb->message = g_strdup("I've received SIGINT, who turn off me?");
    else if(signum == SIGTERM)
        xinb->message = g_strdup("I've received SIGTERM, see you later!");
    xmpp_send_message(xinb, LM_MESSAGE_TYPE_MESSAGE);
    g_free(xinb->to);
    g_free(xinb->message);
    
    g_main_loop_quit(xinb->loop);

    return;
}

gboolean xinb_daemonize(Xinb *x)
{
    pid_t pid, sid;

    log_record(x, LOGS_INFO, "Trying to daemonize...");
    
    pid = fork();
    if(pid < 0) {
        g_set_error(&(x->gerror), XINB_ERROR, XINB_XINB_DAEMONIZE_ERROR,
                    "Daemonize failed: %s", strerror(errno));
        return FALSE;
    }

    /* Function that called must check on
       x->gerror == NULL, if true - error,
       else parent process must just terminate.
    */
    if(pid > 0) {
        return FALSE;
    }

    umask(0);

    sid = setsid();
    if(sid < 0) {
        g_set_error(&(x->gerror), XINB_ERROR, XINB_XINB_DAEMONIZE_ERROR,
                    "Couldn't set session id: %s", strerror(errno));
        return FALSE;
    }

    if(chdir("/") < 0) {
        g_set_error(&(x->gerror), XINB_ERROR, XINB_XINB_DAEMONIZE_ERROR,
                    "Couldn't change directory: %s", strerror(errno));
        return FALSE;
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return TRUE;
}

int main(int argc, char *argv[])
{
    LmMessageHandler *h_message, *h_iq;
    struct stat stat_buf;
    gchar *xinb_dir_path;
    gint stat_ret;
    
    if(argc > 2) {
        g_print("Usage: %s [-|file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    signal(SIGINT, signals_handler);
    signal(SIGTERM, signals_handler);
    
    xinb = g_malloc0(sizeof(Xinb));
    xinb->config = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    /* Create new directory XINB_DIR in $HOME if it doesn't exist.
       Note: in case directory doesn't exist you should create
       config XINB_CONFIG_PATH.
    */
    xinb_dir_path = g_strdup_printf("%s/%s", getenv("HOME"), XINB_DIR);
    stat_ret = stat(xinb_dir_path, &stat_buf);
    if(stat_ret < 0 || !S_ISDIR(stat_buf.st_mode)) {
        if(mkdir(xinb_dir_path,
                 S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0) {
            g_printerr("Couldn't create directory: %s\n", strerror(errno));
            xinb_release(xinb);
            exit(EXIT_FAILURE);
        }
        g_print("New directory has been created: '%s'\n", xinb_dir_path);
    }
    g_free(xinb_dir_path);
    
    if(!xinb_read_config(xinb)) {
        g_printerr("Config was not loaded\n");
        xinb_release(xinb);
        exit(EXIT_FAILURE);
    }
    
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

    if(!g_strcmp0(g_hash_table_lookup(xinb->config, "daemon"), "on") &&
       !xinb_daemonize(xinb)) {
        if(xinb->gerror) {
            log_record(xinb, LOGS_ERR, xinb->gerror->message);
            log_record(xinb, LOGS_INFO, "Daemon terminating...");
            g_clear_error(&(xinb->gerror));
            xinb_release(xinb);
            exit(EXIT_FAILURE);
        }

        log_record(xinb, LOGS_INFO, "Parent process terminating...");
        xinb_release(xinb);
        exit(EXIT_SUCCESS);
    }
    
    xinb->context = g_main_context_default();
    
    if(!xmpp_connect(xinb) ||
       !xmpp_send_presence(xinb, LM_MESSAGE_SUB_TYPE_AVAILABLE)) {
        xinb_release(xinb);
        exit(EXIT_FAILURE);
    }

    /* For sending messages from stdin if it's present.
       It's forbidden for daemon.
     */
    if(argc > 1 && g_strcmp0(g_hash_table_lookup(xinb->config, "daemon"), "on")) {
        FILE *stream;
        gchar *input = argv[1];

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

    h_message = lm_message_handler_new(xmpp_receive_command, xinb, NULL);
    lm_connection_register_message_handler(xinb->conn, h_message,
                                           LM_MESSAGE_TYPE_MESSAGE,
                                           LM_HANDLER_PRIORITY_NORMAL);
    h_iq = lm_message_handler_new(xmpp_receive_iq, xinb, NULL);
    lm_connection_register_message_handler(xinb->conn, h_iq,
                                           LM_MESSAGE_TYPE_IQ,
                                           LM_HANDLER_PRIORITY_NORMAL);

    /* Just say hello ^_^. */
    xinb->to = g_strdup(g_hash_table_lookup(xinb->config, "owner"));
    xinb->message = g_strdup_printf("I'm here, what do you want %s?", xinb->to);
    xmpp_send_message(xinb, LM_MESSAGE_TYPE_MESSAGE);
    g_free(xinb->to);
    g_free(xinb->message);

    xinb->start_time = time(NULL);
    xinb->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(xinb->loop);

    lm_message_handler_unref(h_iq);
    lm_message_handler_unref(h_message);
    log_record(xinb, LOGS_INFO, "Screw you guys I am going home");
    
    xinb_release(xinb);
    return 0;
}
