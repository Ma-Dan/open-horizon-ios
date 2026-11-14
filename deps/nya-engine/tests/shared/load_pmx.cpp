//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "load_pmx.h"
#include "scene/mesh.h"
#include "memory/memory_reader.h"
#include "string_encoding.h"

#include "resources/resources.h"

namespace
{

struct add_data: public pmx_loader::additional_data, nya_scene::shared_mesh::additional_data
{
    const char *type() { return "pmx"; }
};

int read_idx(nya_memory::memory_reader &reader,int size)
{
    switch(size)
    {
        case 1: return reader.read<int8_t>();
        case 2: return reader.read<int16_t>();
        case 4: return reader.read<int32_t>();
    }

    return 0;
}

inline std::string read_string(nya_memory::memory_reader &reader,char encoding)
{
    const int name_len=reader.read<int>();
    std::string str=encoding?std::string((const char*)reader.get_data(),name_len):
    utf8_from_utf16le(reader.get_data(),name_len);
    reader.skip(name_len);
    return str;
}

inline void skip_string(nya_memory::memory_reader &reader) { reader.skip(reader.read<int>()); }

template<typename t> void load_vertex_morph(nya_memory::memory_reader &reader,pmx_loader::additional_data::vert_morph &m)
{
    for(size_t j=0;j<m.verts.size();++j)
    {
        pmd_morph_data::morph_vertex &v=m.verts[j];
        v.idx=reader.read<t>();
        v.pos.x=reader.read<float>();
        v.pos.y=reader.read<float>();
        v.pos.z=-reader.read<float>();
    }
}

template<typename t> void load_uv_morph(nya_memory::memory_reader &reader,pmx_loader::additional_data::uv_morph &m)
{
    for(size_t j=0;j<m.verts.size();++j)
    {
        add_data::morph_uv &v=m.verts[j];
        v.idx=reader.read<t>();
        v.tc.x=reader.read<float>();
        v.tc.y=-reader.read<float>();
        reader.skip(2*4); //ToDo
    }
}

inline void read_vector(nya_math::vec3 &out,nya_memory::memory_reader &reader)
{
    out.x=reader.read<float>(),out.y=reader.read<float>(),out.z=-reader.read<float>();
}

inline void read_angle(nya_math::vec3 &out,nya_memory::memory_reader &reader)
{
    out.x=-reader.read<float>(),out.y=-reader.read<float>(),out.z=reader.read<float>();
}

}

