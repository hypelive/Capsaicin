#include "virtual_shadow_map.h"
#include "capsaicin_internal.h"
#include "virtual_shadow_map_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"

namespace Capsaicin
{
VirtualShadowMap::VirtualShadowMap()
    : RenderTechnique("Virtual Shadow Map") {}

VirtualShadowMap::~VirtualShadowMap()
{
    VirtualShadowMap::terminate();
}

RenderOptionList VirtualShadowMap::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

VirtualShadowMap::RenderOptions VirtualShadowMap::convertOptions(
    [[maybe_unused]] RenderOptionList const& options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList VirtualShadowMap::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList VirtualShadowMap::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList VirtualShadowMap::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"DepthCopy", SharedTexture::Access::Read});
    textures.push_back({"Debug", SharedTexture::Access::Write});
    return textures;
}

DebugViewList VirtualShadowMap::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("Cascades");
    return views;
}

bool VirtualShadowMap::init([[maybe_unused]] CapsaicinInternal const& capsaicin) noexcept
{
    m_allocationsState = gfxCreateBuffer<AllocationsState>(gfx_, 1);
    m_virtualPageTable = gfxCreateTexture2D(gfx_, PAGE_TABLE_RESOLUTION_UINT,
        PAGE_TABLE_RESOLUTION_UINT, DXGI_FORMAT_R32_UINT);
    constexpr float clearValue = std::numeric_limits<float>::max();
    m_physicalPages = gfxCreateTexture2D(gfx_, CASCADE_RESOLUTION_UINT, CASCADE_RESOLUTION_UINT,
        DXGI_FORMAT_R32_UINT, 1, &clearValue);
    m_debugTexture = gfxCreateTexture2D(gfx_, CASCADE_RESOLUTION_UINT, CASCADE_RESOLUTION_UINT, DXGI_FORMAT_R8G8B8A8_UNORM);

    m_markVisiblePagesProgram = capsaicin.createProgram(
        "render_techniques/virtual_shadow_map/mark_visible_pages");
    m_markVisiblePagesKernel       = gfxCreateComputeKernel(gfx_, m_markVisiblePagesProgram);
    m_allocatePhysicalPagesProgram = capsaicin.createProgram(
        "render_techniques/virtual_shadow_map/allocate_physical_pages");
    m_allocatePhysicalPagesKernel = gfxCreateComputeKernel(gfx_, m_allocatePhysicalPagesProgram);

    GfxDrawState const renderingDrawState = {};
    gfxDrawStateSetCullMode(renderingDrawState, D3D12_CULL_MODE_NONE);

    gfxDrawStateSetColorTarget(
        renderingDrawState, 0, m_debugTexture.getFormat());

    m_renderingProgram            = capsaicin.createProgram(
        "render_techniques/virtual_shadow_map/shadows_rendering");
    m_renderingKernel = gfxCreateMeshKernel(gfx_, m_renderingProgram, renderingDrawState);

    m_debugProgram = capsaicin.createProgram("render_techniques/virtual_shadow_map/debug_draw");
    m_debugKernel  = gfxCreateComputeKernel(gfx_, m_debugProgram);

    return m_markVisiblePagesKernel && m_allocatePhysicalPagesKernel && m_renderingKernel && m_debugKernel;
}

