//
// open horizon -- undefined_darkness@outlook.com
//

#include "containers/qdf_provider.h"
#include "containers/dpl_provider.h"

#include "util/config.h"
#include "util/platform.h"

#include "resources/file_resources_provider.h"
#include "resources/composite_resources_provider.h"
#include "extensions/zip_resources_provider.h"

#include "system/system.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#ifndef _WIN32
    #include <unistd.h>
    #include <sys/stat.h>
#endif

#include "resources.h"

namespace { nya_resources::zip_resources_provider zprov; }

//------------------------------------------------------------

//Pack tooling (env-gated, no effect on normal play):
//  OH_PACK_OUT=<dir>  record-and-repack: every resource served from the original
//                    game data (qdf/dlc/acah dir) is copied into <dir> under its
//                    flat resource name, plus a _manifest.txt line per entry.
//  OH_PACK=<dir>      standalone pack mode: no QDF/DLC/original data is opened at
//                    all; resources come from the app dir (repo resources/) and
//                    the pack dir only. This is the iPhone-bundle resource shape.
namespace
{

bool oh_pack_mode()
{
#ifdef OH_PLATFORM_IOS
    return true; //iPhone: resources come from the bundle-embedded pack only
#else
    const char *p = getenv("OH_PACK");
    return p && p[0];
#endif
}

#ifdef OH_PLATFORM_IOS
static const char *oh_pack_dir()
{
    //bundle root (nya_system::get_app_path() = <app>.app/) + pack folder
    static std::string dir = std::string(nya_system::get_app_path()) + "pack";
    return dir.c_str();
}
#endif

void mkpath_dirs(const std::string &path)
{
    for (size_t i = 1; i < path.size(); ++i)
    {
        if (path[i] != '/')
            continue;
        std::string sub = path.substr(0, i);
#ifndef _WIN32
        mkdir(sub.c_str(), 0755);
#endif
    }
}

void pack_record(const char *source_tag, const char *flat_name,
                 nya_resources::resource_data *d, bool copy_bytes = true)
{
    const char *dir = getenv("OH_PACK_OUT");
    if (!dir || !dir[0] || !d)
        return;

    static std::mutex m;
    static std::set<std::string> done;
    std::lock_guard<std::mutex> lock(m);
    if (!done.insert(flat_name).second)
        return;

    if (copy_bytes)
    {
        std::vector<char> buf(d->get_size());
        if (!buf.empty() && !d->read_all(buf.data()))
            return;

        std::string path = std::string(dir) + "/" + flat_name;
        mkpath_dirs(path);
        FILE *f = fopen(path.c_str(), "wb");
        if (!f)
            return;
        fwrite(buf.data(), 1, buf.size(), f);
        fclose(f);
    }

    std::string man = std::string(dir) + "/_manifest.txt";
    FILE *mfile = fopen(man.c_str(), "ab");
    if (mfile)
    {
        fprintf(mfile, "%s\t%s\t%zu\n", source_tag, flat_name, d->get_size());
        fclose(mfile);
    }
}

}

//------------------------------------------------------------

