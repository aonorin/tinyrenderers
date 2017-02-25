#define LC_IMAGE_IMPLEMENTATION
#include "lc_image.h"

#define TINYCI_IMPLEMENTATION
#include "tinyci.hpp"
using namespace ci;
using namespace ci::app;
#define TINY_RENDERER_IMPLEMENTATION

#include "tinyci_renderer.hpp"
#if defined(TINY_RENDERER_DX)
    #include "tinydx.h"
#elif defined(TINY_RENDERER_VK)
    #include "tinyvk.h"
#endif

const uint32_t kImageCount = 3;

class TextureApp : public App {
public:
    void setup() override;
    void cleanup() override;
    void mouseDown( MouseEvent event ) override;
    void update() override;
    void draw() override;

private:
    tr_renderer*        m_renderer = nullptr;
    tr_descriptor_set*  m_desc_set = nullptr;
    tr_cmd_pool*        m_cmd_pool = nullptr;
    tr_cmd**            m_cmds = nullptr;
    tr_shader_program*  m_shader = nullptr;
    tr_buffer*          m_rect_index_buffer = nullptr;
    tr_buffer*          m_rect_vertex_buffer = nullptr;
    tr_pipeline*        m_pipeline = nullptr;
    tr_texture*         m_texture = nullptr;
    tr_sampler*         m_sampler = nullptr;
};

void renderer_log(tr_log_type type, const char* msg, const char* component)
{
    switch(type) {
        //case tr_log_type_info  : {CI_LOG_I("[" << component << "] : " << msg);} break;
        //case tr_log_type_warn  : {CI_LOG_W("[" << component << "] : " << msg);} break;
        //case tr_log_type_debug : {CI_LOG_E("[" << component << "] : " << msg);} break;
        //case tr_log_type_error : {CI_LOG_D("[" << component << "] : " << msg);} break;
    }
}

