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
#include "ShaderMacroHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "CommonlyUsedStates.h"
#include "../../../Tutorials/Common/src/TexturedCube.hpp"

#include "imgui.h"

namespace Diligent
{

#include "../assets/structures.fxh"

template <class T>
static T DivideAndRoundUp(T Dividend, T Divisor)
{
    return (Dividend + Divisor - 1) / Divisor;
}

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
    CubePsoCI.ExtraLayoutElements    = nullptr;;
    CubePsoCI.NumExtraLayoutElements = 0;

    // Fetch vertex manually in vertex shader
    // clang-format off
    ShaderResourceVariableDesc Vars[] =
    {
    {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_VERTEX, "g_CachedVertexData", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    // clang-format on
    CubePsoCI.ShaderVars = Vars;
    CubePsoCI.ShaderVarCount = _countof(Vars);

    m_PipelineCube.pPSO = TexturedCube::CreatePipelineState(CubePsoCI);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly to the pipeline state object.
    m_PipelineCube.pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_GlobalConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_PipelineCube.pPSO->CreateShaderResourceBinding(&m_PipelineCube.pSRB, true);
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
    {SHADER_TYPE_PIXEL, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_PIXEL, "g_CachedVertexData", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    {SHADER_TYPE_PIXEL, "g_IdTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // clang-format off
    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    ImmutableSamplerDesc ImtblSamplers[] =
    {
    { SHADER_TYPE_PIXEL, "g_IdTexture", Sam_PointClamp }
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_PipelineVisBufShade.pPSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly to the pipeline state object.
    m_PipelineVisBufShade.pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants")->Set(m_GlobalConstants);

    m_PipelineVisBufShade.pPSO->CreateShaderResourceBinding(&m_PipelineVisBufShade.pSRB, true);
}

void KM01_VisibilityBuffer::CreateVisBufVertexCachePSO()
{
    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    const uint32_t threadGroupSize = 32;

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("THREAD_GROUP_SIZE", threadGroupSize);
    Macros.Finalize();

    RefCntAutoPtr<IShader> pCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "VisBufVertexCache CS";
        ShaderCI.FilePath        = "visbuf_vertexcache.csh";
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pCS);
    }

    ComputePipelineStateCreateInfo PSOCreateInfo;
    PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

    PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    // clang-format off
    ShaderResourceVariableDesc Vars[] =
    {
    {SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    {SHADER_TYPE_COMPUTE, "g_VertexData", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    {SHADER_TYPE_COMPUTE, "g_IndexData", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    {SHADER_TYPE_COMPUTE, "g_InstanceData", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    // clang-format on
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    PSODesc.Name      = "VisBufVertexCache PSO";
    PSOCreateInfo.pCS = pCS;

    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_PipelineVisBufVertexCache.pPSO);
    m_PipelineVisBufVertexCache.pPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_GlobalConstants);
    m_PipelineVisBufVertexCache.pPSO->CreateShaderResourceBinding(&m_PipelineVisBufVertexCache.pSRB, true);
}

void KM01_VisibilityBuffer::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
    InstBuffDesc.Mode      = BUFFER_MODE_STRUCTURED;
    InstBuffDesc.Size      = sizeof(float4x4) * MaxInstances;
    InstBuffDesc.ElementByteStride = sizeof(float4);
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

void KM01_VisibilityBuffer::CreateVertexCacheBuffer()
{
    BufferDesc BuffDesc;
    BuffDesc.Name              = "VertexCache";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(CachedMeshVertex);
    BuffDesc.Size              = sizeof(CachedMeshVertex) * 36 * (MAX_GRID_SIZE * MAX_GRID_SIZE * MAX_GRID_SIZE);

    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_VertexCacheBuffer);
}

void KM01_VisibilityBuffer::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt("Grid Size", &m_GridSize, 1, MAX_GRID_SIZE))
        {
            PopulateInstanceBuffer();
        }
    }
    ImGui::End();
}

void KM01_VisibilityBuffer::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Create dynamic uniform buffer that will store our transformation matrix and mesh draw info
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(GlobalConstants), "GlobalConstants", &m_GlobalConstants);

    CreateCubePSO();

    CreateVisBufShadePSO();

    CreateVisBufVertexCachePSO();

    CreateInstanceBuffer();

