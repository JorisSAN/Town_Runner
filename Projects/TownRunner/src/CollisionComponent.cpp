#include "CollisionComponent.hpp"



namespace Diligent
{

CollisionComponent::CollisionComponent(Actor* ownerP) :
    Component(ownerP)
{
    _collider = nullptr;
}

CollisionComponent::CollisionComponent(Actor* ownerP, int updateOrder):
    Component(ownerP, updateOrder) 
{
    _collider = nullptr;
}

CollisionComponent::CollisionComponent(Actor* ownerP, CollisionShape* collisionShape) :
    Component(ownerP)
{
    _collisionShape.emplace_back(collisionShape);
    _collider = nullptr;
}

CollisionComponent::CollisionComponent(Actor* ownerP, CollisionShape* collisionShape, int updateOrder) :
    Component(ownerP, updateOrder)
{
    _collisionShape.emplace_back(collisionShape);
    _collider = nullptr;
}

CollisionComponent::CollisionComponent(Actor* ownerP, std::vector<CollisionShape*> collisionShape) :
    Component(ownerP) 
{
    _collisionShape = collisionShape;
}

CollisionComponent::CollisionComponent(Actor* ownerP, std::vector<CollisionShape*> collisionShape, int updateOrder) :
    Component(ownerP, updateOrder) 
{
    _collisionShape = collisionShape;
}

CollisionComponent::~CollisionComponent() {}

void CollisionComponent::update(double CurrTime, double ElapsedTime)
{
}

}