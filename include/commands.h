#ifndef __COMMANDS_H__
#define __COMMANDS_H__

enum {
    COMMAND_TYPE_NONE = -1,
    COMMAND_TYPE_EXEC,
    COMMAND_TYPE_MESSAGE,
    COMMAND_TYPE_SERVICE /* I'll think about it. */
};

gboolean command_run(Xinb*, gchar*);

#endif
