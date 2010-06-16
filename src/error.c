#include <loudmouth/loudmouth.h>

#include "../include/error.h"

GQuark xinb_error_quark(void)
{
    GQuark q = 0;

    if(q == 0)
        q = g_quark_from_static_string("xinb-error-quark");

    return q;
}
