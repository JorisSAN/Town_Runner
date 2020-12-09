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

#include <array>

#include "Light.h"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "TexturedCube.hpp"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "FastRand.hpp"

namespace Diligent
{

namespace
{

struct ShaderConstants
{
    float4x4 ViewProjMatrix;
    float4x4 ViewProjInvMatrix;
    float4   ViewportSize;
    int      ShowLightVolumes;
};

} // namespace

Light::Light()
{

}

Light::Light(const SampleInitInfo& InitInfo, RefCntAutoPtr<IRenderPass>& RenderPass) :
    m_pRenderPass(RenderPass)
{
    Initialize(InitInfo);
}

void Light::GetEngineInitializationAttribs(RENDER_DEVICE_TYPE DeviceType,
                                                             EngineCreateInfo&  Attribs,
                                                             SwapChainDesc&     SCDesc)
{
    // We do not need the depth buffer from the swap chain in this sample
    SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
}

void Light::CreateLightVolumePSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;

    PSODesc.Name = "Deferred lighting PSO";

    PSOCreateInfo.GraphicsPipeline.pRenderPass  = m_pRenderPass;
    PSOCreateInfo.GraphicsPipeline.SubpassIndex = 1; // This PSO will be used within the second subpass

    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                 = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode           = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable      = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False; // Do not write depth

    // We will use alpha-blending to accumulate influence of all lights
    auto& RT0Blend          = PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    RT0Blend.BlendEnable    = True;
    RT0Blend.BlendOp        = BLEND_OPERATION_ADD;
    RT0Blend.SrcBlend       = BLEND_FACTOR_ONE;
    RT0Blend.DestBlend      = BLEND_FACTOR_ONE;
    RT0Blend.SrcBlendAlpha  = BLEND_FACTOR_ZERO;
    RT0Blend.DestBlendAlpha = BLEND_FACTOR_ONE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Light volume VS";
        ShaderCI.FilePath        = "light_volume.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        VERIFY_EXPR(pVS != nullptr);
    }

    const auto IsVulkan = m_pDevice->GetDeviceCaps().IsVulkanDevice();
    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        // For Vulkan, we will use special version that uses native input attachments
        ShaderCI.SourceLanguage  = IsVulkan ? SHADER_SOURCE_LANGUAGE_GLSL : SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Light volume PS";
        ShaderCI.FilePath        = IsVulkan ? "light_volume_glsl.psh" : "light_volume_hlsl.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
        VERIFY_EXPR(pPS != nullptr);
    }

    // clang-format off
    const LayoutElement LayoutElems[] =
    {
        LayoutElement{0, 0, 3, VT_FLOAT32, False}, // Attribute 0 - vertex position
        LayoutElement{1, 0, 2, VT_FLOAT32, False}, // Attribute 1 - texture coordinates (we don't use them)
        LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}, // Attribute 2 - light position
        LayoutElement{3, 1, 3, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}  // Attribute 3 - light color
    };
    // clang-format on

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    // Define variable type that will be used by default
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "g_SubpassInputColor", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_SubpassInputDepthZ", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pLightVolumePSO);
    VERIFY_EXPR(m_pLightVolumePSO != nullptr);

    m_pLightVolumePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "ShaderConstants")->Set(m_pShaderConstantsCB);
    m_pLightVolumePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "ShaderConstants")->Set(m_pShaderConstantsCB);
}

void Light::CreateAmbientLightPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;

    PSODesc.Name = "Ambient light PSO";

    PSOCreateInfo.GraphicsPipeline.pRenderPass  = m_pRenderPass;
    PSOCreateInfo.GraphicsPipeline.SubpassIndex = 1; // This PSO will be used within the second subpass

    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False; // Disable depth

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    ShaderCI.UseCombinedTextureSamplers = true;

    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ambient light VS";
        ShaderCI.FilePath        = "ambient_light.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        VERIFY_EXPR(pVS != nullptr);
    }

    const auto IsVulkan = m_pDevice->GetDeviceCaps().IsVulkanDevice();

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.SourceLanguage  = IsVulkan ? SHADER_SOURCE_LANGUAGE_GLSL : SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Ambient light PS";
        ShaderCI.FilePath        = IsVulkan ? "ambient_light_glsl.psh" : "ambient_light_hlsl.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
        VERIFY_EXPR(pPS != nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "g_SubpassInputColor", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_SubpassInputDepthZ", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pAmbientLightPSO);
    VERIFY_EXPR(m_pAmbientLightPSO != nullptr);
}

