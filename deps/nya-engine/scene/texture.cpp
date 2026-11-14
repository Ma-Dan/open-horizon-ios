//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "texture.h"
#include "scene.h"
#include "memory/memory_reader.h"
#include "memory/tmp_buffer.h"
#include "formats/tga.h"
#include "formats/dds.h"
#include "formats/ktx.h"
#include "formats/meta.h"
#include "render/fbo.h"
#include "render/shader.h"
#include "render/screen_quad.h"
#include "render/bitmap.h"
#include <stdlib.h>
#include <string.h>

//OH_ENABLE_ASTC_CPU_DECODE: decode ASTC textures to RGBA on the CPU when the
//render API has no ASTC hardware support (macOS GL validation builds; iOS uses
//hardware ASTC and never needs this). Links the vendored astcenc static lib.
#ifdef OH_ENABLE_ASTC_CPU_DECODE
    #include "astcenc.h"
#endif

namespace
{

#ifdef OH_ENABLE_ASTC_CPU_DECODE

//decode a full ASTC mip chain (as stored in a DDS payload) to RGBA, one mip at a time
bool decode_astc_chain(const unsigned char *src, size_t src_size, unsigned char *dst,
                       unsigned int w, unsigned int h, unsigned int mips, int bw, int bh)
{
    astcenc_config cfg;
    if(astcenc_config_init(ASTCENC_PRF_LDR_SRGB, (unsigned int)bw, (unsigned int)bh, 1,
                           0.0f, ASTCENC_FLG_DECOMPRESS_ONLY, &cfg) != ASTCENC_SUCCESS)
        return false;

    astcenc_context *ctx = 0;
    if(astcenc_context_alloc(&cfg, 1, &ctx, 0) != ASTCENC_SUCCESS)
        return false;

    const astcenc_swizzle swz = { ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A };

    bool ok = true;
    for(unsigned int i = 0; i < mips && ok; ++i)
    {
        unsigned int cw = w >> i, ch = h >> i;
        if(cw < 1) cw = 1;
        if(ch < 1) ch = 1;

        const size_t blocks = ((cw + bw - 1) / bw) * ((ch + bh - 1) / bh);
        const size_t mip_size = blocks * 16;
        if(mip_size > src_size)
        {
            //some NTXR mip chains stop early (below one block); keep the mips
            //we did decode instead of failing the whole texture
            fprintf(stderr,"OH_ASTC mip %u/%u size mismatch: need %zu have %zu (%ux%u blk %dx%d) - truncating\n",
                    i, mips, mip_size, src_size, cw, ch, bw, bh);
            ok = true;
            break;
        }

        unsigned char *out = dst;
        for(unsigned int m = 0; m < i; ++m)
        {
            unsigned int mw = w >> m, mh = h >> m;
            if(mw < 1) mw = 1;
            if(mh < 1) mh = 1;
            out += (size_t)mw * mh * 4;
        }

        astcenc_image img;
        memset(&img, 0, sizeof(img));
        img.dim_x = cw;
        img.dim_y = ch;
        img.dim_z = 1;
        img.data_type = ASTCENC_TYPE_U8;
        uint8_t *plane = out;
        img.data = (void **)&plane;

        astcenc_error err = astcenc_decompress_image(ctx, src, mip_size, &img, &swz, 0);
        if(err != ASTCENC_SUCCESS)
        {
            fprintf(stderr,"OH_ASTC decompress_image failed at mip %u: %d (%ux%u blk %dx%d)\n",
                    i, (int)err, cw, ch, bw, bh);
            ok = false;
        }

        src += mip_size;
        src_size -= mip_size;
    }

    astcenc_context_free(ctx);
    return ok;
}

#endif

}

