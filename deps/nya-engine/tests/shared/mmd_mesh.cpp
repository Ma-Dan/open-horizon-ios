//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "mmd_mesh.h"

bool mmd_mesh::load(const char *name)
{
    unload();

    nya_scene::mesh::register_load_function(pmx_loader::load,false);
    nya_scene::mesh::register_load_function(pmd_loader::load,false);

    return mesh::load(name) && init();
}

bool mmd_mesh::init()
{
    m_pmd_data=pmd_loader::get_additional_data(*this);
    m_pmx_data=pmx_loader::get_additional_data(*this);

    if(!is_mmd())
        return true;

    m_vmorphs.create();
    m_morphs_count.create();
    m_bone_pos.create();
    m_bone_rot.create();

    if(m_pmd_data)
        m_morphs.resize(m_pmd_data->morphs.size());
    else if(m_pmx_data)
    {
        m_morphs.resize(m_pmx_data->morphs.size());
        m_morphs_count->y=float(m_pmx_data->vert_tex_w);
        m_morphs_count->z=float(m_pmx_data->vert_tex_h);
    }

    for(int i=0;i<get_groups_count();++i)
    {
        nya_scene::material &m=modify_material(i);
        m.set_param(m.get_param_idx("morphs count"), m_morphs_count);
        m.set_param_array(m.get_param_idx("morphs"), m_vmorphs);

        m.set_param_array(m.get_param_idx("mmd bones pos"), m_bone_pos);
        m.set_param_array(m.get_param_idx("mmd bones rot"), m_bone_rot);
    }

    m_skip_groups.resize(get_groups_count(),false);

    update(0);
    return true;
}

void mmd_mesh::unload()
{
    mesh::unload();
    m_pmd_data=0;
    m_pmx_data=0;
    m_morphs.clear();
}

void mmd_mesh::set_anim(const nya_scene::animation &anim,int layer,bool lerp)
{
    mmd_mesh::set_anim(nya_scene::animation_proxy(anim),layer,lerp);
}

void mmd_mesh::set_anim(const nya_scene::animation_proxy &anim,int layer,bool lerp)
{
    mesh::set_anim(anim,layer,lerp);
    if (!anim.is_valid())
        return;

    for(int i=0;i<m_morphs.size();++i)
        m_morphs[i].last_value=-1.0f;

    int idx=-1;
    for(int i=0;i<int(m_anims.size());++i)
    {
        if(m_anims[i].layer==layer)
        {
            idx=i;
            break;
        }
    }

    if(idx<0)
    {
        idx=int(m_anims.size());
        m_anims.resize(idx+1);
        m_anims[idx].layer=layer;
    }

    m_anims[idx].curves_map.clear();
    m_anims[idx].curves_map.resize(m_morphs.size());
    if(!anim->get_shared_data().is_valid())
        return;

    if(m_pmd_data)
    {
        for(int i=0;i<int(m_morphs.size());++i)
            m_anims[idx].curves_map[i]=anim->get_shared_data()->anim.get_curve_idx(m_pmd_data->morphs[i].name.c_str());
    }
    else if(m_pmx_data)
    {
        for(int i=0;i<int(m_morphs.size());++i)
            m_anims[idx].curves_map[i]=anim->get_shared_data()->anim.get_curve_idx(m_pmx_data->morphs[i].name.c_str());
    }
}

void mmd_mesh::set_morph(int idx,float value,bool overriden)
{
    if(idx<0 || idx>=int(m_morphs.size()))
        return;

    if(value<=0.001f)
        value=0.0f;

    m_morphs[idx].value=value;
    m_morphs[idx].overriden=overriden;
}

int mmd_mesh::find_morph(const char *name) const
{
    if(!name)
        return -1;

    for(int i=0;i<get_morphs_count();++i)
    {
        if(strcmp(get_morph_name(i),name)==0)
            return i;
    }

    return -1;
}