void Light::CreateLightsBuffer()
{
    m_pLightsBuffer.Release();

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name           = "Lights instances buffer";
    VertBuffDesc.Usage          = USAGE_DYNAMIC;
    VertBuffDesc.BindFlags      = BIND_VERTEX_BUFFER;
    VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    VertBuffDesc.uiSizeInBytes  = sizeof(LightAttribs) * m_LightsCount;

    m_pDevice->CreateBuffer(VertBuffDesc, nullptr, &m_pLightsBuffer);
}

void Light::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreateUniformBuffer(m_pDevice, sizeof(ShaderConstants), "Shader constants CB", &m_pShaderConstantsCB);

    // Load textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);

    CreateLightsBuffer();
    InitLights();

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    CreateLightVolumePSO(pShaderSourceFactory);
    CreateAmbientLightPSO(pShaderSourceFactory);

    // Transition all resources to required states as no transitions are allowed within the render pass.
    StateTransitionDesc Barriers[] = //
        {
            {m_pShaderConstantsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true},
            {m_CubeVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true},
            {m_CubeIndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, true},
            {m_pLightsBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true}
        };

    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);
}

void Light::ReleaseWindowResources()
{
    m_FramebufferCache.clear();
    m_pLightVolumeSRB.Release();
    m_pAmbientLightSRB.Release();
}

