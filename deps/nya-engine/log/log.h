//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "output_stream.h"

namespace nya_log
{

class log_base: public ostream_base
{
protected:
    virtual void output(const char *string) {}

public:
    virtual ~log_base() {}
};

log_base &no_log();

void set_log(log_base *l);

log_base &log();
log_base &log(const char *fmt, ...);

}
