//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

//requires: libdevil http://openil.sourceforge.net/

#include "texture_il.h"
#include "scene/scene.h"

#include "IL/il.h"

namespace nya_scene
{

bool load_texture_il(nya_scene::shared_texture &res,nya_scene::resource_data &data,const char* name)
{
    static bool once=true;
    if(once)
        ilInit(),once=false;
    ilEnable(IL_ORIGIN_SET);
	ilOriginFunc(IL_ORIGIN_LOWER_LEFT);

    const ILuint img=ilGenImage();
    ilBindImage(img);

    if(!ilLoadL(IL_TYPE_UNKNOWN,data.get_data(),ILuint(data.get_size())))
    {
        ilDeleteImage(img);
        return false;
    }

    const ILint width=ilGetInteger(IL_IMAGE_WIDTH);
    const ILint height=ilGetInteger(IL_IMAGE_HEIGHT);
    ILint bpp=ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL);
    ILint img_format=ilGetInteger(IL_IMAGE_FORMAT);

    if(img_format==IL_COLOR_INDEX)
    {
        bpp=ilGetInteger(IL_PALETTE_BPP);
        if(bpp==4)
            img_format=IL_RGBA;
        else if(bpp==3)
            img_format=IL_RGB;
        else
        {
            nya_log::log()<<"load texture il error: incorrect pallete format: "<<img_format<<" bpp "<<bpp<<"\n";
            ilDeleteImage(img);
            return false;
        }

        ilConvertImage(img_format,IL_UNSIGNED_BYTE);
    }

    nya_render::texture::color_format format;
    switch(img_format)
    {
    case IL_LUMINANCE: format=nya_render::texture::greyscale; break;

    case IL_RGB: format=nya_render::texture::color_rgb;
            if(bpp>3)
                ilConvertImage(IL_RGB,IL_UNSIGNED_BYTE);
            break;

    case IL_BGR: format=nya_render::texture::color_rgb;
            ilConvertImage(IL_RGB,IL_UNSIGNED_BYTE);
            break;

    case IL_RGBA: format=nya_render::texture::color_rgba;
            if(bpp>4)
                ilConvertImage(IL_RGBA,IL_UNSIGNED_BYTE);
            break;

    case IL_BGRA: format=nya_render::texture::color_bgra;
            if(bpp>4)
                ilConvertImage(IL_BGRA,IL_UNSIGNED_BYTE);
            break;

    default:
        nya_log::log()<<"load texture il error: incorrect format: "<<img_format<<" bpp "<<bpp<<"\n";
        ilDeleteImage(img);
        return false;
    }
    res.tex.build_texture(ilGetData(),width,height,format);
    ilDeleteImage(img);
    texture::read_meta(res,data);
    return true;
}

}
