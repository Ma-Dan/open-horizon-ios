//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "plain_file_log.h"
#include <stdio.h>

namespace nya_log
{

void plain_file_log::output(const char *str)
{
    if(FILE *f=fopen(m_file_name.c_str(),"a+"))
    {
        fprintf(f,"%s",str?str:"NULL");
        fclose(f);
    }
}

bool plain_file_log::open(const char *file_name,bool clear)
{
    if(FILE *f=fopen(file_name,clear?"w+":"a+"))
    {
        fclose(f);
        m_file_name.assign(file_name);
        return true;
    }

    m_file_name.clear();
    return false;
}

void plain_file_log::close() { m_file_name.clear(); }

}
