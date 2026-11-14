//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "scene/mesh.h"

#include "load_pmd.h"
#include "load_pmx.h"

class mmd_mesh: public nya_scene::mesh
{
public:
    bool load(const char *name);
    void unload();

    void set_anim(const nya_scene::animation &anim,int layer=0,bool lerp=false);
    void set_anim(const nya_scene::animation_proxy &anim,int layer=0,bool lerp=false);

    void set_morph(int idx,float value,bool overriden);
    int find_morph(const char *name) const;

    void update(unsigned int dt);

    void draw(const char *pass_name=nya_scene::material::default_pass) const;
    void draw_group(int group_idx,const char *pass_name=nya_scene::material::default_pass) const;

    int get_morphs_count() const { return (int)m_morphs.size(); }
    const char *get_morph_name(int idx) const;
    pmd_morph_data::morph_kind get_morph_kind(int idx) const;
    float get_morph(int idx) const;

    bool is_mmd() const { return m_pmd_data!=0 || m_pmx_data!=0; }

    mmd_mesh(): m_pmd_data(0),m_pmx_data(0) {}

    mmd_mesh(const mmd_mesh &from): mesh(from) { init(); m_morphs=from.m_morphs; m_anims=from.m_anims; }

    mmd_mesh &operator = (const mmd_mesh &from)
    {
        if(this==&from)
            return *this;

        this->~mmd_mesh();
        return *new(this)mmd_mesh(from);
    }

private:
    bool init();
    void remap_skeleton(int remap_idx) const;
    
    const pmd_loader::additional_data *m_pmd_data;
    const pmx_loader::additional_data *m_pmx_data;

    struct morph
    {
        bool overriden;
        float value;
        float last_value;

        morph(): overriden(false),value(0.0f),last_value(0.0f) {}
    };

    std::vector<morph> m_morphs;
    nya_scene::material::param_proxy m_morphs_count;
    nya_scene::material::param_array_proxy m_vmorphs;

    mutable nya_scene::material::param_array_proxy m_bone_pos;
    mutable nya_scene::material::param_array_proxy m_bone_rot;

    struct applied_anim { int layer; std::vector<int> curves_map; };
    std::vector<applied_anim> m_anims;

    std::vector<bool> m_skip_groups;
};
