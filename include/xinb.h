#ifndef __XINB_H__
#define __XINB_H__

#include <stdio.h>

typedef struct {
    GMainLoop *loop;
    GMainContext *context;
    GHashTable *config;
    LmConnection *conn;
    GError *gerror;
    gchar *message;
    gchar *to;
    gint start_time;
    gchar *cwd;
    GSList* mlog;
    LmConnectionState state;
    FILE *logfd;
} Xinb;

void xinb_release(Xinb*);

#define XINB_DIR ".xinb"
#define XINB_PROGRAM_NAME "xinb"

#endif
