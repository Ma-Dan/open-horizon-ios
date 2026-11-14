//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#pragma once

#include "scene/material.h"
#include "render/fbo.h"
#include "render/vbo.h"
#include "math/quaternion.h"

namespace vr
{
    class IVRSystem;
    struct HmdMatrix34_t;
}

namespace nya_system
{

enum vr_eye
{
    vr_eye_left,
    vr_eye_right
};

enum vr_colorspace
{
    vr_colorspace_auto,
    vr_colorspace_gamma,
    vr_colorspace_linear,
};

bool disable_vsync();

bool submit_texture(vr_eye eye,const nya_render::texture &tex,vr_colorspace=vr_colorspace_auto);
bool submit_texture(vr_eye eye,const nya_scene::texture &tex,vr_colorspace=vr_colorspace_auto);
bool submit_texture(vr_eye eye,const nya_scene::texture_proxy &tex,vr_colorspace=vr_colorspace_auto);

nya_math::mat4 get_proj_matrix(vr::IVRSystem *hmd,vr_eye eye, float znear, float zfar);

nya_math::vec3 get_pos(const vr::HmdMatrix34_t &mat);
nya_math::quat get_rot(const vr::HmdMatrix34_t &mat);
nya_math::quat get_camera_rot(const vr::HmdMatrix34_t &mat);

nya_render::vbo create_distortion_mesh(vr::IVRSystem *hmd,vr_eye eye,bool flip_y=false,int segments_count=43);

class vr_postprocess
{
public:
    bool init(vr::IVRSystem *hmd);
    const nya_scene::texture &process(vr_eye eye,const nya_scene::texture &tex);
    const nya_scene::texture &process(vr_eye eye,const nya_scene::texture_proxy &tex);
    int get_width() const { return m_result_tex_l.get_width(); }
    int get_height() const { return m_result_tex_l.get_height(); }
    void release();

private:
    nya_render::fbo m_fbo_l,m_fbo_r;
    nya_render::vbo m_mesh_l, m_mesh_r;
    nya_scene::material m_mat;
    nya_scene::texture_proxy m_tex;
    nya_scene::texture m_result_tex_l,m_result_tex_r;
};

}