void mmd_mesh::update(unsigned int dt)
{
    mesh::update(dt);

    if(!is_mmd())
        return;

    for(int i=0;i<m_anims.size();++i)
    {
        const nya_scene::animation_proxy &ap=get_anim(m_anims[i].layer);
        if(!ap.is_valid() || !ap->get_shared_data().is_valid())
            continue;

        const nya_render::animation &a=ap->get_shared_data()->anim;
        const int time=get_anim_time(m_anims[i].layer);
        const float weight=ap->get_weight();
        if(i==0)
        {
            for(int j=0;j<m_anims[i].curves_map.size();++j)
            {
                if(m_morphs[j].overriden)
                    continue;

                m_morphs[j].value=a.get_curve(m_anims[i].curves_map[j],time)*weight;
            }
        }
        else
        {
            for(int j=0;j<m_anims[i].curves_map.size();++j)
            {
                if(m_morphs[j].overriden)
                    continue;

                m_morphs[j].value+=a.get_curve(m_anims[i].curves_map[j],time)*weight;
            }
        }
    }

    //update group morphs first

    if(m_pmx_data)
    {
        for(int i=0;i<(int)m_pmx_data->morphs.size();++i)
        {
            const pmx_loader::additional_data::morph &m=m_pmx_data->morphs[i];
            if(m.type!=pmx_loader::morph_type_group)
                continue;

            if(m_morphs[i].value==m_morphs[i].last_value)
                continue;

            const pmx_loader::additional_data::group_morph &gm=m_pmx_data->group_morphs[m.idx];
            for(int j=0;j<(int)gm.morphs.size();++j)
                m_morphs[gm.morphs[j].first].value=m_morphs[i].value*gm.morphs[j].second;

            m_morphs[i].last_value=m_morphs[i].value;
        }
    }

    if(m_vmorphs.is_valid())
        m_vmorphs->set_count(0);

    for(int i=0;i<m_morphs.size();++i)
    {
        if(m_pmd_data)
        {
            //ToDo

            /*            const pmd_morph_data::morph &m=m_pmd_data->morphs[i];
             for(int j=0;j<int(m.verts.size());++j)
             m_vertex_data[m.verts[j].idx].pos+=m.verts[j].pos*m_morphs[i].value;
             */
        }
        else if(m_pmx_data)
        {
            const pmx_loader::additional_data::morph &m=m_pmx_data->morphs[i];
            switch(m.type)
            {
                case pmx_loader::morph_type_vertex:
                {
                    if(m_morphs[i].value<0.001f)
                        continue;

                    const int idx=m_vmorphs->get_count();
                    m_vmorphs->set_count(idx+1);
                    m_vmorphs->set(idx,m_morphs[i].value,float(m.idx*m_pmx_data->mvert_count));
                    break;
                }

                case pmx_loader::morph_type_uv:
                {
                    /*
                     const pmx_loader::additional_data::uv_morph &vm=m_pmx_data->uv_morphs[m.idx];
                     const pmx_loader::vert *verts=(const pmx_loader::vert *)&m_original_vertex_data[0];
                     for(int j=0;j<int(vm.verts.size());++j)
                     {
                     const int idx=vm.verts[j].idx;
                     m_vertex_data[idx].tc=verts[idx].tc+vm.verts[j].tc*m_morphs[i].value;
                     }
                     */
                    break;
                }

                case pmx_loader::morph_type_material:
                {
                    if(m_morphs[i].value==m_morphs[i].last_value)
                        continue;

                    const float value=m_morphs[i].value;
                    const pmx_loader::additional_data::mat_morph &mm=m_pmx_data->mat_morphs[m.idx];
                    for(int j=0;j<int(mm.mats.size());++j)
                    {
                        const pmx_loader::additional_data::morph_mat &m=mm.mats[j];
                        const pmx_loader::additional_data::mat &om=m_pmx_data->materials[m.mat_idx];
                        nya_scene::material &mat=modify_material(m.mat_idx);
                        
                        pmx_loader::pmx_material_params params;
                        pmx_loader::pmx_edge_params edge;
                        
                        if(m.op==pmx_loader::additional_data::morph_mat::op_add)
                        {
                            params.ambient=om.params.ambient+m.params.ambient*value;
                            params.diffuse=om.params.diffuse+m.params.diffuse*value;
                            params.specular=om.params.specular+m.params.specular*value;
                            params.shininess=om.params.shininess+m.params.shininess*value;
                            
                            edge.color=om.edge_params.color+m.edge_params.color*value;
                            edge.width=om.edge_params.width+m.edge_params.width*value;
                        }
                        else if(m.op==pmx_loader::additional_data::morph_mat::op_mult)
                        {
                            params.ambient=nya_math::vec3::lerp(om.params.ambient,om.params.ambient*m.params.ambient,value);
                            params.diffuse=nya_math::vec4::lerp(om.params.diffuse,om.params.diffuse*m.params.diffuse,value);
                            params.specular=nya_math::vec3::lerp(om.params.specular,om.params.specular*m.params.specular,value);
                            params.shininess=nya_math::lerp(om.params.shininess,om.params.shininess*m.params.shininess,value);
                            
                            edge.color=nya_math::vec4::lerp(om.edge_params.color,om.edge_params.color*m.edge_params.color,value);
                            edge.width=nya_math::lerp(om.edge_params.width,om.edge_params.width*m.edge_params.width,value);
                        }
                        
                        mat.set_param(mat.get_param_idx("amb k"),params.ambient,1.0f);
                        mat.set_param(mat.get_param_idx("diff k"),params.diffuse);
                        mat.set_param(mat.get_param_idx("spec k"),params.specular,params.shininess);

                        const float eps = 0.001f;
                        m_skip_groups[m.mat_idx]=params.diffuse.w<eps;

                        if(om.edge_group_idx>=0)
                        {
                            nya_scene::material &mat=modify_material(om.edge_group_idx);
                            mat.set_param(mat.get_param_idx("edge offset"),edge.width*0.02f,edge.width*0.02f,edge.width*0.02f,0.0f);
                            mat.set_param(mat.get_param_idx("edge color"),edge.color);
                            m_skip_groups[om.edge_group_idx]=edge.color.w<eps || edge.width<eps;
                        }
                    }
                    break;
                }

                case pmx_loader::morph_type_bone:
                {
                    if(m_morphs[i].value==m_morphs[i].last_value)
                        continue;

                    const pmx_loader::additional_data::bone_morph &bm=m_pmx_data->bone_morphs[m.idx];
                    for(int j=0;j<int(bm.bones.size());++j)
                    {
                        set_bone_pos(bm.bones[j].idx,bm.bones[j].pos*m_morphs[i].value,true);
                        nya_math::quat rot=bm.bones[j].rot;
                        rot.apply_weight(m_morphs[i].value);
                        set_bone_rot(bm.bones[j].idx,rot,true);
                    }
                    break;
                }

                default: break;
            }
        }

        m_morphs[i].last_value=m_morphs[i].value;
    }

    if(m_morphs_count.is_valid())
        m_morphs_count->x=float(m_vmorphs->get_count());

    if(m_pmx_data && m_pmx_data->bone_remaps.size()==1) //ToDo
        remap_skeleton(0);
}

