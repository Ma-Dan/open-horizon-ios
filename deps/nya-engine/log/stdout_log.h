//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "log.h"

namespace nya_log
{

class stdout_log: public log_base
{
private:
    virtual void output(const char *str);
};

}
