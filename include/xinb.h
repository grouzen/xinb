#ifndef __XINB_H__
#define __XINB_H__

struct xinb_account {
    gchar *server;
    gchar *username;
    gchar *password;
    gchar *resource;
    gchar *owner;
};

struct xinb {
    GMainLoop *loop;
    GMainContext *context;
    LmConnection *conn;
    GError *gerror;
    gchar *message;
    gchar *to;
    LmConnectionState state;
};

#endif