bool setup_resources()
{
#ifndef _WIN32
    chdir(nya_system::get_app_path());
#endif

    config::register_var("acah_path", "");

    static qdf_resources_provider qdfp;
    if (!oh_pack_mode() &&
        !qdfp.open_archive((config::get_var("acah_path") + "datafile.qdf").c_str()))
    {
        static const char message[] = "You are running Open Horizon outside of the Assault Horizon folder.\n"
                                      "Please specify the path to the Assault Horizon folder.\n"
                                      "It will be saved automatically.";

        if (platform::show_msgbox(message))
        {
            std::string folder = platform::open_folder_dialog();
            if (!folder.length())
                return false;

            config::set_var("acah_path", folder);

            if (!qdfp.open_archive((config::get_var("acah_path") + "datafile.qdf").c_str()))
                return false;
        }
        else
            return false;
    }

    class target_resource_provider: public nya_resources::resources_provider
    {
        nya_resources::resources_provider &m_provider;
        dpl_resources_provider m_dlc_provider;
        nya_resources::file_resources_provider m_fprov;
        nya_resources::file_resources_provider m_fprov2;
        nya_resources::composite_resources_provider m_cprov;
        bool m_pack_mode;

    public:
        target_resource_provider(nya_resources::resources_provider &provider):
            m_provider(provider), m_pack_mode(oh_pack_mode())
        {
            m_fprov.set_folder(nya_system::get_app_path());
#ifdef OH_PLATFORM_IOS
            m_fprov2.set_folder(oh_pack_dir());
#else
            if (m_pack_mode)
                m_fprov2.set_folder(getenv("OH_PACK"));
            else
                m_fprov2.set_folder(config::get_var("acah_path").c_str());
#endif

            m_cprov.add_provider(&m_fprov);
            m_cprov.add_provider(&m_fprov2);
            if (!m_pack_mode)
                m_cprov.add_provider(&provider);
            nya_resources::set_resources_provider(this);

            if (!m_pack_mode)
                m_dlc_provider.open_archive("target/DATA.PAC", "DATA.PAC.xml");
        }

        //enumeration forwards to the composite stack (app dir + pack dir):
        //used by list_files() for missions/ and campaigns/ listings
        int get_resources_count() override
        {
            return m_cprov.get_resources_count();
        }

        const char *get_resource_name(int idx) override
        {
            return m_cprov.get_resource_name(idx);
        }

    private:
        nya_resources::resource_data *access(const char *resource_name)
        {
            if (!resource_name)
                return 0;

            if (zprov.has(resource_name))
                return zprov.access(resource_name);

            if (m_dlc_provider.has(resource_name))
            {
                nya_resources::resource_data *d = m_dlc_provider.access(resource_name);
                pack_record("dlc", resource_name, d);
                return d;
            }

            const std::string str(resource_name);
            if (!m_pack_mode && m_provider.has(("target/" + str).c_str()))
            {
                nya_resources::resource_data *d = m_provider.access(("target/" + str).c_str());
                pack_record("qdf:target", resource_name, d);
                return d;
            }

            if (!m_pack_mode && m_provider.has(("common/" + str).c_str()))
            {
                nya_resources::resource_data *d = m_provider.access(("common/" + str).c_str());
                pack_record("qdf:common", resource_name, d);
                return d;
            }

            if (!m_pack_mode && m_provider.has(resource_name))
            {
                nya_resources::resource_data *d = m_provider.access(resource_name);
                pack_record("qdf", resource_name, d);
                return d;
            }

            if (m_fprov.has(resource_name))
            {
                nya_resources::resource_data *d = m_fprov.access(resource_name);
                pack_record("app", resource_name, d, false /*already in repo resources/*/);
                return d;
            }

            nya_resources::resource_data *d = m_fprov2.access(resource_name);
            if (!d && m_pack_mode)
                fprintf(stderr, "OH_PACK_MISS: %s\n", resource_name);
            if (!m_pack_mode)
                pack_record("acah", resource_name, d);
            return d;
        }

        bool has(const char *resource_name)
        {
            if (!resource_name)
                return false;

            const std::string str(resource_name);
            return (m_dlc_provider.has(resource_name)) ||
                   (!m_pack_mode && (m_provider.has(("target/" + str).c_str()) || m_provider.has(("common/" + str).c_str()) || m_provider.has(resource_name))) ||
                   m_fprov.has(resource_name) || m_fprov2.has(resource_name);
        }

    } static trp(qdfp);

    nya_resources::set_resources_provider(&trp);
    return true;
}

//------------------------------------------------------------

bool set_zip_mod(const char *name)
{
    return zprov.open_archive(name);
}

//------------------------------------------------------------
