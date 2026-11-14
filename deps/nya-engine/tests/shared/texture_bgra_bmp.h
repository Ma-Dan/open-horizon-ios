//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "scene/texture.h"
#include "memory/memory_reader.h"

//loads only bmp with alpha because almost nobody support alpha in bmp

namespace nya_scene
{
    static bool load_texture_bgra_bmp(shared_texture &res,resource_data &data,const char* name)
    {
        nya_memory::memory_reader reader(data.get_data(),data.get_size());
        if(!reader.test("BM",2))
            return false;

        reader.seek(28);
        if(reader.read<unsigned short>()!=32) //bpp
            return false;
        if(reader.read<unsigned short>()!=0) //compression
            return false;

        reader.seek(10);
        const unsigned int data_offset=reader.read<unsigned int>();
        reader.skip(4);
        int width=reader.read<int>();
        int height=reader.read<int>();
        reader.seek(data_offset);
        if(height<0)
        {
            height= -height;
            unsigned int *data=(unsigned int*)reader.get_data();
            const int top=width*(height-1);
            for(int offset=0;offset<width*height/2;offset+=width)
            {
                unsigned int *a=data+offset,*b=data+top-offset;
                for(int w=0;w<width;++w,++a,++b)
                    std::swap(*a,*b);
            }
        }
        else if(!height)
            return false;

        res.tex.build_texture(reader.get_data(),width,height,nya_render::texture::color_bgra);
        return true;
    }
}
