//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "stdout_log.h"
#include <stdio.h>

namespace nya_log
{

void stdout_log::output(const char *str) { printf("%s", str?str:"NULL"); }

}