bool pmx_loader::load(nya_scene::shared_mesh &res,nya_scene::resource_data &data,const char* name)
{
    if(!data.get_size())
        return false;

    nya_memory::memory_reader reader(data.get_data(),data.get_size());
    if(!reader.test("PMX ",4))
        return false;

    if(reader.read<float>()!=2.0f)
    {
        nya_log::log()<<"pmx load error: invalid version\n";
        return false;
    }

    const char header_size=reader.read<uint8_t>();
    if(header_size!=sizeof(pmx_header))
    {
        nya_log::log()<<"pmx load error: invalid header\n";
        return false;
    }
    
    const pmx_header header=reader.read<pmx_header>();
    
    for(int i=0;i<4;++i)
    {
        const int size=reader.read<int>();
        reader.skip(size);
    }
    
    const int vert_count=reader.read<int>();
    if(!vert_count)
    {
        nya_log::log()<<"pmx load error: no verts found\n";
        return false;
    }

    std::vector<vert> verts(vert_count);

    for(int i=0;i<vert_count;++i)
    {
        vert &v=verts[i];
        read_vector(v.pos,reader);
        read_vector(v.normal,reader);

        v.tc.x=reader.read<float>();
        v.tc.y=1.0f-reader.read<float>();
        reader.skip(header.extended_uv*sizeof(float)*4);

        switch(reader.read<uint8_t>())
        {
            case 0:
                v.bone_idx[0]=(float)read_idx(reader,header.bone_idx_size);
                v.bone_weight[0]=1.0f;
                for(int j=1;j<4;++j)
                    v.bone_weight[j]=v.bone_idx[j]=0.0f;
                break;
                
            case 1:
                v.bone_idx[0]=(float)read_idx(reader,header.bone_idx_size);
                v.bone_idx[1]=(float)read_idx(reader,header.bone_idx_size);
                
                v.bone_weight[0]=reader.read<float>();
                v.bone_weight[1]=1.0f-v.bone_weight[0];
                
                for(int j=2;j<4;++j)
                    v.bone_weight[j]=v.bone_idx[j]=0.0f;
                break;
                
            case 2:
                for(int j=0;j<4;++j)
                    v.bone_idx[j]=(float)read_idx(reader,header.bone_idx_size);
                
                for(int j=0;j<4;++j)
                    v.bone_weight[j]=reader.read<float>();
                break;
                
            case 3:
                v.bone_idx[0]=(float)read_idx(reader,header.bone_idx_size);
                v.bone_idx[1]=(float)read_idx(reader,header.bone_idx_size);
                
                v.bone_weight[0]=reader.read<float>();
                v.bone_weight[1]=1.0f-v.bone_weight[0];
                
                for(int j=2;j<4;++j)
                    v.bone_weight[j]=v.bone_idx[j]=0.0f;

                reader.skip(sizeof(float)*3*3);
                break;

            default:
                nya_log::log()<<"pmx load error: invalid skining\n";
                return false;
        }

        reader.read<float>(); //edge
    }

    const int indices_count=reader.read<int>();
    const void *indices=reader.get_data();
    if(header.index_size==2)
        res.vbo.set_index_data(indices,nya_render::vbo::index2b,indices_count);
    else if(header.index_size==4)
        res.vbo.set_index_data(indices,nya_render::vbo::index4b,indices_count);
    else if(header.index_size==1)
    {
        const unsigned char *data=(unsigned char *)indices;
        std::vector<unsigned short> tmp(indices_count);
        for(int i=0;i<tmp.size();++i)
            tmp[i]=data[i];
        res.vbo.set_index_data(tmp.data(),nya_render::vbo::index2b,indices_count);
    }
    else
    {
        nya_log::log()<<"pmx load error: invalid index size\n";
        return false;
    }

    reader.skip(indices_count*header.index_size);

    const int textures_count=reader.read<int>();
    std::vector<std::string> tex_names(textures_count);
    for(int i=0;i<textures_count;++i)
        tex_names[i]=read_string(reader,header.text_encoding);

    const int mat_count=reader.read<int>();
    res.groups.resize(mat_count);
    res.materials.resize(mat_count);
    
    std::string path(name);
    size_t p=path.rfind("/");
    if(p==std::string::npos)
        p=path.rfind("\\");
    if(p==std::string::npos)
        path.clear();
    else
        path.resize(p+1);

    //nya_resources::file_resources_provider frp; frp.set_folder(path.c_str()); for(nya_resources::resource_info *fri=frp.first_res_info();fri;fri=fri->get_next()) printf("%s\n",fri->get_name());

    const nya_math::vec3 mmd_light_dir=-nya_math::vec3(-0.5,-1.0,-0.5).normalize();
    //const nya_math::vec3 mmd_light_color=nya_math::vec3(154,154,154)/255.0;

    add_data *ad=new add_data;
    res.add_data=ad;

    ad->materials.resize(mat_count);

    nya_scene::shader sh("pmx.nsh");

    for(int i=0,offset=0;i<mat_count;++i)
    {
        nya_scene::shared_mesh::group &g=res.groups[i];
        nya_scene::material &m = res.materials[i];

        read_string(reader,header.text_encoding);
        m.set_name(read_string(reader,header.text_encoding).c_str());

        const pmx_material_params &params=ad->materials[i].params=reader.read<pmx_material_params>();

        const unsigned char flag=reader.read<uint8_t>();
        const pmx_edge_params &edge=ad->materials[i].edge_params=reader.read<pmx_edge_params>();
        ad->materials[i].edge_group_idx=-1;

        const int tex_idx=read_idx(reader,header.texture_idx_size);
        const int sph_tex_idx=read_idx(reader,header.texture_idx_size);
        const int sph_mode=reader.read<int8_t>();
        const unsigned char toon_flag=reader.read<uint8_t>();
        
        int toon_tex_idx=-1;
        if(toon_flag==0)
        {
            toon_tex_idx=read_idx(reader,header.texture_idx_size);
            if(toon_tex_idx>=(int)tex_names.size())
            {
                nya_log::log()<<"pmx load error: invalid toon tex idx\n";
                return false;
            }
        }
        else if(toon_flag==1)
            toon_tex_idx=reader.read<int8_t>();
        else
        {
            nya_log::log()<<"pmx load error: invalid toon flag\n";
            return false;
        }

        if(tex_idx>=(int)tex_names.size())
        {
            nya_log::log()<<"pmx load error: invalid tex idx\n";
            return false;
        }

        if(sph_tex_idx>=(int)tex_names.size())
        {
            nya_log::log()<<"pmx load error: invalid sph tex idx\n";
            return false;
        }

        const int comment_len=reader.read<int>();
        reader.skip(comment_len);

        g.name="mesh";
        g.offset=offset;
        g.count=reader.read<int>();
        g.material_idx=i;
        offset+=g.count;

        {
            nya_scene::texture tex;
            if(tex_idx<0 || !tex.load((path+tex_names[tex_idx]).c_str()))
                tex.build("\xff\xff\xff\xff",1,1,nya_render::texture::color_bgra);
            m.set_texture("diffuse",tex);
        }

        {
            bool loaded=false;
            nya_scene::texture tex;
            if(toon_flag>0)
            {
                char buf[255];
                sprintf(buf,"toon%02d.bmp",toon_tex_idx+1);
                if(nya_resources::get_resources_provider().has((path+buf).c_str()))
                    loaded=tex.load((path+buf).c_str());
                else
                    loaded=tex.load(buf);
            }
            else if(toon_tex_idx>=0)
                loaded=tex.load((path+tex_names[toon_tex_idx]).c_str());

            if(!loaded)
                tex.build("\xff\xff\xff\xff",1,1,nya_render::texture::color_bgra);

            nya_render::texture rtex=tex.internal().get_shared_data()->tex;
            rtex.set_wrap(nya_render::texture::wrap_clamp, nya_render::texture::wrap_clamp);
            m.set_texture("toon",tex);
        }

        nya_scene::material::pass &p=m.get_pass(m.add_pass(nya_scene::material::default_pass));
        p.set_shader(sh);
        p.get_state().set_blend(true,nya_render::blend::src_alpha,nya_render::blend::inv_src_alpha);
        p.get_state().set_cull_face(!(flag & (1<<0)),nya_render::cull_face::cw);

        {
            nya_scene::texture tex;
            if(sph_tex_idx>=0 && tex.load((path+tex_names[sph_tex_idx]).c_str()))
            {
                m.set_texture("env",tex);
                if(sph_mode==2)
                    m.set_param(m.get_param_idx("env param"), 0.0f,1.0f);
                else if(sph_mode==1)
                    m.set_param(m.get_param_idx("env param"), 1.0f,0.0f);
            }
            else
                tex.build("\xff\xff\xff\xff",1,1,nya_render::texture::color_bgra);
        }

        m.set_param(m.get_param_idx("light dir"),mmd_light_dir);
        m.set_param(m.get_param_idx("amb k"),params.ambient,1.0f);
        m.set_param(m.get_param_idx("diff k"),params.diffuse);
        m.set_param(m.get_param_idx("spec k"),params.specular,params.shininess);

        if(!(flag & (1<<4)))
            continue;

        ad->materials[i].edge_group_idx=(int)res.groups.size();

        res.groups.resize(res.groups.size()+1);
        nya_scene::shared_mesh::group &ge=res.groups.back();
        ge=res.groups[i];
        ge.name="edge";
        ge.material_idx=int(res.materials.size());
        res.materials.resize(res.materials.size()+1);
        nya_scene::material &me=res.materials.back();

        nya_scene::material::pass &pe=me.get_pass(me.add_pass(nya_scene::material::default_pass));
        nya_scene::shader she;
        she.load("pmx_edge.nsh");
        pe.set_shader(she);
        pe.get_state().set_blend(true,nya_render::blend::src_alpha,nya_render::blend::inv_src_alpha);
        pe.get_state().set_cull_face(true,nya_render::cull_face::ccw);
        me.set_param(me.get_param_idx("edge offset"),edge.width*0.03f,edge.width*0.03f,edge.width*0.03f,0.0f);
        me.set_param(me.get_param_idx("edge color"),edge.color);
    }

    typedef unsigned short ushort;

    const int bones_count=reader.read<int>();

    std::vector<pmx_bone> bones(bones_count);
    for(int i=0;i<bones_count;++i)
    {
        pmx_bone &b=bones[i];

        b.name=read_string(reader,header.text_encoding);
        skip_string(reader); //en name

        read_vector(b.pos,reader);
        b.idx=i;

        b.parent=read_idx(reader,header.bone_idx_size);
        b.order=reader.read<int>();

        const flag<uint16_t> f(reader.read<uint16_t>()); //ToDo
        if(f.c(0x0001))
            read_idx(reader,header.bone_idx_size);
        else
            reader.skip(sizeof(float)*3);

        b.bound.has_rot=f.c(0x0100);
        b.bound.has_pos=f.c(0x0200);
        if(b.bound.has_rot || b.bound.has_pos)
        {
            b.bound.src_idx=read_idx(reader,header.bone_idx_size);
            b.bound.k=reader.read<float>();
        }

        if(f.c(0x0400))
            reader.skip(sizeof(float)*3); //ToDo: fixed axis

        if(f.c(0x0800))
            reader.skip(sizeof(float)*3*2); //ToDo

        if(f.c(0x2000))
            reader.read<int>(); //ToDo

        b.ik.has=f.c(0x0020);
        if(b.ik.has)
        {
            b.ik.eff_idx=read_idx(reader,header.bone_idx_size);
            b.ik.count=reader.read<int>();
            b.ik.k=reader.read<float>();

            const int link_count=reader.read<int>();
            b.ik.links.resize(link_count);
            for(int j=0;j<link_count;++j)
            {
                pmx_bone::ik_link &l=b.ik.links[j];
                l.idx=read_idx(reader,header.bone_idx_size);
                l.has_limits=reader.read<int8_t>()>0;
                if(l.has_limits)
                {
                    read_angle(l.from,reader);
                    read_angle(l.to,reader);
                    std::swap(l.from.xy(),l.to.xy());
                }
            }
        }
    }

    for(int i=0;i<int(bones.size());++i)
    {
        bool had_sorted=false;
        for(int j=0;j<int(bones.size());++j)
        {
            const int p=bones[j].parent;
            if(p<=j)
                continue;

            had_sorted=true;
            std::swap(bones[j],bones[p]);
            for(int k=0;k<(int)bones.size();++k)
            {
                if(bones[k].parent==j)
                    bones[k].parent=p;
                else if(bones[k].parent==p)
                    bones[k].parent=j;
            }
        }

        if(!had_sorted)
            break;
    }

    std::vector<int> old_bones(bones_count);
    for(int i=0;i<bones_count;++i)
        old_bones[bones[i].idx]=i;

    for(int i=0;i<bones_count;++i)
    {
        const pmx_bone &b=bones[i];

        if((b.bound.has_pos || b.bound.has_rot) && b.bound.src_idx>=0 && b.bound.src_idx<bones_count)
            res.skeleton.add_bound(old_bones[b.bound.src_idx],i,b.bound.k,b.bound.has_pos,b.bound.has_rot,true);

        if(res.skeleton.add_bone(b.name.c_str(),b.pos,nya_math::quat(),b.parent,true)!=i)
        {
            nya_log::log()<<"pmx load error: invalid bone\n";
            return false;
        }

        if(b.ik.has)
        {
            const int ik=res.skeleton.add_ik(i,old_bones[b.ik.eff_idx],b.ik.count,b.ik.k,true);
            for(int j=0;j<int(b.ik.links.size());++j)
            {
                const pmx_bone::ik_link &l=b.ik.links[j];
                if(l.has_limits)
                    res.skeleton.add_ik_link(ik,old_bones[l.idx],l.from,l.to,true);
                else
                    res.skeleton.add_ik_link(ik,old_bones[l.idx],true);
            }
        }
    }

    if(bones_count==0)
        res.skeleton.add_bone("Dummy",nya_math::vec3(),nya_math::quat(),-1,true);

    const int morphs_count=reader.read<int>();
    ad->morphs.resize(morphs_count);

    for(int i=0;i<morphs_count;++i)
    {
        additional_data::morph &m=ad->morphs[i];

        m.name=read_string(reader,header.text_encoding);
        skip_string(reader); //en name

        m.kind=pmd_morph_data::morph_kind(reader.read<int8_t>());

        m.type=morph_type(reader.read<int8_t>());
        switch(m.type)
        {
            case morph_type_group:
            {
                m.idx=(int)ad->group_morphs.size();
                ad->group_morphs.resize(ad->group_morphs.size()+1);
                additional_data::group_morph &m=ad->group_morphs.back();

                const int size=reader.read<int>();
                m.morphs.resize(size);
                for(int j=0;j<size;++j)
                {
                    m.morphs[j].first=read_idx(reader,header.morph_idx_size);
                    m.morphs[j].second=reader.read<float>();
                }
            }
            break;

            case morph_type_vertex:
            {
                m.idx=(int)ad->vert_morphs.size();
                ad->vert_morphs.resize(ad->vert_morphs.size()+1);
                additional_data::vert_morph &m=ad->vert_morphs.back();

                const int size=reader.read<int>();
                m.verts.resize(size);

                switch(header.index_size)
                {
                    case 1: load_vertex_morph<uint8_t>(reader,m); break;
                    case 2: load_vertex_morph<uint16_t>(reader,m); break;
                    case 4: load_vertex_morph<uint32_t>(reader,m); break;
                }
            }
            break;

            case morph_type_bone:
            {
                m.idx=(int)ad->bone_morphs.size();
                ad->bone_morphs.resize(ad->bone_morphs.size()+1);
                additional_data::bone_morph &m=ad->bone_morphs.back();

                const int size=reader.read<int>();
                m.bones.resize(size);

                for(int j=0;j<size;++j)
                {
                    additional_data::morph_bone &b=m.bones[j];
                    b.idx=read_idx(reader,header.bone_idx_size);
                    read_vector(b.pos,reader);

                    b.rot.v.x=-reader.read<float>();
                    b.rot.v.y=-reader.read<float>();
                    b.rot.v.z=reader.read<float>();
                    b.rot.w=reader.read<float>();
                }
            }
            break;

            case morph_type_uv:
            case morph_type_add_uv1:
            case morph_type_add_uv2:
            case morph_type_add_uv3:
            case morph_type_add_uv4:
            {
                m.idx=(int)ad->uv_morphs.size();
                ad->uv_morphs.resize(ad->uv_morphs.size()+1);
                additional_data::uv_morph &m=ad->uv_morphs.back();

                const int size=reader.read<int>();
                m.verts.resize(size);

                switch(header.index_size)
                {
                    case 1: load_uv_morph<uint8_t>(reader,m); break;
                    case 2: load_uv_morph<uint16_t>(reader,m); break;
                    case 4: load_uv_morph<uint32_t>(reader,m); break;
                }
            }
            break;

            case morph_type_material:
            {
                m.idx=(int)ad->mat_morphs.size();
                ad->mat_morphs.resize(ad->mat_morphs.size()+1);
                additional_data::mat_morph &m=ad->mat_morphs.back();

                const int size=reader.read<int>();

                for(int j=0;j<size;++j)
                {
                    additional_data::morph_mat mm;
                    mm.mat_idx=read_idx(reader,header.material_idx_size);
                    mm.op=additional_data::morph_mat::op_type(reader.read<int8_t>());
                    mm.params=reader.read<pmx_material_params>();
                    mm.edge_params=reader.read<pmx_edge_params>();
                    mm.tex_color=reader.read<nya_math::vec4>();
                    mm.sp_tex_color=reader.read<nya_math::vec4>();
                    mm.toon_tex_color=reader.read<nya_math::vec4>();

                    if(mm.mat_idx>=0)
                        m.mats.push_back(mm);
                }
            }
            break;

            default:
                nya_log::log()<<"pmx load error: invalid morph type\n";
                return false;
        }
    }

    const int bone_limit=244; //ToDo: get from shader
    std::map<int,int> bone_remap;
    for(int i=0;i<(int)verts.size();++i)
    {
        const vert &v=verts[i];
        for(int k=0;k<4;++k)
        {
            if(v.bone_weight[k]>0.0f)
                bone_remap[int(v.bone_idx[k])]=-1;
        }
    }

    if(bone_remap.size()<=bone_limit)
    {
        ad->bone_remaps.resize(1);
        additional_data::bone_remap &br=ad->bone_remaps.back();
        br.goffset=0;
        br.gcount=mat_count;
        br.bones.resize(bone_remap.size());
        int idx = 0;
        for(auto &r: bone_remap)
        {
            r.second=idx++;
            br.bones[r.second]=r.first;
        }

        for(int i=0;i<(int)verts.size();++i)
        {
            vert &v=verts[i];
            for(int k=0;k<4;++k)
            {
                if(v.bone_weight[k]>0.0f)
                    v.bone_idx[k]=bone_remap[int(v.bone_idx[k])];
            }
        }
    }
    else
    {
        return false;

        //ToDo: divide model in parts

        /*
        unsigned char *inds1b=(unsigned char *)indices;
        unsigned short *inds2b=(unsigned short *)indices;
        unsigned int *inds4b=(unsigned int *)indices;

        for(int j=res.groups[i].offset;j<res.groups[i].offset+res.groups[i].count;++j)
        {
            const vert &v=header.index_size==2?verts[inds2b[j]]:(header.index_size==4?verts[inds4b[j]]:verts[inds1b[j]]);
            for(int k=0;k<4;++k)
            {
                if(v.bone_weight[k]>0.0f)
                    bone_remap[int(v.bone_idx[k])]=-1;
            }
        }
        */
    }

    std::map<int,int> vmorph_remap;
    for(int i=0;i<(int)ad->vert_morphs.size();++i)
    {
        for(int j=0;j<(int)ad->vert_morphs[i].verts.size();++j)
        {
            const pmd_morph_data::morph_vertex &m=ad->vert_morphs[i].verts[j];
            if(vmorph_remap.find(m.idx)==vmorph_remap.end())
                vmorph_remap[m.idx]=(int)vmorph_remap.size()+1;
        }
    }

    ad->mvert_count=(int)vmorph_remap.size()+1;
    int morphs_tex_w=512;
    
    int morphs_tex_len=int(ad->vert_morphs.size()*ad->mvert_count);
    while(morphs_tex_len > morphs_tex_w * morphs_tex_w)
        morphs_tex_w *= 2;
    int morphs_tex_h=int((morphs_tex_len+morphs_tex_w-1)/morphs_tex_w); //ceil division
    std::vector<nya_math::vec3> vert_morphs_data(morphs_tex_w*morphs_tex_h);
    ad->vert_tex_w=morphs_tex_w;
    ad->vert_tex_h=morphs_tex_h;

    for(int i=0;i<(int)ad->vert_morphs.size();++i)
    {
        nya_math::vec3 *mv=&vert_morphs_data[i*ad->mvert_count];
        for(int j=0;j<(int)ad->vert_morphs[i].verts.size();++j)
        {
            const pmd_morph_data::morph_vertex &m=ad->vert_morphs[i].verts[j];
         
            if(vmorph_remap[m.idx] > vert_morphs_data.size() - i*ad->mvert_count)
                printf("FUCK");
            
            mv[vmorph_remap[m.idx]]=m.pos;
        }
    }

    for(int i=0;i<(int)verts.size();++i)
    {
        std::map<int,int>::iterator it=vmorph_remap.find(i);
        verts[i].morph_idx=it==vmorph_remap.end()?0.0f:float(it->second);
    }

    nya_scene::texture morphs_tex;
    morphs_tex.build(vert_morphs_data.data(),morphs_tex_w,morphs_tex_h, nya_render::texture::color_rgb32f);
    for(int i=0;i<(int)res.materials.size();++i)
    {
        res.materials[i].set_texture("morph",morphs_tex);
        res.materials[i].set_param(res.materials[i].get_param_idx("morphs count"),0.0f,float(morphs_tex_w),float(morphs_tex_h));
    }

    //ToDo: uv morphs texture

    const int frames_count=reader.read<int>();
    for(int i=0;i<frames_count;++i)
    {
        skip_string(reader); //name
        skip_string(reader); //en name

        reader.skip(1);//flag

        const int inner_count=reader.read<int>();
        for(int j=0;j<inner_count;++j)
        {
            char type=reader.read<int8_t>();
            if(type)
                read_idx(reader,header.morph_idx_size);
            else
                read_idx(reader,header.bone_idx_size);
        }
    }

    const int rigid_bodies_count=reader.read<int>();
    ad->rigid_bodies.resize(rigid_bodies_count);
    for(int i=0;i<rigid_bodies_count;++i)
    {
        pmd_phys_data::rigid_body &rb=ad->rigid_bodies[i];

        rb.name=read_string(reader,header.text_encoding);
        skip_string(reader); //en name

        rb.bone=read_idx(reader,header.bone_idx_size);
        if(rb.bone>=0)
            rb.bone=old_bones[rb.bone];

        rb.collision_group=reader.read<uint8_t>();
        rb.collision_mask=reader.read<uint16_t>();
        rb.type=(pmd_phys_data::shape_type)reader.read<uint8_t>();
        rb.size=reader.read<nya_math::vec3>();
        read_vector(rb.pos,reader);
        rb.pos-=res.skeleton.get_bone_original_pos(rb.bone);
        read_angle(rb.rot,reader);
        rb.mass=reader.read<float>();
        rb.vel_attenuation=reader.read<float>();
        rb.rot_attenuation=reader.read<float>();
        rb.restriction=reader.read<float>();
        rb.friction=reader.read<float>();
        rb.mode=(pmd_phys_data::object_type)reader.read<uint8_t>();
    }

    const int joints_count=reader.read<int>();
    ad->joints.resize(joints_count);
    for(int i=0;i<joints_count;++i)
    {
        pmd_phys_data::joint &j=ad->joints[i];
        j.name=read_string(reader,header.text_encoding);
        skip_string(reader); //en name

        char type=reader.read<int8_t>();
        if(type>0)
        {
            nya_log::log()<<"pmx load warning: unsupported phys joint type\n";
            ad->joints.clear();
            ad->rigid_bodies.clear();
            break;
        }

        j.rigid_src=read_idx(reader,header.rigidbody_idx_size);
        j.rigid_dst=read_idx(reader,header.rigidbody_idx_size);
        read_vector(j.pos,reader);
        read_angle(j.rot,reader);

        read_vector(j.pos_min,reader);
        read_vector(j.pos_max,reader);
        std::swap(j.pos_max.z,j.pos_min.z);

        read_angle(j.rot_min,reader);
        read_angle(j.rot_max,reader);
        std::swap(j.rot_max.xy(),j.rot_min.xy());

        j.pos_spring=reader.read<nya_math::vec3>();
        read_angle(j.rot_spring,reader);
    }

    res.vbo.set_vertex_data(&verts[0],sizeof(vert),vert_count);
    int offset=0;
    res.vbo.set_vertices(offset,4); offset+=sizeof(verts[0].pos); offset+=sizeof(float);
    res.vbo.set_normals(offset); offset+=sizeof(verts[0].normal);
    res.vbo.set_tc(0,offset,3); offset+=sizeof(verts[0].tc);
    res.vbo.set_tc(1,offset,4); offset+=sizeof(verts[0].bone_idx);
    res.vbo.set_tc(2,offset,4); //offset+=sizeof(verts[0].bone_weight);
    
    return true;
}

const pmx_loader::additional_data *pmx_loader::get_additional_data(const nya_scene::mesh &m)
{
    if(!m.internal().get_shared_data().is_valid())
        return 0;

    nya_scene::shared_mesh::additional_data *d=m.internal().get_shared_data()->add_data;
    if(!d)
        return 0;

    const char *type=d->type();
    if(!type || strcmp(type,"pmx")!=0)
        return 0;

    return static_cast<pmx_loader::additional_data*>(static_cast<add_data*>(d));
}
