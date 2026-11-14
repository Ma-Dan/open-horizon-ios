//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

//requires: openvr https://github.com/ValveSoftware/openvr

#include "system_openvr.h"
#include "memory/invalid_object.h"
#include "render/render_opengl.h"
#include <openvr.h>

#ifdef _WIN32
    #include "windows.h"
#endif

namespace nya_system
{

bool disable_vsync()
{
#ifdef _WIN32
    typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int interval);
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if(!wglSwapIntervalEXT)
        return false;

    wglSwapIntervalEXT(0);
    return true;
#else
    return false;
#endif
}

inline vr::EVREye to_eye(vr_eye eye) { return eye==vr_eye_left?vr::EVREye::Eye_Left:vr::EVREye::Eye_Right; }

bool submit_texture(vr_eye eye,const nya_render::texture &tex,vr_colorspace colorspace)
{
    if(!vr::VRCompositor())
        return false;

    vr::EColorSpace vrcolorspace=vr::ColorSpace_Auto;
    if(colorspace==vr_colorspace_gamma)
        vrcolorspace=vr::ColorSpace_Gamma;
    else if(colorspace==vr_colorspace_linear)
        vrcolorspace=vr::ColorSpace_Linear;

    if(nya_render::get_render_api()==nya_render::render_api_directx11)
    {
        vr::Texture_t vr_tex={tex.get_dx11_tex_id(),vr::TextureType_DirectX,vrcolorspace};
        return vr::VRCompositor()->Submit(to_eye(eye),&vr_tex)==vr::VRCompositorError_None;
    }

    if(nya_render::get_render_api()==nya_render::render_api_opengl)
    {
        vr::Texture_t vr_tex={(void*)tex.get_gl_tex_id(),vr::TextureType_OpenGL,vrcolorspace};
        vr::EVRCompositorError result=vr::VRCompositor()->Submit(to_eye(eye),&vr_tex);
        nya_render::render_opengl::gl_bind_texture2d(0); //resets cache
        return result==vr::VRCompositorError_None;
    }

    return false;
}

bool submit_texture(vr_eye eye,const nya_scene::texture &tex,vr_colorspace colorspace)
{
    if(!tex.internal().get_shared_data().is_valid())
        return false;

    return submit_texture(eye,tex.internal().get_shared_data()->tex,colorspace);
}

bool submit_texture(vr_eye eye,const nya_scene::texture_proxy &tex,vr_colorspace colorspace)
{
    if(!tex.is_valid())
        return false;

    return submit_texture(eye,tex.get(),colorspace);
}

nya_math::mat4 get_proj_matrix(vr::IVRSystem *hmd,vr_eye eye,float znear,float zfar)
{
    nya_math::mat4 mat;
    if(!hmd)
        return mat;

    vr::HmdMatrix44_t vr_mat = hmd->GetProjectionMatrix(to_eye(eye), znear, zfar);
    memcpy(&mat, &vr_mat, sizeof(vr_mat));
    mat.transpose();
    return mat;
}

nya_math::vec3 get_pos(const vr::HmdMatrix34_t &mat)
{
    //return nya_math::mat4(mat.m).get_pos();

    return nya_math::vec3(mat.m[0][3],mat.m[1][3],mat.m[2][3]);
}

nya_math::quat get_rot(const vr::HmdMatrix34_t &mat)
{
    return nya_math::mat4(mat.m).get_rot();
}

nya_math::quat get_camera_rot(const vr::HmdMatrix34_t &mat)
{
    nya_math::quat q=nya_math::mat4(mat.m).get_rot();
    q.v.xy()= -q.v.xy();
    return q;
}

struct dist_vert { nya_math::vec2 pos,uv_r,uv_g,uv_b; };