    CreateVertexCacheBuffer();

    // Load textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_NORM_UV, BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice, BIND_SHADER_RESOURCE, BUFFER_MODE_STRUCTURED);
    m_TextureSRV       = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Set cube texture SRV in the SRB
    m_PipelineCube.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

    // Set cube vertex, index and instance SRV
    m_PipelineCube.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_CachedVertexData")->Set(m_VertexCacheBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    m_PipelineVisBufShade.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_CachedVertexData")->Set(m_VertexCacheBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    m_PipelineVisBufVertexCache.pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_VertexData")->Set(m_CubeVertexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_PipelineVisBufVertexCache.pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_IndexData")->Set(m_CubeIndexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_PipelineVisBufVertexCache.pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_InstanceData")->Set(m_InstanceBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    m_PipelineVisBufVertexCache.pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_CachedVertexData")->Set(m_VertexCacheBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
}

void KM01_VisibilityBuffer::WindowResize(Uint32 Width, Uint32 Height)
{
    const auto& Desc = m_pSwapChain->GetDesc();
    VisibilityBuffer::Create(m_pDevice, m_MainVisBuf, Desc.Width, Desc.Height);

    if (m_MainVisBuf)
    {
        m_PipelineVisBufShade.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_IdTexture")->Set(m_MainVisBuf->m_pIdRTV->GetTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
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
    assert(m_MainVisBuf);

    const uint32_t CUBE_VERTEX_COUNT = 36;
    const uint32_t CUBE_INSTANCE_COUNT = m_GridSize * m_GridSize * m_GridSize;
    const uint32_t CUBE_TOTAL_VERTEX_COUNT = CUBE_VERTEX_COUNT * CUBE_INSTANCE_COUNT;

    // Update Global Constants
    {
        float ScreenX = (float)m_pSwapChain->GetDesc().Width;
        float ScreenY = (float)m_pSwapChain->GetDesc().Height;
        MapHelper<GlobalConstants> CBConstants(m_pImmediateContext, m_GlobalConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0].ViewProj = m_ViewProjMatrix.Transpose();
        CBConstants[0].Rotation = m_RotationMatrix.Transpose();
        CBConstants[0].ScreenSizeInvSize = float4(ScreenX, ScreenY, 1.0f / ScreenX, 1.0f / ScreenY);
        CBConstants[0].MeshDrawInfo = uint4(CUBE_VERTEX_COUNT, CUBE_INSTANCE_COUNT, 0, 0) ;
    }

    // Update Vertex Cache
    {
        DispatchComputeAttribs DispatAttribs;
        DispatAttribs.ThreadGroupCountX = DivideAndRoundUp<uint32_t>(CUBE_TOTAL_VERTEX_COUNT, DISPATCH_THREAD_GROUP_SIZE);

        m_pImmediateContext->SetPipelineState(m_PipelineVisBufVertexCache.pPSO);
        m_pImmediateContext->CommitShaderResources(m_PipelineVisBufVertexCache.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DispatchCompute(DispatAttribs);
    }

    {
        auto pRTV = m_MainVisBuf->m_pIdRTV;
        auto pDSV = m_MainVisBuf->m_pDepthDSV;

        // Clear the visibility buffer
        const float ClearColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


        // No need to bind vertex, instance and index buffers, since we are fetching them in shader


        // Set the pipeline state
        m_pImmediateContext->SetPipelineState(m_PipelineCube.pPSO);
        // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
        // makes sure that resources are transitioned to required states.
        m_pImmediateContext->CommitShaderResources(m_PipelineCube.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs DrawAttrs; // This is an indexed draw call
        DrawAttrs.NumVertices  = CUBE_VERTEX_COUNT;
        DrawAttrs.NumInstances = CUBE_INSTANCE_COUNT; // The number of instances
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
        m_pImmediateContext->SetPipelineState(m_PipelineVisBufShade.pPSO);

        // Commit the render target shader's resources
        m_pImmediateContext->CommitShaderResources(m_PipelineVisBufShade.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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
    const auto AnimTime = CurrTime * 0.125;
    m_RotationMatrix = float4x4::RotationY(static_cast<float>(AnimTime) * 1.0f) * float4x4::RotationX(-static_cast<float>(AnimTime) * 0.25f);
}

} // namespace Diligent
