//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "render.h"
#include "shader.h"
#include "texture.h"
#include "vbo.h"
#include <string.h>

namespace nya_render
{

class render_api_interface
{
public:
    virtual bool is_available() const { return true; }

    typedef unsigned int uint;

    struct viewport_state
    {
        rect viewport;
        rect scissor;
        bool scissor_enabled;
        float clear_color[4];
        float clear_depth;
        uint clear_stencil;
        int target;

        viewport_state(): scissor_enabled(false),target(-1) { for(int i=0;i<4;++i) clear_color[i]=0.0f; clear_depth=1.0f; clear_stencil=0; }
    };

    struct state: public nya_render::state,viewport_state
    {
        int vertex_buffer;
        int index_buffer;
        int index_offset;
        int index_count;
        int instances_count;
        vbo::element_type primitive;

        static const uint max_layers = 8;
        int textures[max_layers];

        int shader;
        int uniform_buffer;

        float modelview[4][4];
        float projection[4][4];

        inline void get_mvp(float (&mvp)[4][4]) const
        {
            for(int i=0;i<4;++i) for(int j=0;j<4;++j)
                mvp[i][j]=modelview[i][0]*projection[0][j] + modelview[i][1]*projection[1][j]
                         +modelview[i][2]*projection[2][j] + modelview[i][3]*projection[3][j];
        }

        state()
        {
            vertex_buffer=index_buffer=-1;
            index_offset=index_count=instances_count=0;
            primitive=vbo::triangles;
            shader=uniform_buffer=-1;
            for(int i=0;i<max_layers;++i) textures[i]=-1;
            memset(modelview[0],0,sizeof(modelview));
            memset(projection[0],0,sizeof(projection));
        }
    };

public:
    virtual int create_shader(const char *vertex,const char *fragment) { return -1; }
    virtual uint get_uniforms_count(int shader) { return 0; }
    virtual shader::uniform get_uniform(int shader,int idx) { return shader::uniform(); }
    virtual void remove_shader(int shader) {}

    virtual int create_uniform_buffer(int shader) { return -1; }
    virtual void set_uniform(int uniform_buffer,int idx,const float *buf,uint count) {}
    virtual void remove_uniform_buffer(int uniform_buffer) {}

public:
    virtual int create_vertex_buffer(const void *data,uint stride,uint count,vbo::usage_hint usage=vbo::static_draw) { return -1; }
    virtual void set_vertex_layout(int idx,vbo::layout layout) {}
    virtual void update_vertex_buffer(int idx,const void *data) {}
    virtual bool get_vertex_data(int idx,void *data) { return false; }
    virtual void remove_vertex_buffer(int idx) {}

    virtual int create_index_buffer(const void *data,vbo::index_size type,uint count,vbo::usage_hint usage=vbo::static_draw) { return -1; }
    virtual void update_index_buffer(int idx,const void *data) {}
    virtual bool get_index_data(int idx,void *data) { return false; }
    virtual void remove_index_buffer(int idx) {}

public:
    virtual int create_texture(const void *data,uint width,uint height,texture::color_format &format,int mip_count) { return -1; }
    virtual int create_cubemap(const void *data[6],uint width,texture::color_format &format,int mip_count) { return -1; }
    virtual void update_texture(int idx,const void *data,uint x,uint y,uint width,uint height,int mip) {}
    virtual void set_texture_wrap(int idx,texture::wrap s,texture::wrap t) {}
    virtual void set_texture_filter(int idx,texture::filter minification,texture::filter magnification,texture::filter mipmap,uint aniso) {}
    virtual bool get_texture_data(int texture,uint x,uint y,uint w,uint h,void *data) { return false; }

    virtual void remove_texture(int texture) {}
    virtual uint get_max_texture_dimention() { return 0; }
    virtual bool is_texture_format_supported(texture::color_format format) { return false; }

public:
    virtual int create_target(uint width,uint height,uint samples,const int *attachment_textures,
                              const int *attachment_sides,uint attachment_count,int depth_texture) { return -1; }
    virtual void resolve_target(int idx) {}
    virtual void remove_target(int idx) {}
    virtual uint get_max_target_attachments() { return 0; }
    virtual uint get_max_target_msaa() { return 0; }

public:
    virtual void clear(const viewport_state &s,bool color,bool depth,bool stencil) {}
    virtual void draw(const state &s) {}

    virtual void invalidate_cached_state() {}
    virtual void apply_state(const state &s) {}
};

enum render_api
{
    render_api_opengl,
    render_api_directx11,
    render_api_metal,
    //render_api_vulcan,
    render_api_custom
};

render_api get_render_api();
bool set_render_api(render_api api);
bool set_render_api(render_api_interface *api);
render_api_interface &get_api_interface();
render_api_interface::state &get_api_state();

}
