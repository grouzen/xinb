#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <loudmouth/loudmouth.h>

#include "../include/xinb.h"
#include "../include/logs.h"

FILE *log_open(Xinb *x)
{
    FILE *fd;

    gchar *log_name, *verbose, *daemon;

    verbose = g_hash_table_lookup(x->config, "verbose");
    daemon = g_hash_table_lookup(x->config, "daemon");

    if(!g_strcmp0(verbose, "on") && !g_strcmp0(daemon, "on"))
        openlog(XINB_PROGRAM_NAME, LOG_PID, LOG_USER);

    log_name = g_strdup_printf("%s/%s/xinb.log", getenv("HOME"), XINB_DIR);
    
    fd = fopen(log_name, "a");
    if(!fd) {
        if(!g_strcmp0(daemon, "on")) {
            syslog(LOGS_ERR, "Couldn't create new log file '%s': %s",
                   log_name, strerror(errno));
        } else {
            g_printerr("Couldn't create new log file '%s': %s.\n",
                       log_name, strerror(errno));
        }
    }
    
    g_free(log_name);
    return fd;
}

void log_close(Xinb *x)
{
    gchar *verbose, *daemon;

    verbose = g_hash_table_lookup(x->config, "verbose");
    daemon = g_hash_table_lookup(x->config, "daemon");
    
    if(!g_strcmp0(verbose, "on") && !g_strcmp0(daemon, "on"))
        closelog();
    
    if(x->logfd && fclose(x->logfd) == EOF)
        g_printerr("Couldn't close log file: %s.\n", strerror(errno));

    return;
}

void log_record(Xinb *x, gint level, gchar *format, ...)
{
    va_list ap;
    time_t t;
    struct tm *date;
    gchar *verbose, *daemon, *status, *record;
    gchar out[1024];

    va_start(ap, format);
    verbose = g_hash_table_lookup(x->config, "verbose");
    daemon = g_hash_table_lookup(x->config, "daemon");
    vsnprintf(out, 1024, format, ap);
    
    if(!g_strcmp0(verbose, "on")) {
        if(!g_strcmp0(daemon, "on")) {
            vsyslog(level, format, ap);
        } else {
            if(level == LOGS_INFO)
                g_print("%s\n", out);
            else if(level == LOGS_ERR)
                g_printerr("%s\n", out);
        }
    }
    va_end(ap);
    
    if(level == LOGS_INFO)
        status = g_strdup("[INFO]");
    else if(level == LOGS_ERR)
        status = g_strdup("[ERR]");

    t = time(NULL);
    date = localtime(&t);
    record = g_strdup_printf("%02d-%02d-%d:%02d%02d%02d %s %s\n",
                             date->tm_mday, date->tm_mon + 1, date->tm_year + 1900,
                             date->tm_hour, date->tm_min, date->tm_sec,
                             status, out);
    fputs(record, x->logfd);
    
    g_free(status);
    g_free(record);
    return;
}
