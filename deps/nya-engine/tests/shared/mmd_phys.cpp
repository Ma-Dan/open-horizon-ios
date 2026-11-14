//nya-engine (C) nyan.developer@gmail.com released under the MIT license (see LICENSE)

#include "mmd_phys.h"

#include "btBulletDynamicsCommon.h"

inline btVector3 convert(const nya_math::vec3& from) { return btVector3(from.x,from.y,from.z); }
inline btQuaternion convert(const nya_math::quat& from) { return btQuaternion(from.v.x,from.v.y,from.v.z,from.w); }
inline nya_math::vec3 convert(const btVector3 &from) { return nya_math::vec3(from.x(),from.y(),from.z()); }
inline nya_math::quat convert(const btQuaternion &from) { return nya_math::quat(from.x(),from.y(),from.z(),from.w()); }

inline btTransform make_transform(nya_math::vec3 pos,nya_math::vec3 rot)
{
    btMatrix3x3 mx,my,mz;
    mx.setEulerZYX(rot.x,0.0f,0.0f);
    my.setEulerZYX(0.0f,rot.y,0.0f);
    mz.setEulerZYX(0.0f,0.0f,rot.z);

    btTransform	transform;
    transform.setIdentity();
    transform.setOrigin(convert(pos));
    transform.setBasis(my*mz*mx);
    return transform;
}

