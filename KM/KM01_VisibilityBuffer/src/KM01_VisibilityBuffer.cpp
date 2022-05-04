/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <random>

#include "KM01_VisibilityBuffer.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "CommonlyUsedStates.h"
#include "../../../Tutorials/Common/src/TexturedCube.hpp"
#include "imgui.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new KM01_VisibilityBuffer();
}


TEXTURE_FORMAT KM01_VisibilityBuffer::VisibilityBuffer::GetIdBufferFormat()
{
    return TEX_FORMAT_RGBA8_UNORM;
}

TEXTURE_FORMAT KM01_VisibilityBuffer::VisibilityBuffer::GetDepthBufferFormat()
{
    return TEX_FORMAT_D32_FLOAT;
}

void KM01_VisibilityBuffer::VisibilityBuffer::Create(IRenderDevice* pDevice, std::shared_ptr<VisibilityBuffer>& VisBuffer, Uint32 W, Uint32 H)
{
    if (VisBuffer && VisBuffer->Width == W && VisBuffer->Height == H)
        return;

    VisBuffer.reset(new VisibilityBuffer);
    VisBuffer->Width = W;
    VisBuffer->Height = H;

    // Create id buffer
    TextureDesc RTIdDesc;
    RTIdDesc.Name      = "Visibility id buffer";
    RTIdDesc.Type      = RESOURCE_DIM_TEX_2D;
    RTIdDesc.Width     = W;
    RTIdDesc.Height    = H;
    RTIdDesc.MipLevels = 1;
    RTIdDesc.Format    = GetIdBufferFormat();
    // The render target can be bound as a shader resource and as a render target
    RTIdDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    // Define optimal clear value
    RTIdDesc.ClearValue.Format   = RTIdDesc.Format;
    RTIdDesc.ClearValue.Color[0] = 1.f;
    RTIdDesc.ClearValue.Color[1] = 1.f;
    RTIdDesc.ClearValue.Color[2] = 1.f;
    RTIdDesc.ClearValue.Color[3] = 1.f;
    RefCntAutoPtr<ITexture> pRTColor;
    pDevice->CreateTexture(RTIdDesc, nullptr, &pRTColor);
    // Store the render target view
    VisBuffer->m_pIdRTV = pRTColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

    // Create depth buffer
    TextureDesc RTDepthDesc = RTIdDesc;
    RTDepthDesc.Name        = "Visibility depth buffer";
    RTDepthDesc.Format      = GetDepthBufferFormat();
    RTDepthDesc.BindFlags   = BIND_DEPTH_STENCIL;
    // Define optimal clear value
    RTDepthDesc.ClearValue.Format               = RTDepthDesc.Format;
    RTDepthDesc.ClearValue.DepthStencil.Depth   = 1;
    RTDepthDesc.ClearValue.DepthStencil.Stencil = 0;
    RefCntAutoPtr<ITexture> pRTDepth;
    pDevice->CreateTexture(RTDepthDesc, nullptr, &pRTDepth);
    // Store the depth-stencil view
    VisBuffer->m_pDepthDSV = pRTDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
}


