//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "android_log.h"

#ifdef __ANDROID__
    #include <android/log.h>
    #include "memory/mutex.h"
#endif

namespace nya_log
{

void android_log::output(const char *str)
{
#ifdef __ANDROID__
    if(!str)
        return;

    static nya_memory::mutex mutex;
    nya_memory::lock_guard guard(mutex);

    m_buf.append(str);
    for(size_t i=m_buf.find("\n");i!=std::string::npos;i=m_buf.find("\n"))
    {
        std::string tmp=m_buf.substr(0,i);
        m_buf=m_buf.substr(i+1);
        __android_log_print(ANDROID_LOG_INFO,m_id.c_str(),"%s",tmp.c_str());
    }
#endif
}

}