nya_render::vbo create_distortion_mesh(vr::IVRSystem *hmd,vr_eye eye,bool flip_y,int segments_count)
{
    if (!hmd || segments_count<=0)
        return nya_memory::invalid_object<nya_render::vbo>();

    std::vector<dist_vert> verts;
    const float dw=1.0f/(segments_count-1),dh=1.0f/(segments_count-1);
    for(int y=0;y<segments_count;++y)
    {
        for(int x=0;x<segments_count;++x)
        {
            const float u=x*dw,v=y*dh;
            vr::DistortionCoordinates_t dc0;
            hmd->ComputeDistortion(to_eye(eye),u,v,&dc0);
            dist_vert vert;
            vert.pos.set(2.0f*u-1.0f,1.0f-2.0f*v);
            vert.uv_r.set(dc0.rfRed[0],dc0.rfRed[1]);
            vert.uv_g.set(dc0.rfGreen[0],dc0.rfGreen[1]);
            vert.uv_b.set(dc0.rfBlue[0],dc0.rfBlue[1]);
            verts.push_back(vert);
        }
    }

    if(!flip_y)
    {
        for(int i=0;i<(int)verts.size();++i)
        {
            dist_vert &v=verts[i];
            v.uv_r.y=1.0f-v.uv_r.y;
            v.uv_g.y=1.0f-v.uv_g.y;
            v.uv_b.y=1.0f-v.uv_b.y;
        }
    }

    std::vector<unsigned short> indices;

    for(unsigned short y=0;y<segments_count-1;++y)
    {
        for(unsigned short x=0;x<segments_count-1;++x)
        {
            const unsigned short a=segments_count*y+x,
                                    b=segments_count*y+x+1,
                                    c=(y+1)*segments_count+x+1,
                                    d=(y+1)*segments_count+x;
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    nya_render::vbo v;
    v.set_vertex_data(verts.data(),(unsigned int)sizeof(dist_vert),(unsigned int)verts.size());
    v.set_vertices(0,2);
    for(int i=0;i<3;++i)
        v.set_tc(i,(i+1)*sizeof(nya_math::vec2),2);
    v.set_index_data(indices.data(),nya_render::vbo::index2b,(unsigned int)indices.size());
    return v;
}

bool vr_postprocess::init(vr::IVRSystem *hmd)
{
    release();
    if(!hmd)
        return false;

    unsigned int w=0,h=0;
    hmd->GetRecommendedRenderTargetSize(&w,&h);
    if(!m_result_tex_l.build(0,w,h,nya_render::texture::color_rgba) || !m_result_tex_r.build(0,w,h,nya_render::texture::color_rgba))
        return false;

    m_fbo_l.set_color_target(m_result_tex_l.internal().get_shared_data()->tex);
    m_fbo_r.set_color_target(m_result_tex_r.internal().get_shared_data()->tex);

    nya_scene::shader s;
    s.build("@sampler tex tex\n"

            "@all\n"
                "varying vec2 tc_r;"
                "varying vec2 tc_g;"
                "varying vec2 tc_b;"
        "\n"
            "@vertex\n"
                "void main()"
                "{"
                    "tc_r=gl_MultiTexCoord0.xy;"
                    "tc_g=gl_MultiTexCoord1.xy;"
                    "tc_b=gl_MultiTexCoord1.xy;"
                    "gl_Position=vec4(gl_Vertex.xy,0.0,1.0);"
                "}"
        "\n"
            "@fragment\n"
                "uniform sampler2D tex;"
                "void main()"
                "{"
                    "gl_FragColor=vec4(texture2D(tex,tc_r).r,texture2D(tex,tc_g).g,texture2D(tex,tc_b).b,1.0);"
                "}");

    m_mat.get_default_pass().set_shader(s);
    m_tex.create();
    m_mat.set_texture("tex",m_tex);
    m_mesh_l=create_distortion_mesh(hmd,vr_eye_left);
    m_mesh_r=create_distortion_mesh(hmd,vr_eye_right);
    return true;
}

const nya_scene::texture &vr_postprocess::process(vr_eye eye,const nya_scene::texture &tex)
{
    nya_render::rect pv=nya_render::get_viewport();
    nya_render::set_viewport(0,0,get_width(),get_height());
    const nya_render::fbo &f=eye==vr_eye_left?m_fbo_l:m_fbo_r;
    f.bind();
    m_tex.set(tex);
    m_mat.internal().set();
    const nya_render::vbo &v=eye==vr_eye_left?m_mesh_l:m_mesh_r;
    v.bind();
    v.draw();
    v.unbind();
    m_mat.internal().unset();
    static nya_scene::texture no_tex;
    m_tex.set(no_tex);
    f.unbind();
    nya_render::set_viewport(pv);
    return eye==vr::Eye_Left?m_result_tex_l:m_result_tex_r;
}

const nya_scene::texture &vr_postprocess::process(vr_eye eye,const nya_scene::texture_proxy &from)
{
    if(!from.is_valid())
        return nya_memory::invalid_object<nya_scene::texture>();

    return process(eye,from.get());
}

void vr_postprocess::release()
{
    m_fbo_l.release();
    m_fbo_r.release();
    m_mesh_l.release();
    m_mesh_r.release();
    m_mat.unload();
    m_result_tex_l.unload();
    m_result_tex_r.unload();
}

}
