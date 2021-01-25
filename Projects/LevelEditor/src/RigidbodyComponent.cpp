#include "RigidbodyComponent.hpp"
#include "BasicMath.hpp"
#include "Actor.h"

namespace Diligent
{

RigidbodyComponent::RigidbodyComponent(Diligent::Actor* ownerP, Transform transform, PhysicsWorld* _world) :
    Component(ownerP)
{
    _rigidBody = _world->createRigidBody(transform);
}

RigidbodyComponent::RigidbodyComponent(Diligent::Actor* ownerP, Transform transform, PhysicsWorld* _world, int updateOrder) :
    Component(ownerP, updateOrder)
{
    _rigidBody = _world->createRigidBody(transform);
}


RigidbodyComponent::~RigidbodyComponent()
{
}


void RigidbodyComponent::update(double CurrTime, double ElapsedTime)
{
    //Update position
    reactphysics3d::Vector3 rbPosV3 = _rigidBody->getTransform().getPosition();
    reactphysics3d::Transform rbTrans = _rigidBody->getTransform();

    float3 pos = owner.getPosition();
    rbTrans.setPosition(reactphysics3d::Vector3(pos.x,pos.y,pos.z));
//    float3                  rbPosF3 = float3(rbPosV3.x, rbPosV3.y, rbPosV3.z);
//    owner.setPosition(rbPosF3);
    _rigidBody->setTransform(rbTrans);

    //Log info
    //std::string actorName = owner.GetActorName();
    //std::string message   = "Actor name : " + actorName + " ,x = " + std::to_string(rbPosV3.x) + " ,y = " + std::to_string(rbPosV3.y) + " ,z = " + std::to_string(rbPosV3.z);
    //Diligent::Log::Instance().addInfo(message);
    //Diligent::Log::Instance().Draw();
}

} // namespace Diligent