#if defined(TINY_RENDERER_VK)
VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug(
    VkDebugReportFlagsEXT      flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t                   object,
    size_t                     location,
    int32_t                    messageCode,
    const char*                pLayerPrefix,
    const char*                pMessage,
    void*                      pUserData
)
{
    if( flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT ) {
        //CI_LOG_I( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    else if( flags & VK_DEBUG_REPORT_WARNING_BIT_EXT ) {
        //CI_LOG_W( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    else if( flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ) {
        //CI_LOG_I( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    else if( flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ) {
        //CI_LOG_E( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    else if( flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT ) {
        //CI_LOG_D( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    return VK_FALSE;
}
#endif

void TextureApp::setup()
{
    addAssetDirectory("../../..");

    std::vector<const char*> instance_layers = {
#if defined(_DEBUG)
        "VK_LAYER_LUNARG_api_dump",
        "VK_LAYER_LUNARG_core_validation",
        "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_parameter_validation"
#endif
    };

    std::vector<const char*> device_layers = {
#if defined(_DEBUG)
        "VK_LAYER_LUNARG_api_dump",
        "VK_LAYER_LUNARG_core_validation",
        "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_parameter_validation"
#endif
    };

    tr_renderer_settings settings = {0};
    settings.handle.hinstance               = static_cast<TinyRenderer*>(getRenderer().get())->getHinstance();
    settings.handle.hwnd                    = static_cast<TinyRenderer*>(getRenderer().get())->getHwnd();
    settings.width                          = getWindowWidth();
    settings.height                         = getWindowHeight();
    settings.swapchain.image_count          = kImageCount;
    settings.swapchain.sample_count         = tr_sample_count_8;
    settings.swapchain.color_format         = tr_format_b8g8r8a8_unorm;
    settings.swapchain.depth_stencil_format = tr_format_undefined;
    settings.log_fn                         = renderer_log;
#if defined(TINY_RENDERER_VK)
    settings.vk_debug_fn                    = vulkan_debug;
    settings.instance_layers.count          = static_cast<uint32_t>(instance_layers.size());
    settings.instance_layers.names          = instance_layers.empty() ? nullptr : instance_layers.data();
    settings.device_layers.count            = static_cast<uint32_t>(device_layers.size());
    settings.device_layers.names            = device_layers.data();
#endif
    tr_create_renderer("ColorApp", &settings, &m_renderer);

    tr_create_cmd_pool(m_renderer, m_renderer->graphics_queue, false, &m_cmd_pool);
    tr_create_cmd_n(m_cmd_pool, false, kImageCount, &m_cmds);
    
#if defined(TINY_RENDERER_VK)
    auto vert = loadFile(getAssetPath("texture_vert.spv"))->getBuffer();
    auto frag = loadFile(getAssetPath("texture_frag.spv"))->getBuffer();
    tr_create_shader_program(m_renderer, 
                             vert->getSize(), (uint32_t*)(vert->getData()), "main", 
                             frag->getSize(), (uint32_t*)(frag->getData()), "main", &m_shader);
#elif defined(TINY_RENDERER_DX)
    auto hlsl = loadFile(getAssetPath("texture.hlsl"))->getBuffer();    
    tr_create_shader_program(m_renderer, 
                             hlsl->getSize(), hlsl->getData(), "VSMain", 
                             hlsl->getSize(), hlsl->getData(), "PSMain", &m_shader);
#endif

    std::vector<tr_descriptor> descriptors(2);
    descriptors[0].type          = tr_descriptor_type_texture;
    descriptors[0].count         = 1;
    descriptors[0].binding       = 0;
    descriptors[0].shader_stages = tr_shader_stage_frag;
    descriptors[1].type          = tr_descriptor_type_sampler;
    descriptors[1].count         = 1;
    descriptors[1].binding       = 1;
    descriptors[1].shader_stages = tr_shader_stage_frag;
    tr_create_descriptor_set(m_renderer, descriptors.size(), descriptors.data(), &m_desc_set);

    tr_vertex_layout vertex_layout = {};
    vertex_layout.attrib_count = 2;
    vertex_layout.attribs[0].semantic = tr_semantic_position;
    vertex_layout.attribs[0].format   = tr_format_r32g32b32a32_float;
    vertex_layout.attribs[0].binding  = 0;
    vertex_layout.attribs[0].location = 0;
    vertex_layout.attribs[0].offset   = 0;
    vertex_layout.attribs[1].semantic = tr_semantic_texcoord0;
    vertex_layout.attribs[1].format   = tr_format_r32g32_float;
    vertex_layout.attribs[1].binding  = 0;
    vertex_layout.attribs[1].location = 1;
    vertex_layout.attribs[1].offset   = tr_util_format_stride(tr_format_r32g32b32a32_float);
    tr_pipeline_settings pipeline_settings = {tr_primitive_topo_tri_list};
    tr_create_pipeline(m_renderer, m_shader, &vertex_layout, m_desc_set, m_renderer->swapchain_render_targets[0], &pipeline_settings, &m_pipeline);

    std::vector<float> vertexData = {
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f,
    };

#if defined(TINY_RENDERER_DX)
    // Flip the y so they're the same in both renderer
    vertexData[6*0 + 1] *= -1.0f;
    vertexData[6*1 + 1] *= -1.0f;
    vertexData[6*2 + 1] *= -1.0f;
    vertexData[6*3 + 1] *= -1.0f;
#endif

    uint64_t vertexDataSize = sizeof(float) * vertexData.size();
    uint64_t vertexStride = sizeof(float) * 6;
    tr_create_vertex_buffer(m_renderer, vertexDataSize, true, vertexStride, &m_rect_vertex_buffer);
    memcpy(m_rect_vertex_buffer->cpu_mapped_address, vertexData.data(), vertexDataSize);
        
    std::vector<uint16_t> indexData = {
        0, 1, 2,
        0, 2, 3
    };
        
    uint64_t indexDataSize = sizeof(uint16_t) * indexData.size();
    tr_create_index_buffer(m_renderer, indexDataSize, true, tr_index_type_uint16, &m_rect_index_buffer);
    memcpy(m_rect_index_buffer->cpu_mapped_address, indexData.data(), indexDataSize);

    Surface surf = loadImage(getAssetPath("box_panel.jpg"));
    tr_create_texture_2d(m_renderer, surf.getWidth(), surf.getHeight(), tr_sample_count_1, tr_format_r8g8b8a8_unorm, tr_max_mip_levels, NULL, false, tr_texture_usage_sampled_image, &m_texture);
    tr_util_update_texture_uint8(m_renderer->graphics_queue, surf.getWidth(), surf.getHeight(), surf.getRowBytes(), surf.getData(), surf.hasAlpha() ? 4 : 3, m_texture, NULL, NULL);
    
    tr_create_sampler(m_renderer, &m_sampler);

    m_desc_set->descriptors[0].textures[0] = m_texture;
    m_desc_set->descriptors[1].samplers[0] = m_sampler;
    tr_update_descriptor_set(m_renderer, m_desc_set);
}

void TextureApp::cleanup()
{
    if (nullptr != m_renderer) {
        tr_destroy_renderer(m_renderer);
    }
}

void TextureApp::mouseDown( MouseEvent event )
{
}

void TextureApp::update()
{
}

void TextureApp::draw()
{
    uint32_t frameIdx = getElapsedFrames() % m_renderer->settings.swapchain.image_count;

    tr_fence* image_acquired_fence = m_renderer->image_acquired_fences[frameIdx];
    tr_semaphore* image_acquired_semaphore = m_renderer->image_acquired_semaphores[frameIdx];
    tr_semaphore* render_complete_semaphores = m_renderer->render_complete_semaphores[frameIdx];

    tr_acquire_next_image(m_renderer, image_acquired_semaphore, image_acquired_fence);

    uint32_t swapchain_image_index = m_renderer->swapchain_image_index;
    tr_render_target* render_target = m_renderer->swapchain_render_targets[swapchain_image_index];

    tr_cmd* cmd = m_cmds[frameIdx];

    tr_begin_cmd(cmd);
    tr_cmd_render_target_transition(cmd, render_target, tr_texture_usage_present, tr_texture_usage_color_attachment); 
    tr_cmd_set_viewport(cmd, 0, 0, getWindowWidth(), getWindowHeight(), 0.0f, 1.0f);
    tr_cmd_set_scissor(cmd, 0, 0, getWindowWidth(), getWindowHeight());
    tr_cmd_begin_render(cmd, render_target);
    tr_clear_value clear_value = {0.0f, 0.0f, 0.0f, 0.0f};
    tr_cmd_clear_color_attachment(cmd, 0, &clear_value);
    tr_cmd_bind_pipeline(cmd, m_pipeline);
    tr_cmd_bind_index_buffer(cmd, m_rect_index_buffer);
    tr_cmd_bind_vertex_buffers(cmd, 1, &m_rect_vertex_buffer);
    tr_cmd_bind_descriptor_sets(cmd, m_pipeline, m_desc_set);
    tr_cmd_draw_indexed(cmd, 6, 0);
    tr_cmd_end_render(cmd);
    tr_cmd_render_target_transition(cmd, render_target, tr_texture_usage_color_attachment, tr_texture_usage_present); 
    tr_end_cmd(cmd);

    tr_queue_submit(m_renderer->graphics_queue, 1, &cmd, 1, &image_acquired_semaphore, 1, &render_complete_semaphores);
    tr_queue_present(m_renderer->present_queue, 1, &render_complete_semaphores);

    tr_queue_wait_idle(m_renderer->graphics_queue);
}

CINDER_APP(TextureApp, TinyRenderer)