void VirtualShadowMap::render([[maybe_unused]] CapsaicinInternal& capsaicin) noexcept
{
    m_options                 = convertOptions(capsaicin.getOptions());
    auto const& targetTexture = capsaicin.getSharedTexture("Debug");

    const glm::vec2 renderResolution = {targetTexture.getWidth(), targetTexture.getHeight()};
    glm::mat4x4     lightViewProjection;
    auto const&     gpuDrawConstants = capsaicin.allocateConstantBuffer<VSMConstants>(1);
    {
        const auto& cameraMatrices            = capsaicin.getCameraMatrices();
        const auto& camera                    = capsaicin.getCamera();
        const auto& directionalLightDirection = capsaicin.getComponent<CustomLightBuilder>()->
                                                          m_directionalLightDirection;

        const auto defaultLightView = glm::lookAt(glm::vec3{0.0f}, -directionalLightDirection,
            glm::vec3{0.0f, 0.0f, 1.0f});
        const auto lightProjection = glm::ortho(-CASCADE_SIZE_0, CASCADE_SIZE_0, -CASCADE_SIZE_0,
            CASCADE_SIZE_0, -50.0f, 50.0f);

        const auto defaultLightViewProjection = lightProjection * defaultLightView;

        auto cameraPositionNDC = defaultLightViewProjection * glm::vec4{camera.eye, 1.0f};

        glm::vec2 cameraPageOffset =
            glm::ceil(glm::vec2{cameraPositionNDC.x, cameraPositionNDC.y} / PAGE_NDC);
        glm::vec2 alignedCameraPositionNDC        = cameraPageOffset * PAGE_NDC;
        glm::vec4 alignedCameraPositionWorldSpace = glm::inverse(defaultLightViewProjection) * glm::vec4{
                                                        alignedCameraPositionNDC, cameraPositionNDC.z, 1.0f};

        glm::vec3 shadowCameraWorldPosition =
            glm::vec3{alignedCameraPositionWorldSpace} + directionalLightDirection * 5.0f;
        glm::mat4x4 lightView = glm::lookAt(shadowCameraWorldPosition,
            shadowCameraWorldPosition - directionalLightDirection, glm::vec3{0.0f, 0.0f, 1.0f});
        lightViewProjection = lightProjection * lightView;

        VSMConstants drawConstants        = {};
        drawConstants.viewProjection      = cameraMatrices.view_projection;
        drawConstants.invViewProjection   = cameraMatrices.inv_view_projection;
        drawConstants.lightViewProjection = lightViewProjection;
        drawConstants.cameraPosition      = camera.eye;
        drawConstants.screenSize          = glm::vec4{renderResolution.x, renderResolution.y,
                                                      1.0f / renderResolution.x,
                                                      1.0f / renderResolution.y};
        drawConstants.alignedLightPositionNDC = glm::vec4{alignedCameraPositionNDC, cameraPositionNDC.z, 0};
        drawConstants.cameraPageOffset = glm::vec4{cameraPageOffset.x, cameraPageOffset.y, 1.0f, 1.0f};
        drawConstants.viewport = glm::uvec4{renderResolution.x - TILE_SIZE * PAGE_TABLE_RESOLUTION,
                                            renderResolution.y - TILE_SIZE * PAGE_TABLE_RESOLUTION, 0, 0};

        gfxBufferGetData<VSMConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Clear
    {
        gfxCommandClearBuffer(gfx_, m_allocationsState);
        gfxCommandClearTexture(gfx_, m_virtualPageTable);
        gfxCommandClearTexture(gfx_, m_physicalPages);
    }

    // Form new allocation requests.
    {
        gfxProgramSetParameter(gfx_, m_markVisiblePagesProgram, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_markVisiblePagesProgram, "g_DepthCopy",
            capsaicin.getSharedTexture("DepthCopy"));
        gfxProgramSetParameter(gfx_, m_markVisiblePagesProgram, "g_VirtualPageTable", m_virtualPageTable);
        // TODO remove debug
        gfxProgramSetParameter(gfx_, m_markVisiblePagesProgram, "g_TargetTexture", targetTexture);

        gfxCommandBindKernel(gfx_, m_markVisiblePagesKernel);

        glm::uvec2 const groupCount =
            ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    // Allocate required pages.
    {
        gfxProgramSetParameter(gfx_, m_allocatePhysicalPagesProgram, "g_AllocationsState",
            m_allocationsState);
        gfxProgramSetParameter(gfx_, m_allocatePhysicalPagesProgram, "g_VirtualPageTable",
            m_virtualPageTable);

        gfxCommandBindKernel(gfx_, m_allocatePhysicalPagesKernel);
        const glm::uvec2 groupCount = {ceil(PAGE_TABLE_RESOLUTION / TILE_SIZE),
                                       ceil(PAGE_TABLE_RESOLUTION / TILE_SIZE)};
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    // Render shadow maps.
    {
        // Prepare draw data.
        std::vector<DrawData> drawData;
        for (auto const& index : capsaicin.getInstanceIdData())
        {
            Instance const& instance = capsaicin.getInstanceData()[index];

            for (uint32_t j = 0; j < instance.meshlet_count; ++j)
            {
                drawData.emplace_back(instance.meshlet_offset_idx + j, index);
            }
        }

        const uint32_t drawDataSize = static_cast<uint32_t>(drawData.size());
        gfxDestroyBuffer(gfx_, m_drawDataBuffer);
        m_drawDataBuffer = gfxCreateBuffer<DrawData>(gfx_, drawDataSize, drawData.data());
    

        // Filling the draw constants.
        RenderingConstants drawConstants = {};
        drawConstants.viewProjection     = lightViewProjection;
        drawConstants.drawCount          = drawDataSize;

        gfxDestroyBuffer(gfx_, m_drawConstantsBuffer);
        m_drawConstantsBuffer = gfxCreateBuffer<RenderingConstants>(gfx_, 1, &drawConstants);
    

        // Set the root parameters.
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_DrawConstants", m_drawConstantsBuffer);
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_DrawDataBuffer", m_drawDataBuffer);
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_VirtualPageTable", m_virtualPageTable);
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_PhysicalPages", m_physicalPages);

        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_InstanceBuffer",
            capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_TransformBuffer",
            capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_MeshletBuffer",
            capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_MeshletPackBuffer",
            capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_VertexBuffer",
            capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_VertexDataIndex",
            capsaicin.getVertexDataIndex());
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_MaterialBuffer",
            capsaicin.getMaterialBuffer());

        auto const& textures = capsaicin.getTextures();
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_TextureMaps", textures.data(),
            (uint32_t)textures.size());
        gfxProgramSetParameter(gfx_, m_renderingProgram, "g_LinearSampler",
            capsaicin.getLinearSampler());
    

        // Run the amplification shader.
        //gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandSetViewport(gfx_, 0, 0, CASCADE_RESOLUTION, CASCADE_RESOLUTION);
        gfxCommandBindColorTarget(gfx_, 0, m_debugTexture);
        gfxCommandBindKernel(gfx_, m_renderingKernel);

        uint32_t const* num_threads  = gfxKernelGetNumThreads(gfx_, m_renderingKernel);
        uint32_t const  num_groups_x = (drawDataSize + num_threads[0] - 1) / num_threads[0];

        gfxCommandDrawMesh(gfx_, num_groups_x, 1, 1);

        gfxCommandSetViewport(gfx_);
    }

    // Debug
    {
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_DepthCopy", capsaicin.getSharedTexture("DepthCopy"));
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_TargetTexture", targetTexture);
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_VirtualPageTable", m_virtualPageTable);

        gfxCommandBindKernel(gfx_, m_debugKernel);

        gfxCommandDispatch(gfx_, static_cast<uint32_t>(PAGE_TABLE_RESOLUTION),
            static_cast<uint32_t>(PAGE_TABLE_RESOLUTION), 1);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void VirtualShadowMap::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_debugKernel);
    m_debugKernel = {};
    gfxDestroyProgram(gfx_, m_debugProgram);
    m_debugProgram = {};
}

void VirtualShadowMap::renderGUI([[maybe_unused]] CapsaicinInternal& capsaicin) const noexcept { }
} // namespace Capsaicin