void Light::ApplyLighting()
{
    // Set the lighting PSO
    m_pImmediateContext->SetPipelineState(m_pAmbientLightPSO);

    // Commit shader resources
    m_pImmediateContext->CommitShaderResources(m_pAmbientLightSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    {
        // Draw quad
        DrawAttribs DrawAttrs;
        DrawAttrs.NumVertices = 4;
        DrawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL; // Verify the state of vertex and index buffers
        m_pImmediateContext->Draw(DrawAttrs);
    }

    {
        // Map the cube's constant buffer and fill it in with its view-projection matrix
        MapHelper<LightAttribs> LightsData(m_pImmediateContext, m_pLightsBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        memcpy(LightsData, m_Lights.data(), m_Lights.size() * sizeof(m_Lights[0]));
    }

    // Bind vertex and index buffers
    Uint32   Offsets[2] = {};
    IBuffer* pBuffs[2]  = {m_CubeVertexBuffer, m_pLightsBuffer};
    // Note that RESOURCE_STATE_TRANSITION_MODE_TRANSITION are not allowed inside render pass!
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, Offsets, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    // Set the lighting PSO
    m_pImmediateContext->SetPipelineState(m_pLightVolumePSO);

    // Commit shader resources
    m_pImmediateContext->CommitShaderResources(m_pLightVolumeSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    {
        // Draw lights
        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType    = VT_UINT32; // Index type
        DrawAttrs.NumIndices   = 36;
        DrawAttrs.NumInstances = m_LightsCount;
        DrawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL; // Verify the state of vertex and index buffers
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }
}

void Light::UpdateLights(float fElapsedTime)
{
    float3 VolumeMin{-static_cast<float>(GridDim), -static_cast<float>(GridDim), -static_cast<float>(GridDim)};
    float3 VolumeMax{+static_cast<float>(GridDim), +static_cast<float>(GridDim), +static_cast<float>(GridDim)};
    for (int light = 0; light < m_LightsCount; ++light)
    {
        auto& Light = m_Lights[light];
        auto& Dir   = m_LightMoveDirs[light];
        Light.Location += Dir * fElapsedTime;
        auto ClampCoordinate = [](float& Coord, float& Dir, float Min, float Max) //
        {
            if (Coord < Min)
            {
                Coord += (Min - Coord) * 2.f;
                Dir *= -1.f;
            }
            else if (Coord > Max)
            {
                Coord -= (Coord - Max) * 2.f;
                Dir *= -1.f;
            }
        };
        ClampCoordinate(Light.Location.x, Dir.x, VolumeMin.x, VolumeMax.x);
        ClampCoordinate(Light.Location.y, Dir.y, VolumeMin.y, VolumeMax.y);
        ClampCoordinate(Light.Location.z, Dir.z, VolumeMin.z, VolumeMax.z);
    }
}

void Light::CreateSRB(RefCntAutoPtr<ITexture> pColorBuffer, RefCntAutoPtr<ITexture> pDepthZBuffer)
{
    // Create SRBs that reference the framebuffer textures

    if (!m_pLightVolumeSRB)
    {
        m_pLightVolumePSO->CreateShaderResourceBinding(&m_pLightVolumeSRB, true);
        m_pLightVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SubpassInputColor")->Set(pColorBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_pLightVolumeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SubpassInputDepthZ")->Set(pDepthZBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }

    if (!m_pAmbientLightSRB)
    {
        m_pAmbientLightPSO->CreateShaderResourceBinding(&m_pAmbientLightSRB, true);
        m_pAmbientLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SubpassInputColor")->Set(pColorBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_pAmbientLightSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SubpassInputDepthZ")->Set(pDepthZBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }
}

void Light::InitLights()
{
    // Randomly distribute lights within the volume

    FastRandReal<float> Rnd{0, 0, 1};

    m_Lights.resize(m_LightsCount);
    for (auto& Light : m_Lights)
    {
        Light.Location = (float3{Rnd(), Rnd(), Rnd()} - float3{0.5f, 0.5f, 0.5f}) * 2.0 * static_cast<float>(GridDim);
        Light.Size     = 0.5f + Rnd() * 0.5f;
        Light.Color    = float3{Rnd(), Rnd(), Rnd()};
    }

    m_LightMoveDirs.resize(m_Lights.size());
    for (auto& MoveDir : m_LightMoveDirs)
    {
        MoveDir = (float3{Rnd(), Rnd(), Rnd()} - float3{0.5f, 0.5f, 0.5f}) * 1.f;
    }
}

// Render a frame
void Light::RenderActor(const Camera& camera, bool IsShadowPass)
{
    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    const auto  CameraView     = camera.m_ViewMatrix * SrfPreTransform;
    const auto& CameraWorld    = camera.GetWorldMatrix();
    float3      CameraWorldPos = float3::MakeVector(CameraWorld[3]);
    const auto& Proj           = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    auto CameraViewProj = CameraView * Proj;

    const auto& SCDesc = m_pSwapChain->GetDesc();

    {
        // Update constant buffer
        MapHelper<ShaderConstants> Constants(m_pImmediateContext, m_pShaderConstantsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        Constants->ViewProjMatrix    = CameraViewProj.Transpose();
        Constants->ViewProjInvMatrix = CameraViewProj.Inverse().Transpose();
        Constants->ViewportSize      = float4{
            static_cast<float>(SCDesc.Width),
            static_cast<float>(SCDesc.Height),
            1.f / static_cast<float>(SCDesc.Width),
            1.f / static_cast<float>(SCDesc.Height) //
        };
        Constants->ShowLightVolumes = m_ShowLightVolumes ? 1 : 0;
    }

    ApplyLighting();
}

void Light::UpdateActor(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    if (m_AnimateLights)
        UpdateLights(static_cast<float>(ElapsedTime));

    float4x4 View = float4x4::Translation(0.0f, 0.0f, 25.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute world-view-projection matrix
    m_CameraViewProjMatrix    = View * SrfPreTransform * Proj;
    m_CameraViewProjInvMatrix = m_CameraViewProjMatrix.Inverse();
}

} // namespace Diligent
