#ifndef __COMMANDS_H__
#define __COMMANDS_H__

enum {
    COMMAND_TYPE_EXEC = 0,
    COMMAND_TYPE_MESSAGE,
    COMMAND_TYPE_SHEDULE /* I'll think about it. */
};

struct command_exec {
    gchar *file;
    gchar **args;
};

struct command_message {
    gchar *to;
    gchar *body;
};

struct command {
    gint type;
    union {
        struct command_exec *exec;
        struct command_message *message;
        /* TODO: struct command_shedule shedule. */
    } value;
};

#define COMMAND_MAX_LENGTH 256

gboolean command_run(struct xinb*, gchar*);

#endif
