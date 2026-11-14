//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

//#define NYA_DIRECTX11
//#define NYA_OPENGL3

#include "render_objects.h"
#include "texture.h"

#ifdef _WIN32
    #include <windows.h>
    #if defined(_MSC_VER) && _MSC_VER >= 1700
        #if _WIN32_WINNT >= _WIN32_WINNT_WIN8 && !_USING_V110_SDK71_
            #include "winapifamily.h"
            #if defined(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
            #elif defined(WINAPI_PARTITION_PHONE) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE)
                #define WINDOWS_PHONE8
                #define WINDOWS_METRO
            #elif defined(WINAPI_PARTITION_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
                #define WINDOWS_METRO
            #endif
        #endif
    #endif

    #if defined WINDOWS_METRO || defined NYA_DIRECTX11
        #define DIRECTX11
    #endif

    #ifdef DIRECTX11
        #include <d3d11.h>
        #include <vector>
        #define NO_EXTENSIONS_INIT
    #else
        #include <gl/gl.h>
        #include <gl/glext.h>

        #ifdef NYA_OPENGL3
            #define OPENGL3
        #endif
    #endif
#elif defined __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #import <OpenGLES/ES2/gl.h>
        #import <OpenGLES/ES2/glext.h>

        #define OPENGL_ES
    #else
        #ifdef NYA_OPENGL3
            #define OPENGL3
        #endif

        #ifdef OPENGL3
            #include <OpenGL/gl3.h>
        #else
            #include <OpenGL/gl.h>
        #endif

        #include <OpenGL/glext.h>
        #define NO_EXTENSIONS_INIT
    #endif

#elif __ANDROID__ || EMSCRIPTEN
    #include <GLES2/gl2.h>
    #define GL_GLEXT_PROTOTYPES
    #include <GLES2/gl2ext.h>
    #define OPENGL_ES
#else
    #include <GL/glx.h>
    #include <GL/gl.h>
    #include <GL/glext.h>
#endif

#ifdef OPENGL_ES
    #define ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
    #define NO_EXTENSIONS_INIT
#endif

#ifdef OPENGL3
    #define ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
#endif

#ifdef DIRECTX11
    #define DIRECTX11_ONLY(...) __VA_ARGS__
    #define OPENGL_ONLY(...)
#else
    #define DIRECTX11_ONLY(...)
    #define OPENGL_ONLY(...) __VA_ARGS__
#endif

namespace nya_render
{
#ifdef ATTRIBUTES_INSTEAD_OF_CLIENTSTATES
    enum
    {
        vertex_attribute=0,
        normal_attribute=1,
        color_attribute=2,
        tc0_attribute=3,
        max_attributes = tc0_attribute+16
    };
#endif

#ifndef DIRECTX11
    bool has_extension(const char *name);
    void *get_extension(const char*ext_name);
#endif

#ifdef DIRECTX11
    ID3D11InputLayout *dx_get_layout(int mesh_idx);
    ID3D11InputLayout *dx_add_layout(int mesh_idx,
                                  const D3D11_INPUT_ELEMENT_DESC*desc,size_t desc_size);
    void dx_remove_layout(int mesh_idx);

    struct dx_target
    {
        ID3D11RenderTargetView *color;
        ID3D11DepthStencilView *depth;

        dx_target(): color(0),depth(0) {}
    };

    dx_target dx_get_default_target();
    dx_target dx_get_target();
    bool dx_is_default_target();
    int dx_get_target_height();
    void dx_unbind_texture_idx(int idx);

    void dx_set_target(ID3D11RenderTargetView *color,ID3D11DepthStencilView *depth,bool default=false,int height=-1);
#endif

    void reset_vbo_state();

    int release_textures();
    int release_vbos();
    int release_fbos();
    void release_states();

    int invalidate_vbos();
    int invalidate_fbos();
}