namespace nya_scene
{

int texture::m_load_ktx_mip_offset=0;

bool texture::load_ktx(shared_texture &res,resource_data &data,const char* name)
{
    if(!data.get_size())
        return false;

    if(data.get_size()<12)
        return false;

    if(memcmp((const char *)data.get_data()+1,"KTX ",4)!=0)
        return false;

    nya_formats::ktx ktx;
    const size_t header_size=ktx.decode_header(data.get_data(),data.get_size());
    if(!header_size)
    {
        log()<<"unable to load ktx: invalid or unsupported ktx header in file "<<name<<"\n";
        return false;
    }

    nya_render::texture::color_format cf;

    switch(ktx.pf)
    {
        case nya_formats::ktx::rgb: cf=nya_render::texture::color_rgb; break;
        case nya_formats::ktx::rgba: cf=nya_render::texture::color_rgba; break;
        case nya_formats::ktx::bgra: cf=nya_render::texture::color_bgra; break;

        case nya_formats::ktx::etc1: cf=nya_render::texture::etc1; break;
        case nya_formats::ktx::etc2: cf=nya_render::texture::etc2; break;
        case nya_formats::ktx::etc2_eac: cf=nya_render::texture::etc2_eac; break;
        case nya_formats::ktx::etc2_a1: cf=nya_render::texture::etc2_a1; break;

        case nya_formats::ktx::pvr_rgb2b: cf=nya_render::texture::pvr_rgb2b; break;
        case nya_formats::ktx::pvr_rgb4b: cf=nya_render::texture::pvr_rgb4b; break;
        case nya_formats::ktx::pvr_rgba2b: cf=nya_render::texture::pvr_rgba2b; break;
        case nya_formats::ktx::pvr_rgba4b: cf=nya_render::texture::pvr_rgba4b; break;

        default: log()<<"unable to load ktx: unsupported color format in file "<<name<<"\n"; return false;
    }

    const int mip_off=m_load_ktx_mip_offset>=int(ktx.mipmap_count)?0:m_load_ktx_mip_offset;
    char *d=(char *)ktx.data;
    nya_memory::memory_reader r(ktx.data,ktx.data_size);
    for(unsigned int i=0;i<ktx.mipmap_count;++i)
    {
        const unsigned int size=r.read<unsigned int>();
        if(int(i)>=mip_off)
        {
            if(r.get_remained()<size)
            {
                log()<<"unable to load ktx: invalid texture mipmap size in file "<<name<<"\n";
                return false;
            }

            memmove(d,r.get_data(),size);
            d+=size;
        }
        r.skip(size);
    }

    const int width=ktx.width>>mip_off;
    const int height=ktx.height>>mip_off;
    read_meta(res,data);
    return res.tex.build_texture(ktx.data,width>0?width:1,height>0?height:1,cf,ktx.mipmap_count-mip_off);
}

bool texture::m_load_dds_flip=false;
int texture::m_load_dds_mip_offset=0;

bool texture::load_dds(shared_texture &res,resource_data &data,const char* name)
{
    if(!data.get_size())
        return false;

    if(data.get_size()<4)
        return false;

    if(memcmp(data.get_data(),"DDS ",4)!=0)
        return false;

    nya_formats::dds dds;
    const size_t header_size=dds.decode_header(data.get_data(),data.get_size());
    if(!header_size)
    {
        log()<<"unable to load dds: invalid or unsupported dds header in file "<<name<<"\n";
        return false;
    }

    if(dds.pf!=nya_formats::dds::palette8_rgba && dds.pf!=nya_formats::dds::palette4_rgba) //ToDo
    {
        for(int i=0;i<m_load_dds_mip_offset && dds.mipmap_count > 1;++i)
        {
            dds.data=(char *)dds.data+dds.get_mip_size(0);
            if(dds.width>1)
                dds.width/=2;
            if(dds.height>1)
                dds.height/=2;
            --dds.mipmap_count;
        }
    }

    nya_memory::tmp_buffer_ref tmp_buf;

    int mipmap_count=dds.need_generate_mipmaps?-1:dds.mipmap_count;
    nya_render::texture::color_format cf;
    switch(dds.pf)
    {
        case nya_formats::dds::dxt1: cf=nya_render::texture::dxt1; break;
        case nya_formats::dds::dxt2:
        case nya_formats::dds::dxt3: cf=nya_render::texture::dxt3; break;
        case nya_formats::dds::dxt4:
        case nya_formats::dds::dxt5: cf=nya_render::texture::dxt5; break;

        case nya_formats::dds::astc4x4: cf=nya_render::texture::astc4x4; break;
        case nya_formats::dds::astc5x5: cf=nya_render::texture::astc5x5; break;
        case nya_formats::dds::astc6x6: cf=nya_render::texture::astc6x6; break;

        case nya_formats::dds::bgra: cf=nya_render::texture::color_bgra; break;
        case nya_formats::dds::rgba: cf=nya_render::texture::color_rgba; break;
        case nya_formats::dds::rgb: cf=nya_render::texture::color_rgb; break;
        case nya_formats::dds::greyscale: cf=nya_render::texture::greyscale; break;

        case nya_formats::dds::bgr:
        {
            nya_render::bitmap_rgb_to_bgr((unsigned char*)dds.data,dds.width,dds.height,3);
            cf=nya_render::texture::color_rgb;
        }
        break;

        case nya_formats::dds::palette8_rgba:
        {
            if(dds.mipmap_count!=1 || dds.type!=nya_formats::dds::texture_2d) //ToDo
            {
                log()<<"unable to load dds: uncomplete palette8_rgba support, unable to load file "<<name<<"\n";
                return false;
            }

            cf=nya_render::texture::color_rgba;
            dds.data_size=dds.width*dds.height*4;
            tmp_buf.allocate(dds.data_size);
            dds.decode_palette8_rgba(tmp_buf.get_data());
            dds.data=tmp_buf.get_data();
            dds.pf=nya_formats::dds::bgra;
        }
        break;

        default: log()<<"unable to load dds: unsupported color format in file "<<name<<"\n"; return false;
    }

    bool result=false;

    const bool decode_dxt=cf>=nya_render::texture::dxt1 && cf<=nya_render::texture::dxt5 &&
                          (!nya_render::texture::is_dxt_supported() || dds.height%2>0);

    //ASTC without hardware support (macOS GL): CPU-decode the mip chain to RGBA.
    //On iOS the CPU decoder is always built in (OH_ENABLE_ASTC_CPU_DECODE);
    //some Apple GLES drivers accept ASTC uploads but render them with
    //scrambled blocks (visible as random triangles on aircraft/cloud textures
    //while terrain renders fine), so decode on CPU there too for correctness.
    const bool decode_astc=cf>=nya_render::texture::astc4x4 && cf<=nya_render::texture::astc6x6 &&
                           (!nya_render::texture::is_astc_supported()
#ifdef OH_PLATFORM_IOS
                            || true
#endif
                           );

    switch(dds.type)
    {
        case nya_formats::dds::texture_2d:
        {
            if(decode_dxt)
            {
                tmp_buf.allocate(dds.get_decoded_size());
                dds.decode_dxt(tmp_buf.get_data());
                dds.data_size=tmp_buf.get_size();
                dds.data=tmp_buf.get_data();
                cf=nya_render::texture::color_rgba;
                dds.pf=nya_formats::dds::bgra;
                if(mipmap_count>1)
                    mipmap_count= -1;
            }
            else if(decode_astc)
            {
#ifdef OH_ENABLE_ASTC_CPU_DECODE
                const int bw=(dds.pf==nya_formats::dds::astc6x6)?6:(dds.pf==nya_formats::dds::astc5x5?5:4);
                tmp_buf.allocate(dds.get_decoded_size());
                if(!decode_astc_chain((const unsigned char *)dds.data,dds.data_size,
                                      (unsigned char *)tmp_buf.get_data(),
                                      dds.width,dds.height,dds.mipmap_count,bw,bw))
                {
                    log()<<"unable to load dds: astc decode failed in file "<<name<<"\n";
                    return false;
                }

                dds.data_size=tmp_buf.get_size();
                dds.data=tmp_buf.get_data();
                cf=nya_render::texture::color_rgba;
                dds.pf=nya_formats::dds::bgra;
                if(mipmap_count>1)
                    mipmap_count= -1;
#else
                log()<<"unable to load dds: astc not supported and cpu decoder not built in, file "<<name<<"\n";
                return false;
#endif
            }

            if(m_load_dds_flip)
            {
                nya_memory::tmp_buffer_scoped tmp_data(dds.data_size);
                dds.flip_vertical(dds.data,tmp_data.get_data());
                result=res.tex.build_texture(tmp_data.get_data(),dds.width,dds.height,cf,mipmap_count);
            }
            else
                result=res.tex.build_texture(dds.data,dds.width,dds.height,cf,mipmap_count);
        }
        break;

        case nya_formats::dds::texture_cube:
        {
            if(decode_dxt)
            {
                tmp_buf.allocate(dds.get_decoded_size());
                dds.decode_dxt(tmp_buf.get_data());
                dds.data_size=tmp_buf.get_size();
                dds.data=tmp_buf.get_data();
                cf=nya_render::texture::color_rgba;
                dds.pf=nya_formats::dds::bgra;
                if(mipmap_count>1)
                    mipmap_count= -1;
            }

            const void *data[6];
            for(int i=0;i<6;++i)
                data[i]=(const char *)dds.data+i*dds.data_size/6;
            result=res.tex.build_cubemap(data,dds.width,dds.height,cf,mipmap_count);
        }
        break;

        default:
        {
            log()<<"unable to load dds: unsupported texture type in file "<<name<<"\n";
            tmp_buf.free();
            return false;
        }
    }

    tmp_buf.free();
    read_meta(res,data);
    return result;
}

bool texture::load_tga(shared_texture &res,resource_data &data,const char* name)
{
    if(!data.get_size())
        return false;

    nya_formats::tga tga;
    const size_t header_size=tga.decode_header(data.get_data(),data.get_size());
    if(!header_size)
        return false;

    nya_render::texture::color_format color_format;
    switch(tga.channels)
    {
        case 4: color_format=nya_render::texture::color_bgra; break;
        case 3: color_format=nya_render::texture::color_rgb; break;
        case 1: color_format=nya_render::texture::greyscale; break;
        default: log()<<"unable to load tga: unsupported color format in file "<<name<<"\n"; return false;
    }

    typedef unsigned char uchar;

    nya_memory::tmp_buffer_ref tmp_data;
    const void *color_data=tga.data;
    if(tga.rle)
    {
        tmp_data.allocate(tga.uncompressed_size);
        if(!tga.decode_rle(tmp_data.get_data()))
        {
            tmp_data.free();
            log()<<"unable to load tga: unable to decode rle in file "<<name<<"\n";
            return false;
        }

        color_data=tmp_data.get_data();
    }
    else if(header_size+tga.uncompressed_size>data.get_size())
    {
        log()<<"unable to load tga: lack of data, probably corrupted file "<<name<<"\n";
        return false;
    }

    if(tga.channels==3 || tga.horisontal_flip || tga.vertical_flip)
    {
        if(!tmp_data.get_data())
        {
            tmp_data.allocate(tga.uncompressed_size);

            if(tga.horisontal_flip || tga.vertical_flip)
            {
                if(tga.horisontal_flip)
                    tga.flip_horisontal(color_data,tmp_data.get_data());

                if(tga.vertical_flip)
                    tga.flip_vertical(color_data,tmp_data.get_data());
            }
            else
                tmp_data.copy_from(color_data,tga.uncompressed_size);

            color_data=tmp_data.get_data();
        }
        else
        {
            if(tga.horisontal_flip)
                tga.flip_horisontal(tmp_data.get_data(),tmp_data.get_data());

            if(tga.vertical_flip)
                tga.flip_vertical(tmp_data.get_data(),tmp_data.get_data());
        }

        if(tga.channels==3)
            nya_render::bitmap_rgb_to_bgr((unsigned char*)color_data,tga.width,tga.height,3);
    }

    const bool result=res.tex.build_texture(color_data,tga.width,tga.height,color_format);
    tmp_data.free();
    read_meta(res,data);
    return result;
}

inline nya_render::texture::wrap get_wrap(const std::string &s)
{
    if(s=="repeat")
        return nya_render::texture::wrap_repeat;
    if(s=="mirror")
        return nya_render::texture::wrap_repeat_mirror;

    return nya_render::texture::wrap_clamp;
}

bool texture::read_meta(shared_texture &res,resource_data &data)
{
    nya_formats::meta m;
    if(!m.read(data.get_data(),data.get_size()))
        return false;

    bool set_wrap=false;
    nya_render::texture::wrap s,t;
    s=t=nya_render::texture::wrap_repeat;

    for(int i=0;i<(int)m.values.size();++i)
    {
        if(m.values[i].first=="nya_wrap_s")
            s=get_wrap(m.values[i].second),set_wrap=true;
        else if(m.values[i].first=="nya_wrap_t")
            t=get_wrap(m.values[i].second),set_wrap=true;
        else if(m.values[i].first=="nya_aniso")
        {
            const int aniso=atoi(m.values[i].second.c_str());
            res.tex.set_aniso(aniso>0?aniso:0);
        }
    }

    if(set_wrap)
        res.tex.set_wrap(s,t);

    return true;
}

bool texture_internal::set(int slot) const
{
    if(!m_shared.is_valid())
    {
        nya_render::texture::unbind(slot);
        return false;
    }

    m_last_slot=slot;
    m_shared->tex.bind(slot);

    return true;
}

void texture_internal::unset() const
{
    if(!m_shared.is_valid())
        return;

    m_shared->tex.unbind(m_last_slot);
}

unsigned int texture::get_width() const
{
    if( !internal().get_shared_data().is_valid() )
        return 0;

    return internal().get_shared_data()->tex.get_width();
}

unsigned int texture::get_height() const
{
    if(!internal().get_shared_data().is_valid())
        return 0;

    return internal().get_shared_data()->tex.get_height();
}

texture::color_format texture::get_format() const
{
    if(!internal().get_shared_data().is_valid())
        return nya_render::texture::color_rgba;

    return internal().get_shared_data()->tex.get_color_format();
}

nya_memory::tmp_buffer_ref texture::get_data() const
{
    nya_memory::tmp_buffer_ref result;
    if(internal().get_shared_data().is_valid())
        internal().get_shared_data()->tex.get_data(result);
    return result;
}

nya_memory::tmp_buffer_ref texture::get_data(int x,int y,int width,int height) const
{
    nya_memory::tmp_buffer_ref result;
    if(internal().get_shared_data().is_valid())
        internal().get_shared_data()->tex.get_data(result,x,y,width,height);
    return result;
}

bool texture::is_cubemap() const
{
    if(!internal().get_shared_data().is_valid())
        return false;

    return internal().get_shared_data()->tex.is_cubemap();
}

bool texture::build(const void *data,unsigned int width,unsigned int height,color_format format)
{
    texture_internal::shared_resources::shared_resource_mutable_ref ref;
    if(m_internal.m_shared.get_ref_count()==1 && !m_internal.m_shared.get_name())  //was created and unique
    {
        ref=texture_internal::shared_resources::modify(m_internal.m_shared);
        return ref->tex.build_texture(data,width,height,format);
    }

    m_internal.unload();

    ref=m_internal.get_shared_resources().create();
    if(!ref.is_valid())
        return false;

    m_internal.m_shared=ref;
    return ref->tex.build_texture(data,width,height,format);
}

bool texture::crop(uint x,uint y,uint width,uint height)
{
    if(!width || !height)
    {
        unload();
        return true;
    }

    if(x==0 && y==0 && width==get_width() && height==get_height())
        return true;

    if(x+width>get_width() || y+height>get_height())
        return false;

    if(!m_internal.m_shared.is_valid())
        return false;

    nya_render::texture new_tex;
    if(!new_tex.build_texture(0,width,height,get_format()))
        return false;

    if(!new_tex.copy_region(m_internal.m_shared->tex,x,y,width,height,0,0))
    {
        new_tex.release();

        int channels;
        switch(get_format())
        {
            case nya_render::texture::color_rgba:
            case nya_render::texture::color_bgra: channels=4; break;
            case nya_render::texture::color_rgb: channels=3; break;
            case nya_render::texture::greyscale: channels=1; break;
            default: return false;
        }

        nya_memory::tmp_buffer_scoped buf(get_data());
        nya_render::bitmap_crop((unsigned char *)buf.get_data(),get_width(),get_height(),x,y,width,height,channels);
        return build(buf.get_data(),width,height,get_format());
    }

    texture_internal::shared_resources::shared_resource_mutable_ref ref;
    if(m_internal.m_shared.get_ref_count()==1 && !m_internal.m_shared.get_name())  //was created and unique
    {
        ref=texture_internal::shared_resources::modify(m_internal.m_shared);
        ref->tex.release();
        ref->tex=new_tex;
        return true;
    }

    m_internal.unload();

    ref=m_internal.get_shared_resources().create();
    if(!ref.is_valid())
    {
        new_tex.release();
        return false;
    }

    ref->tex=new_tex;
    m_internal.m_shared=ref;

    return true;
}

inline void grey_to_color(const unsigned char *data_from,unsigned char *data_to,size_t src_size,int channels)
{
    const unsigned char *src=data_from;
    unsigned char *dst=data_to;

    for(size_t i=0;i<src_size;++i, ++src)
    {
        for(int j=0;j<channels;++j, ++dst)
            *dst=*src;
    }
}

bool texture::update_region(const void *data,uint x,uint y,uint width,uint height,color_format format,int mip)
{
    if(!data)
        return false;

    if(get_format()==format)
        return update_region(data,x,y,width,height,mip);

    switch(get_format())
    {
        case nya_render::texture::color_bgra:
        case nya_render::texture::color_rgba:
        {
            nya_memory::tmp_buffer_scoped buf(width*height*4);

            if(format==nya_render::texture::color_bgra || format==nya_render::texture::color_rgba)
            {
                buf.copy_from(data,buf.get_size());
                nya_render::bitmap_rgb_to_bgr((unsigned char *)buf.get_data(),width,height,4);
            }
            else if(format==nya_render::texture::color_rgb)
            {
                nya_render::bitmap_rgb_to_rgba((const unsigned char *)data,width,height,255,(unsigned char *)buf.get_data());
                if(get_format()==nya_render::texture::color_bgra)
                    nya_render::bitmap_rgb_to_bgr((unsigned char *)buf.get_data(),width,height,4);
            }
            else if(format==nya_render::texture::greyscale)
                grey_to_color((const unsigned char *)data,(unsigned char *)buf.get_data(),width*height,4);
            else
                return false;

            return update_region(buf.get_data(),x,y,width,height,mip);
        }
        break;

        case nya_render::texture::color_rgb:
        {
            nya_memory::tmp_buffer_scoped buf(width*height*3);
            if(format==nya_render::texture::color_bgra || format==nya_render::texture::color_rgba)
            {
                nya_render::bitmap_rgba_to_rgb((const unsigned char *)data,width,height,(unsigned char *)buf.get_data());
                if(format==nya_render::texture::color_bgra)
                    nya_render::bitmap_rgb_to_bgr((unsigned char *)buf.get_data(),width,height,3);
            }
            else if(format==nya_render::texture::greyscale)
                grey_to_color((const unsigned char *)data,(unsigned char *)buf.get_data(),width*height,3);
            else
                return false;

            return update_region(buf.get_data(),x,y,width,height,mip);
        }
        break;

        //ToDo

        default: return false;
    }

    return false;
}

bool texture::update_region(const void *data,uint x,uint y,uint width,uint height,int mip)
{
    if(!width || !height)
        return x<=get_width() && y<=get_height();

    texture_internal::shared_resources::shared_resource_mutable_ref ref;
    if(m_internal.m_shared.get_ref_count()==1 && !m_internal.m_shared.get_name())  //was created and unique
    {
        ref=texture_internal::shared_resources::modify(m_internal.m_shared);
        return ref->tex.update_region(data,x,y,width,height);
    }

    if(!m_internal.m_shared.is_valid())
        return false;

    const uint w=m_internal.m_shared->tex.get_width();
    const uint h=m_internal.m_shared->tex.get_height();

    if(!x && !y && w==width && h==height && mip<0)
        return build(data,w,h,get_format());

    shared_texture new_stex;
    if(!new_stex.tex.build_texture(0,w,h,get_format()))
        return false;

    if(!new_stex.tex.copy_region(m_internal.m_shared->tex,0,0,w,h,0,0))
    {
        const nya_memory::tmp_buffer_scoped buf(get_data(0,0,w,h));
        if(!buf.get_size() || !new_stex.tex.build_texture(buf.get_data(),w,h,get_format()))
        {
            new_stex.tex.release();
            return false;
        }
    }

    create(new_stex);
    return update_region(data,x,y,width,height,mip);
}

bool texture::update_region(const texture_proxy &source,uint x,uint y)
{
    if(!source.is_valid())
        return false;

    return update_region(source,0,0,source->get_width(),source->get_height(),x,y);
}

bool texture::update_region(const texture &source,unsigned int x,unsigned int y)
{
    return update_region(texture_proxy(source),x,y);
}

bool texture::update_region(const texture_proxy &source,uint src_x,uint src_y,uint width,uint height,uint dst_x,uint dst_y)
{
    if(!source.is_valid() || !source->internal().get_shared_data().is_valid())
        return false;

    if(src_x+width>source->get_width() || src_y+height>source->get_height())
        return false;

    if(dst_x+width>get_width() || dst_y+height>get_height())
        return false;

    if(!internal().get_shared_data().is_valid())
        return false;

    if(m_internal.m_shared.get_ref_count()==1 && !m_internal.m_shared.get_name())  //was created and unique
    {
        nya_render::texture &tex=texture_internal::shared_resources::modify(m_internal.m_shared)->tex;
        if(tex.copy_region(source->internal().get_shared_data()->tex,src_x,src_y,width,height,dst_x,dst_y))
            return true;
    }

    const nya_memory::tmp_buffer_scoped buf(source->get_data(src_x,src_y,width,height));
    return update_region(buf.get_data(),dst_x,dst_y,width,height,source->get_format());
}

bool texture::update_region(const texture &source,uint src_x,uint src_y,uint width,uint height,uint dst_x,uint dst_y)
{
    return update_region(texture_proxy(source),src_x,src_y,width,height,dst_x,dst_y);
}

void texture::set_resources_prefix(const char *prefix)
{
    texture_internal::set_resources_prefix(prefix);
}

void texture::register_load_function(texture_internal::load_function function,bool clear_default)
{
    texture_internal::register_load_function(function,clear_default);
}

}
