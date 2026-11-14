//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include <string>

namespace nya_log
{

class ostream_base
{
protected:
    virtual void output(const char *str) {}

public:
    ostream_base &operator << (long int);
    ostream_base &operator << (unsigned long int);
    ostream_base &operator << (float);
    ostream_base &operator << (double);
    ostream_base &operator << (const char *);

    ostream_base &operator << (int a);
    ostream_base &operator << (unsigned int a);
    ostream_base &operator << (short int a);
    ostream_base &operator << (unsigned short int a);
    ostream_base &operator << (long long int a);
    ostream_base &operator << (unsigned long long int a);
    ostream_base &operator << (const std::string &a);
};

}