const char *mmd_mesh::get_morph_name(int idx) const
{
    if(idx<0 || idx>=get_morphs_count())
        return 0;

    if (m_pmd_data)
        return m_pmd_data->morphs[idx].name.c_str();

    if (m_pmx_data)
        return m_pmx_data->morphs[idx].name.c_str();

    return 0;
}

void mmd_mesh::remap_skeleton(int remap_idx) const
{
    auto &rm=m_pmx_data->bone_remaps[remap_idx];
    m_bone_pos->set_count((int)rm.bones.size());
    m_bone_rot->set_count((int)rm.bones.size());

    auto &sk=internal().get_skeleton();

    for(int i=0;i<(int)rm.bones.size();++i)
    {
        const int idx=rm.bones[i];

        nya_math::quat rot=sk.get_bone_rot(idx);
        nya_math::vec3 pos=sk.get_bone_pos(idx)+rot.rotate(-sk.get_bone_original_pos(idx));

        m_bone_pos->set(i, pos);
        m_bone_rot->set(i, nya_math::vec4(rot.v,rot.w));
    }
}

void mmd_mesh::draw(const char *pass_name) const
{
    if(!m_pmx_data)
    {
        mesh::draw(pass_name);
        return;
    }

    if(m_pmx_data->bone_remaps.size()==1)
    {
        for(int i=0;i<(int)m_skip_groups.size();++i)
        {
            if(!m_skip_groups[i])
                mesh::draw_group(i,pass_name);
        }
        return;
    }

    //ToDo
}

void mmd_mesh::draw_group(int group_idx,const char *pass_name) const
{
    if(!m_pmx_data)
    {
        mesh::draw_group(group_idx,pass_name);
        return;
    }

    if(!m_skip_groups.empty() && (group_idx<0 || group_idx>=(int)m_skip_groups.size() || m_skip_groups[group_idx]))
        return;

    if(m_pmx_data->bone_remaps.size()==1)
    {
        mesh::draw_group(group_idx,pass_name);
        return;
    }

    //ToDo
}

pmd_morph_data::morph_kind mmd_mesh::get_morph_kind(int idx) const
{
    if(idx<0 || idx>=get_morphs_count())
        return pmd_morph_data::morph_base;

    if (m_pmd_data)
        return m_pmd_data->morphs[idx].kind;

    if (m_pmx_data)
        return m_pmx_data->morphs[idx].kind;

    return pmd_morph_data::morph_base;
}

float mmd_mesh::get_morph(int idx) const
{
    if(idx<0 || idx>=get_morphs_count())
        return 0.0f;

    return m_morphs[idx].value;
}
