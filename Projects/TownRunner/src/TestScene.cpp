/*
 *  Copyright 2019-2020 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include <vector>

#include <stdio.h>

#include "TestScene.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "Sphere.h"
#include "Helmet.h"
#include "AnimPeople.h"
#include "InputController.hpp"
#include "Actor.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new TestScene();
}

void TestScene::GetEngineInitializationAttribs(RENDER_DEVICE_TYPE DeviceType, EngineCreateInfo& EngineCI, SwapChainDesc& SCDesc)
{
    SampleBase::GetEngineInitializationAttribs(DeviceType, EngineCI, SCDesc);

    EngineCI.Features.DepthClamp = DEVICE_FEATURE_STATE_OPTIONAL;
}

void TestScene::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    //Initialize react physic 3d
    _reactPhysic = new ReactPhysic();

    Init = InitInfo;

    m_Camera.SetPos(float3(-5.0f, 0.0f, 0.0f));
    m_Camera.SetRotation(PI_F / 2.f, 0, +PI_F);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);

    envMaps.reset(new EnvMap(Init, m_BackgroundMode));

    ActorCreation();
}

void TestScene::ActorCreation()
{
    //Create actor and their components
    //Create Helmet
    Helmet*                    helmet1 = new Helmet(Init, m_BackgroundMode);
    reactphysics3d::Transform helmetTransform(reactphysics3d::Vector3::zero(), reactphysics3d::Quaternion::identity());
    RigidbodyComponent*       helmetRigidbody = new RigidbodyComponent(helmet1->GetActor(), helmetTransform, _reactPhysic->GetPhysicWorld());
    helmet1->addComponent(helmetRigidbody);

    //Create Anim people
    AnimPeople* animPeople1 = new AnimPeople(Init, m_BackgroundMode);

    actors.emplace_back(helmet1);
    actors.emplace_back(animPeople1);

    for (auto actor : actors)
    {
        actor->setPosition(float3(0.0f, 0.0f, 0.0f));
    }
}

// Render a frame
void TestScene::Render()
{
    // Reset default framebuffer
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Clear the back buffer
    const float ClearColor[] = {0.23f, 0.5f, 0.74f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    envMaps->RenderActor(m_Camera, false);

    for (auto actor : actors)
    {
        if (actor->getState() == Actor::ActorState::Active)
        {
            actor->RenderActor(m_Camera, false);
        }
    }
}

void TestScene::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    //React physic
    _reactPhysic->Update();


    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

    // Animate Actors
    for (auto actor : actors)
    {
            actor->Update(CurrTime, ElapsedTime);
    }

    if (m_InputController.IsKeyDown(InputKeys::MoveBackward))
        actors.back()->setState(Actor::ActorState::Dead);

    // Delete dead actors
    std::vector<Actor*> deadActors;
    for (auto actor : actors)
    {
        if (actor->getState() == Actor::ActorState::Dead)
        {
            deadActors.emplace_back(actor);
        }
    }
    for (auto deadActor : deadActors)
    {
    }
}

void TestScene::addActor(Actor* actor)
{
    actors.emplace_back(actor);
    actor->Initialize(Init);
}

void TestScene::removeActor(Actor* actor)
{
    auto iter = std::find(begin(actors), end(actors), actor);
    if (iter != end(actors))
    {
        std::iter_swap(iter, end(actors) - 1);
        actors.pop_back();
    }
}

} // namespace Diligent
