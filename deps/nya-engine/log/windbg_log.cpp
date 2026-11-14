//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "windbg_log.h"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace nya_log
{

void windbg_log::output(const char *str)
{
#ifdef _WIN32
    OutputDebugStringA(str);
#endif
}

}
