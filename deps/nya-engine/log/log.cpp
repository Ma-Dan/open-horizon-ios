//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "log.h"
#include "stdout_log.h"
#include <stdarg.h>
#include <stdio.h>

namespace nya_log
{

namespace { log_base *current_log=new stdout_log(); }

log_base &no_log()
{
    static log_base *l=new log_base();
    return *l;
}

void set_log(log_base *l) { current_log=current_log?l:&no_log(); }
log_base &log() { return *current_log; }

log_base &log(const char *fmt, ...)
{
    va_list args,args_copy;
    va_start(args,fmt);
#ifdef _WIN32
    args_copy=args;
    const int len=_vscprintf(fmt,args)+1;
#else
    va_copy(args_copy,args);
    const int len=vsnprintf(0,0,fmt,args)+1;
#endif
    std::string buf;
    buf.resize(len);
    vsprintf(&buf[0],fmt,args_copy);
    *current_log<<buf;
    return *current_log;
}

}
