//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "log.h"

namespace nya_log
{

class android_log: public log_base
{
public:
    void set_id(const char *id) { m_id.assign(id?id:""); }

    android_log(const char *id="nya"): m_id(id) {}

private:
    virtual void output(const char *str);

private:
    std::string m_id,m_buf;
};

}
