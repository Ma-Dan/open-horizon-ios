//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "log.h"

namespace nya_log
{

class plain_file_log: public log_base
{
public:
    bool open(const char *file_name,bool clear=true);
    void close();

    plain_file_log(const char *file_name="",bool clear=true) { open(file_name,clear); }

private:
    virtual void output(const char *string);

private:
    std::string m_file_name;
};

}
