#ifndef __XINB_H__
#define __XINB_H__

struct xinb {
    GMainLoop *loop;
    GMainContext *context;
    GHashTable *account;
    LmConnection *conn;
    GError *gerror;
    gchar *message;
    gchar *to;
    LmConnectionState state;
};

void xinb_release(struct xinb*);
void xinb_send_stream(struct xinb*, FILE*);

#endif