void KM01_VisibilityBuffer::CreateCubePSO()
{
    // clang-format off
    // Define vertex shader input layout
    // This tutorial uses two types of input: per-vertex data and per-instance data.
    LayoutElement LayoutElems[] =
    {
        // Per-instance data - second buffer slot
        // We will use four attributes to encode instance-specific 4x4 transformation matrix
        // Attribute 2 - first row
        LayoutElement{0, 0, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 3 - second row
        LayoutElement{1, 0, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 4 - third row
        LayoutElement{2, 0, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 5 - fourth row
        LayoutElement{3, 0, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
    };
    // clang-format on

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice                = m_pDevice;
    CubePsoCI.RTVFormat              = VisibilityBuffer::GetIdBufferFormat();
    CubePsoCI.DSVFormat              = VisibilityBuffer::GetDepthBufferFormat();
    CubePsoCI.pShaderSourceFactory   = pShaderSourceFactory;
    CubePsoCI.VSFilePath             = "visbuf_cube.vsh";
    CubePsoCI.PSFilePath             = "visbuf_cube.psh";
    CubePsoCI.ExtraLayoutElements    = LayoutElems;
    CubePsoCI.NumExtraLayoutElements = _countof(LayoutElems);

    // Fetch vertex manually in vertex shader
    // clang-format off
    ShaderResourceVariableDesc Vars[] =
    {
    {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    {SHADER_TYPE_VERTEX, "g_VertexData", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    {SHADER_TYPE_VERTEX, "g_IndexData", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    // clang-format on
    CubePsoCI.ShaderVars = Vars;
    CubePsoCI.ShaderVarCount = _countof(Vars);

    m_pCubePSO = TexturedCube::CreatePipelineState(CubePsoCI);

    // Create dynamic uniform buffer that will store our transformation matrix
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly to the pipeline state object.
    m_pCubePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pCubePSO->CreateShaderResourceBinding(&m_CubeSRB, true);
}

void KM01_VisibilityBuffer::CreateVisBufShadePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues
    // It is always a good idea to give objects descriptive names
    // clang-format off
    PSOCreateInfo.PSODesc.Name                                  = "Visibility Buffer Shade PSO";
    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType                          = PIPELINE_TYPE_GRAPHICS;
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    // In this tutorial, we will load shaders from file. To be able to do that,
    // we need to create a shader source stream factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Visibility Buffer Shade VS";
        ShaderCI.FilePath        = "visbuf_shade.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Render Target PS";
        ShaderCI.FilePath        = "visbuf_shade.psh";

        m_pDevice->CreateShader(ShaderCI, &pPS);

        /*
        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        BufferDesc CBDesc;
        CBDesc.Name           = "RTPS constants CB";
        CBDesc.Size           = sizeof(float4) + sizeof(float2x2) * 2;
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_RTPSConstants);
        */
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] =
    {
        { SHADER_TYPE_PIXEL, "g_IdTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE }
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        { SHADER_TYPE_PIXEL, "g_IdTexture", Sam_LinearClamp }
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pVisBufShadePSO);

    // Since we did not explcitly specify the type for Constants, default type
    // (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
    // to the pipeline state object.
    //m_pVisBufShadePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants")->Set(m_RTPSConstants);

    m_pVisBufShadePSO->CreateShaderResourceBinding(&m_VisBufShadeSRB, true);
}

void KM01_VisibilityBuffer::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * MaxInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

void KM01_VisibilityBuffer::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt("Grid Size", &m_GridSize, 1, 32))
        {
            PopulateInstanceBuffer();
        }
    }
    ImGui::End();
}

void KM01_VisibilityBuffer::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreateCubePSO();
    CreateVisBufShadePSO();

    CreateInstanceBuffer();

    // Load textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_NORM_UV, BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice, BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    m_TextureSRV       = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    // Set cube texture SRV in the SRB
    m_CubeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
    m_CubeSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_VertexData")->Set(m_CubeVertexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_CubeSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_IndexData")->Set(m_CubeIndexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

}

void KM01_VisibilityBuffer::WindowResize(Uint32 Width, Uint32 Height)
{
    const auto& Desc = m_pSwapChain->GetDesc();
    VisibilityBuffer::Create(m_pDevice, m_MainVisBuf, Desc.Width, Desc.Height);

    if (m_MainVisBuf)
    {
        m_VisBufShadeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_IdTexture")->Set(m_MainVisBuf->m_pIdRTV->GetTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }
}

void KM01_VisibilityBuffer::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);
    // In this tutorial we will be using off-screen depth-stencil buffer, so
    // we do not need the one in the swap chain.
    Attribs.SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
}

void KM01_VisibilityBuffer::PopulateInstanceBuffer()
{
    // Populate instance data buffer
    std::vector<float4x4> InstanceData(m_GridSize * m_GridSize * m_GridSize);

    float fGridSize = static_cast<float>(m_GridSize);

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                      // to generate consistent distribution.

    std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
    std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
    std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);

    float BaseScale = 0.6f / fGridSize;
    int   instId    = 0;
    for (int x = 0; x < m_GridSize; ++x)
    {
        for (int y = 0; y < m_GridSize; ++y)
        {
            for (int z = 0; z < m_GridSize; ++z)
            {
                // Add random offset from central position in the grid
                float xOffset = 2.f * (x + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float yOffset = 2.f * (y + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float zOffset = 2.f * (z + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                // Random scale
                float scale = BaseScale * scale_distr(gen);
                // Random rotation
                float4x4 rotation = float4x4::RotationX(rot_distr(gen)) * float4x4::RotationY(rot_distr(gen)) * float4x4::RotationZ(rot_distr(gen));
                // Combine rotation, scale and translation
                float4x4 matrix        = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(xOffset, yOffset, zOffset);
                InstanceData[instId++] = matrix;
            }
        }
    }
    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}


// Render a frame
void KM01_VisibilityBuffer::Render()
{
    //auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    //auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    assert(m_MainVisBuf);

    {
        auto pRTV = m_MainVisBuf->m_pIdRTV;
        auto pDSV = m_MainVisBuf->m_pDepthDSV;

        // Clear the visibility buffer
        const float ClearColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            // Map the buffer and write current world-view-projection matrix
            MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants[0] = m_ViewProjMatrix.Transpose();
            CBConstants[1] = m_RotationMatrix.Transpose();
        }

        // Bind vertex, instance and index buffers
        const Uint64 offsets[] = {0, 0};
        IBuffer*     pBuffs[]  = {m_InstanceBuffer};
        m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        //m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set the pipeline state
        m_pImmediateContext->SetPipelineState(m_pCubePSO);
        // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
        // makes sure that resources are transitioned to required states.
        m_pImmediateContext->CommitShaderResources(m_CubeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs DrawAttrs; // This is an indexed draw call
        DrawAttrs.NumVertices  = 36;
        DrawAttrs.NumInstances = m_GridSize * m_GridSize * m_GridSize; // The number of instances
        // Verify the state of vertex and index buffers
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->Draw(DrawAttrs);
    }

    {
        auto pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        // Clear the default render target
        const float Zero[] = {0.0f, 0.0f, 0.0f, 1.0f};
        m_pImmediateContext->SetRenderTargets(1, &pRTV, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set the render target pipeline state
        m_pImmediateContext->SetPipelineState(m_pVisBufShadePSO);

        // Commit the render target shader's resources
        m_pImmediateContext->CommitShaderResources(m_VisBufShadeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Draw the render target's vertices
        DrawAttribs RTDrawAttrs;
        RTDrawAttrs.NumVertices = 4;
        RTDrawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL; // Verify the state of vertex and index buffers
        m_pImmediateContext->Draw(RTDrawAttrs);
    }

}

void KM01_VisibilityBuffer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    // Set cube view matrix
    float4x4 View = float4x4::RotationX(-0.6f) * float4x4::Translation(0.f, 0.f, 4.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute view-projection matrix
    m_ViewProjMatrix = View * SrfPreTransform * Proj;

    // Global rotation matrix
    m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-static_cast<float>(CurrTime) * 0.25f);
}

} // namespace Diligent