void mmd_phys_controller::init(mmd_mesh *mesh,btDiscreteDynamicsWorld *world)
{
    if(mesh && world)
        release();

    if(!mesh->is_mmd())
        return;

    const pmd_phys_data *pd=pmd_loader::get_additional_data(*mesh);
    if(!pd)
        pd=pmx_loader::get_additional_data(*mesh);

    if(!pd)
        return;

    m_mesh=mesh;
    m_world=world;

    const nya_render::skeleton &sk=mesh->get_skeleton();

    m_phys_bodies.resize(pd->rigid_bodies.size());
    for(int i=0;i<int(m_phys_bodies.size());++i)
    {
        const pmd_phys_data::rigid_body &from=pd->rigid_bodies[i];
        phys_body &to=m_phys_bodies[i];
        to.bone_idx=from.bone;

        switch(from.type)
        {
            case pmd_phys_data::shape_sphere: to.col_shape=new btSphereShape(from.size.x); break;
            case pmd_phys_data::shape_box: to.col_shape=new btBoxShape(convert(from.size)); break;
            case pmd_phys_data::shape_capsule: to.col_shape=new btCapsuleShape(from.size.x,from.size.y); break;
        }

        btScalar mass(0);
        btVector3 local_inertia(0,0,0);
        if(from.mode!=pmd_phys_data::object_static)
            mass=from.mass;

        if(mass>0.001f)
            to.col_shape->calculateLocalInertia(mass,local_inertia);

        const btTransform bone_offset=make_transform(from.pos,from.rot);

        btTransform	&transform=to.init_transform;
        transform.setIdentity();
        transform.setOrigin(convert(sk.get_bone_pos(from.bone)));
        transform=transform*bone_offset;

        to.motion_state=new btDefaultMotionState(transform);

        btRigidBody::btRigidBodyConstructionInfo rb_info(mass,to.motion_state,to.col_shape,local_inertia);
        rb_info.m_linearDamping=from.vel_attenuation;
        rb_info.m_angularDamping=from.rot_attenuation;
        rb_info.m_restitution=from.restriction;
        rb_info.m_friction=from.friction;
        rb_info.m_additionalDamping=true;

        to.rigid_body=new btRigidBody(rb_info);
        to.bone_tr=bone_offset;
        to.bone_tr_inv=bone_offset.inverse();

        if(from.mode==pmd_phys_data::object_static)
        {
            to.rigid_body->setCollisionFlags(to.rigid_body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            to.rigid_body->setActivationState(DISABLE_DEACTIVATION);
            to.kinematic=from.bone>=0;
        }
        else
            m_skeleton_helper.init_bone(from.bone,sk);

        to.rigid_body->setSleepingThresholds(0.0f,0.0f);

        world->addRigidBody(to.rigid_body,0x0001<<from.collision_group,from.collision_mask);
    }

    update_pre();

    m_phys_joints.resize(pd->joints.size());
    for(int i=0;i<int(m_phys_joints.size());++i)
    {
        const pmd_phys_data::joint &from=pd->joints[i];
        phys_joint &to=m_phys_joints[i];

        btTransform transform=make_transform(from.pos,from.rot);

        btRigidBody *src=m_phys_bodies[from.rigid_src].rigid_body;
        const btTransform src_tr=src->getWorldTransform().inverse()*transform;

        if(from.rigid_dst>=0)
        {
            btRigidBody *dst=m_phys_bodies[from.rigid_dst].rigid_body;
            const btTransform dst_tr=dst->getWorldTransform().inverse()*transform;
            to.constraint=new btGeneric6DofSpringConstraint(*src,*dst,src_tr,dst_tr,true);
        }
        else
            to.constraint=new btGeneric6DofSpringConstraint(*src,src_tr,true);

        to.constraint->setLinearLowerLimit(convert(from.pos_min));
        to.constraint->setLinearUpperLimit(convert(from.pos_max));
        to.constraint->setAngularLowerLimit(convert(from.rot_min));
        to.constraint->setAngularUpperLimit(convert(from.rot_max));

        if(from.pos_spring.x!=0.0f) to.constraint->enableSpring(0,true),to.constraint->setStiffness(0,from.pos_spring.x);
        if(from.pos_spring.y!=0.0f) to.constraint->enableSpring(1,true),to.constraint->setStiffness(1,from.pos_spring.y);
        if(from.pos_spring.z!=0.0f) to.constraint->enableSpring(2,true),to.constraint->setStiffness(2,from.pos_spring.z);

        to.constraint->enableSpring(3,true),to.constraint->setStiffness(3,from.rot_spring.x);
        to.constraint->enableSpring(4,true),to.constraint->setStiffness(4,from.rot_spring.y);
        to.constraint->enableSpring(5,true),to.constraint->setStiffness(5,from.rot_spring.z);

        world->addConstraint(to.constraint);
    }

    update_post();
    mesh->update(0);
}

void mmd_phys_controller::release()
{
    if(!m_world)
        return;

    for(int i=0;i<(int)m_phys_bodies.size();++i)
    {
        phys_body &b=m_phys_bodies[i];
        m_world->removeRigidBody(b.rigid_body);
        delete b.rigid_body;
        delete b.motion_state;
        delete b.col_shape;
    }

    m_phys_bodies.clear();

    for(int i=0;i<(int)m_phys_joints.size();++i)
    {
        m_world->removeConstraint(m_phys_joints[i].constraint);
        delete m_phys_joints[i].constraint;
    }

    m_phys_joints.clear();
    m_mesh=0;
    m_world=0;
}

void mmd_phys_controller::update_pre()
{
    if(!m_mesh)
        return;

    for(int i=0;i<(int)m_phys_bodies.size();++i)
    {
        const phys_body &b=m_phys_bodies[i];
        if(!b.kinematic)
            continue;

        btTransform tr;
        tr.setOrigin(convert(m_mesh->get_bone_pos(b.bone_idx)));
        tr.setRotation(convert(m_mesh->get_bone_rot(b.bone_idx)));
        b.motion_state->setWorldTransform(tr*b.bone_tr);
    }
}

void mmd_phys_controller::update_post()
{
    if(!m_mesh)
        return;

    m_skeleton_helper.set_transform(m_mesh->get_pos(),m_mesh->get_rot(),m_mesh->get_scale());

    for(int i=0;i<(int)m_phys_bodies.size();++i)
    {
        const phys_body &b=m_phys_bodies[i];
        if(b.kinematic)
            continue;

        const btTransform tr=b.rigid_body->getCenterOfMassTransform()*b.bone_tr_inv;
        m_skeleton_helper.set_bone(b.bone_idx,convert(tr.getOrigin()),convert(tr.getRotation()));
    }

    const nya_render::skeleton &sk=m_mesh->get_skeleton();

    for(int i=0;i<(int)m_phys_bodies.size();++i)
    {
        const phys_body &b=m_phys_bodies[i];
        if(b.kinematic)
            continue;

        const int idx=b.bone_idx;
        if(idx<0)
            continue;

        m_mesh->set_bone_pos(idx,m_skeleton_helper.get_bone_local_pos(idx,sk),false);
        m_mesh->set_bone_rot(idx,m_skeleton_helper.get_bone_local_rot(idx,sk),false);
    }
}

void mmd_phys_controller::reset()
{
    if(!m_mesh)
        return;

    for(int i=0;i<(int)m_phys_bodies.size();++i)
    {
        phys_body &b=m_phys_bodies[i];
        btTransform t=btTransform(convert(m_mesh->get_rot()),convert(m_mesh->get_pos()))*b.init_transform;
        b.rigid_body->setWorldTransform(t);
    }

    update_post();
}

void mmd_phys_controller::skeleton_helper::init_bone(int idx,const nya_render::skeleton &sk)
{
    if(idx<0)
        return;

    if(idx>=(int)m_map.size())
        m_map.resize(idx+1,-1);

    if(m_map[idx]>=0)
        return;

    m_map[idx]=(int)m_bones.size();
    m_bones.resize(m_bones.size()+1);
    m_bones.back().offset=sk.get_bone_original_pos(idx)-sk.get_bone_original_pos(sk.get_bone_parent_idx(idx));
}

void mmd_phys_controller::skeleton_helper::set_transform(const nya_math::vec3 &pos,const nya_math::quat &rot,const nya_math::vec3 &scale)
{
    m_tr.set_pos(pos);
    m_tr.set_rot(rot);
    m_tr.set_scale(scale.x,scale.y,scale.z);
}

void mmd_phys_controller::skeleton_helper::set_bone(int idx,const nya_math::vec3 &pos,const nya_math::quat &rot)
{
    if(idx<0 || idx>=(int)m_map.size())
        return;

    const int midx=m_map[idx];
    if(midx<0)
        return;

    m_bones[midx].pos=pos,m_bones[midx].rot=rot;
}

nya_math::vec3 mmd_phys_controller::skeleton_helper::get_bone_local_pos(int idx,const nya_render::skeleton &sk) const
{
    const int pidx=sk.get_bone_parent_idx(idx);
    if(pidx<0)
        return get_bone_pos(idx,sk);

    return get_bone_rot(pidx,sk).rotate_inv(get_bone_pos(idx,sk)-get_bone_pos(pidx,sk))-m_bones[m_map[idx]].offset;
}

nya_math::quat mmd_phys_controller::skeleton_helper::get_bone_local_rot(int idx,const nya_render::skeleton &sk) const
{
    const int pidx=sk.get_bone_parent_idx(idx);
    if(pidx<0)
        return get_bone_rot(idx,sk);

    return nya_math::quat::invert(get_bone_rot(pidx,sk))*get_bone_rot(idx,sk);
}

nya_math::vec3 mmd_phys_controller::skeleton_helper::get_bone_pos(int idx,const nya_render::skeleton &sk) const
{
    const int midx=m_map[idx];
    if(midx<0)
        return sk.get_bone_pos(idx);

    return m_tr.inverse_transform(m_bones[midx].pos);
}

nya_math::quat mmd_phys_controller::skeleton_helper::get_bone_rot(int idx,const nya_render::skeleton &sk) const
{
    const int midx=m_map[idx];
    if(midx<0)
        return sk.get_bone_rot(idx);

    return m_tr.inverse_transform(m_bones[midx].rot);
}

void mmd_phys_world::init()
{
    release();

	m_col_conf=new btDefaultCollisionConfiguration();
	m_dispatcher=new btCollisionDispatcher(m_col_conf);
	m_broadphase=new btDbvtBroadphase();
	m_solver=new btSequentialImpulseConstraintSolver();
	m_world=new btDiscreteDynamicsWorld(m_dispatcher,m_broadphase,m_solver,m_col_conf);
	m_world->setGravity(btVector3(0,-9.8f*10.0f,0));
}

void mmd_phys_world::update(int dt) { if(m_world) m_world->stepSimulation(dt/1000.0f,10); }

void mmd_phys_world::release()
{
    if(m_world) delete m_world,m_world=0;
    if(m_solver) delete m_solver,m_solver=0;
    if(m_broadphase) delete m_broadphase,m_broadphase=0;
    if(m_dispatcher) delete m_dispatcher,m_dispatcher=0;
    if(m_col_conf) delete m_col_conf,m_col_conf=0;
}

btDiscreteDynamicsWorld *mmd_phys_world::get_world() { return m_world; }
