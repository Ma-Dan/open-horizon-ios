//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "render_directx11.h"
#include "render_opengl.h"
#include "render_metal.h"

#include "texture.h"
#include "transform.h"
#include "platform_specific_gl.h"

#include <map>

namespace nya_render
{

namespace
{
    nya_log::log_base *render_log=0;
    nya_render::render_api_interface::state current_state;

    render_api_interface *available_render_interface()
    {
        if (render_opengl::get().is_available())
            return &render_opengl::get();

        if (render_directx11::get().is_available())
            return &render_directx11::get();

        return 0;
    }

    render_api_interface *render_interface = available_render_interface();
}

void set_log(nya_log::log_base *l) { render_log=l; }
nya_log::log_base &log()
{
    if(!render_log)
        return nya_log::log();

    return *render_log;
}

void set_viewport(const rect &r) { current_state.viewport=r; }
void set_viewport(int x,int y,int width,int height)
{
    current_state.viewport.x=x;
    current_state.viewport.y=y;
    current_state.viewport.width=width;
    current_state.viewport.height=height;
}
const rect &get_viewport() { return current_state.viewport; }

void set_projection_matrix(const nya_math::mat4 &mat) { transform::get().set_projection_matrix(mat); }
void set_modelview_matrix(const nya_math::mat4 &mat) { transform::get().set_modelview_matrix(mat); }
void set_orientation_matrix(const nya_math::mat4 &mat) { transform::get().set_orientation_matrix(mat); }

const nya_math::mat4 &get_projection_matrix() { return transform::get().get_projection_matrix(); }
const nya_math::mat4 &get_modelview_matrix() { return transform::get().get_modelview_matrix(); }
const nya_math::mat4 &get_orientation_matrix() { return transform::get().get_orientation_matrix(); }

void set_clear_color(float r,float g,float b,float a)
{
    current_state.clear_color[0]=r;
    current_state.clear_color[1]=g;
    current_state.clear_color[2]=b;
    current_state.clear_color[3]=a;
}
void set_clear_depth(float value) { current_state.clear_depth=value; }
void clear(bool color,bool depth,bool stencil) { render_interface->clear(current_state,color,depth,stencil); }

void blend::enable(blend::mode src,blend::mode dst)
{
    current_state.blend=true;
    current_state.blend_src=src;
    current_state.blend_dst=dst;
}
void blend::disable() { current_state.blend=false; }

void cull_face::enable(cull_face::order o)
{
    current_state.cull_face=true;
    current_state.cull_order=o;
}
void cull_face::disable() { current_state.cull_face=false; }

void depth_test::enable(comparsion mode)
{
    current_state.depth_test=true;
    current_state.depth_comparsion=mode;
}
void depth_test::disable() { current_state.depth_test=false; }

void zwrite::enable() { current_state.zwrite=true; }
void zwrite::disable() { current_state.zwrite=false; }

void color_write::enable() { current_state.color_write=true; }
void color_write::disable() { current_state.color_write=false; }

void scissor::enable(const rect &r) { current_state.scissor_enabled=true; current_state.scissor=r; }
void scissor::enable(int x,int y,int w,int h)
{
    current_state.scissor_enabled=true;
    current_state.scissor.x=x;
    current_state.scissor.y=y;
    current_state.scissor.width=w;
    current_state.scissor.height=h;
}
void scissor::disable() { current_state.scissor_enabled=false; }
bool scissor::is_enabled() { return current_state.scissor_enabled; }
const rect &scissor::get() { return current_state.scissor; }

void set_state(const state &s) { *((state *)&current_state)=s; }
const state &get_state() { return current_state; }

void apply_state(bool ignore_cache)
{
    if (ignore_cache)
        render_interface->invalidate_cached_state();

    render_interface->apply_state(current_state);
}

#ifdef DIRECTX11
namespace
{
    ID3D11Device *render_device=0;
    ID3D11DeviceContext *render_context=0;

void discard_state()
{
    invalidate_resources();
    applied_state=current_state=state();

    rasterizer_state=rasterizer_state_class();
    depth_state=decltype(depth_state)();
    blend_state=decltype(blend_state)();
}

}

void release_states()
{
    depth_state.release();
    rasterizer_state.release();
    blend_state.release();
}

ID3D11Device *get_device() { return render_device; }
void set_device(ID3D11Device *device)
{
    if(render_device==device)
        return;

    discard_state();
    render_device=device;
}

ID3D11DeviceContext *get_context() { return render_context; }
void set_context(ID3D11DeviceContext *context)
{
    if(render_context==context)
        return;

    discard_state();
    render_context=context;
}
#endif

render_api get_render_api()
{
    if (render_interface == &render_opengl::get())
        return render_api_opengl;

    if (render_interface == &render_directx11::get())
        return render_api_directx11;

    // render_metal is not compiled on macOS (requires Objective-C++); it can
    // never be the active api there, so no render_api_metal check is needed.

    return render_api_custom;
}

bool set_render_api(render_api api)
{
    switch(api)
    {
        case render_api_directx11: return set_render_api(&render_directx11::get());
        case render_api_opengl: return set_render_api(&render_opengl::get());
        case render_api_metal: return false; //not implemented yet, excluded from build on macOS
        case render_api_custom: return false;
    }
}

bool set_render_api(render_api_interface *api)
{
    if (!api || !api->is_available())
        return false;
    
    render_interface = api;
    return true;
}

render_api_interface::state &get_api_state() { return current_state; }
render_api_interface &get_api_interface() { return *render_interface; }

unsigned int release_resources()
{
    unsigned int count=0;
    /*
    count+=release_textures();
    count+=release_shaders();
    count+=release_vbos();
    count+=release_fbos();
    DIRECTX11_ONLY(release_states());*/
    return count;
}

unsigned int invalidate_resources()
{
    unsigned int count=0;/*
    count+=texture_obj::invalidate_all();
    count+=invalidate_shaders();
    count+=invalidate_vbos();
    count+=invalidate_fbos();*/
    return count;
}

void set_default_target(ID3D11RenderTargetView *color,ID3D11DepthStencilView *depth,int height)
{
    DIRECTX11_ONLY(dx_set_target(color,depth,true,height));
}

}
