#ifndef __ERROR_H__
#define __ERROR_H__

enum {
    XINB_COMMAND_EXEC_ERROR,
    XINB_COMMAND_GET_TYPE_ERROR,
    XINB_XINB_DAEMONIZE_ERROR
};

GQuark xinb_error_quark(void);

#define XINB_ERROR xinb_error_quark()

#endif
