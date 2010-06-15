#ifndef __LOGS_H__
#define __LOGS_H__

#include <syslog.h>

#define LOGS_INFO LOG_INFO
#define LOGS_ERR LOG_ERR

FILE *log_open(Xinb*);
void log_close(Xinb*);
void log_record(Xinb*, gint, gchar*, ...);

#endif
