#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_buffer.h"
#include "vk_mesh.h"
#include "vk_frame.h"
#include "vk_material.h"
#include "vk_texture.h"
#include "vk_renderable.h"
#include "vk_pipeline.h"
#include "logger.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tiny_obj_loader.h>
#include <string>
#include <unordered_map>
#include <cstdio>   // only for the --help printf before logger is up

static constexpr u32 INITIAL_WIDTH  = 1280;
static constexpr u32 INITIAL_HEIGHT = 720;

// =========================================================================
// AppConfig - populated from argv before anything else runs
// =========================================================================
struct AppConfig {
    bool        verbose  = false;
    std::string log_file = "";
};

static AppConfig parse_args(int argc, char** argv)
{
    AppConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--verbose" || arg == "-v") {
            cfg.verbose = true;

        } else if ((arg == "--log-file" || arg == "-l") && i + 1 < argc) {
            cfg.log_file = argv[++i];

        } else if (arg == "--help" || arg == "-h") {
            printf(
                "Usage: vk_renderer [options]\n"
                "\n"
                "Options:\n"
                "  -v, --verbose           Show trace/debug messages and source locations\n"
                "  -l, --log-file <path>   Mirror all output to a log file (no color codes)\n"
                "  -h, --help              Print this message\n"
            );
            exit(0);

        } else {
            fprintf(stderr, "Unknown argument: %s  (use --help)\n", arg.c_str());
            exit(1);
        }
    }
#ifdef DEBUG
    cfg.verbose = true; // force verbose in debug builds
#endif
    return cfg;
}

// =========================================================================
// Command infrastructure - one pool + one primary cmd buffer per frame slot
// =========================================================================
struct FrameData {
    VkCommandPool   command_pool   = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
};

static FrameData frame_data[MAX_FRAMES_IN_FLIGHT] = {};

static void create_command_infrastructure(const VkContext& ctx)
{
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = ctx.queue_families.graphics;
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(ctx.device, &pool_info, nullptr, &frame_data[i].command_pool));

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = frame_data[i].command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(ctx.device, &alloc_info, &frame_data[i].command_buffer));

        LOG_DEBUG_TO("render", "Frame slot {} - command pool + buffer allocated", i);
    }
}

static void destroy_command_infrastructure(const VkContext& ctx)
{
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        vkDestroyCommandPool(ctx.device, frame_data[i].command_pool, nullptr);
    LOG_DEBUG_TO("render", "Command infrastructure destroyed");
}

// =========================================================================
// Render pass - color attachment (swapchain) + depth attachment
// =========================================================================
static VkRenderPass create_render_pass(const VkContext& ctx, VkFormat swapchain_format)
{
    VkAttachmentDescription color_attachment = {};
    color_attachment.format         = swapchain_format;
    color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format         = VK_FORMAT_D32_SFLOAT;
    depth_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;
    color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref = {};
    depth_ref.attachment = 1;
    depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dep = {};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };
    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 2;
    rp_info.pAttachments    = attachments;
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &dep;

    VkRenderPass render_pass;
    VK_CHECK(vkCreateRenderPass(ctx.device, &rp_info, nullptr, &render_pass));
    return render_pass;
}

// =========================================================================
// Framebuffers - one per swapchain image, each with color + depth views
// =========================================================================
static std::vector<VkFramebuffer> create_framebuffers(
    VkDevice                        device,
    VkRenderPass                    render_pass,
    const std::vector<VkImageView>& color_views,
    VkImageView                     depth_view,
    VkExtent2D                      extent)
{
    std::vector<VkFramebuffer> framebuffers(color_views.size());
    for (u32 i = 0; i < (u32)color_views.size(); ++i) {
        VkImageView attachments[] = { color_views[i], depth_view };

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass      = render_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments    = attachments;
        fb_info.width           = extent.width;
        fb_info.height          = extent.height;
        fb_info.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffers[i]));
    }
    return framebuffers;
}

// =========================================================================
// Command recording
// =========================================================================
static void record_commands(
    VkCommandBuffer    cmd,
    VkFramebuffer      framebuffer,
    VkRenderPass       render_pass,
    VkPipeline         pipeline,
    VkPipelineLayout   pipeline_layout,
    VkDescriptorSet    frame_set,      // set 0 - per-frame camera UBO
    const IRenderable& obj,
    u32                frame_index,
    VkExtent2D         extent)
{
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    VkClearValue clear_values[2] = {};
    clear_values[0].color        = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
    clear_values[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rp_begin = {};
    rp_begin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass        = render_pass;
    rp_begin.framebuffer       = framebuffer;
    rp_begin.renderArea.offset = { 0, 0 };
    rp_begin.renderArea.extent = extent;
    rp_begin.clearValueCount   = 2;
    rp_begin.pClearValues      = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind set 0 (camera UBO) once for the whole pass
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout, 0, 1, &frame_set, 0, nullptr);

    VkViewport viewport = {};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (f32)extent.width;
    viewport.height   = (f32)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // RenderObject binds set 1, pushes constants, binds VBs/IB, and draws
    DrawContext draw_ctx;
    draw_ctx.cmd             = cmd;
    draw_ctx.pipeline_layout = pipeline_layout;
    draw_ctx.frame_index     = frame_index;
    obj.draw(draw_ctx);

    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));
}

// =========================================================================
// Swapchain resource recreation
// =========================================================================
static void recreate_swapchain_resources(
    VkContext&                  ctx,
    VkSwapchain&                sc,
    std::vector<VkFramebuffer>& framebuffers,
    VkRenderPass&               render_pass,
    AllocatedImage&             depth_image,
    u32 w, u32 h)
{
    vk_swapchain_recreate(sc, ctx, w, h);

    for (auto fb : framebuffers)
        vkDestroyFramebuffer(ctx.device, fb, nullptr);
    vkDestroyRenderPass(ctx.device, render_pass, nullptr);
    vk_destroy_image(ctx.allocator, ctx.device, depth_image);

    render_pass  = create_render_pass(ctx, sc.format);
    depth_image  = create_depth_image(ctx, sc.extent);
    framebuffers = create_framebuffers(ctx.device, render_pass, sc.image_views, depth_image.view, sc.extent);
}

// =========================================================================
// Resize callback
// =========================================================================
static bool g_framebuffer_resized = false;
static void framebuffer_resize_callback(GLFWwindow*, int w, int h)
{
    g_framebuffer_resized = true;
    LOG_DEBUG_TO("render", "Framebuffer resize detected: {}x{}", w, h);
}

// =========================================================================
// main
// =========================================================================
int main(int argc, char** argv)
{
   // AppConfig cfg = parse_args(argc, argv);
   // Logger::init(cfg.verbose, cfg.log_file);

   // LOG_INFO("vk_renderer starting");
   // if (cfg.verbose)
   //     LOG_DEBUG("Verbose mode on");
   // if (!cfg.log_file.empty())
   //     LOG_INFO("Logging to file: {}", cfg.log_file);

   // // ---- GLFW ----
   // LOG_DEBUG("Initialising GLFW");
   // if (!glfwInit()) {
   //     LOG_FATAL("glfwInit() failed");
   //     return 1;
   // }

   // glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
   // glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

   // GLFWwindow* window = glfwCreateWindow(
   //     INITIAL_WIDTH, INITIAL_HEIGHT, "vk_renderer", nullptr, nullptr);
   // if (!window) {
   //     LOG_FATAL("glfwCreateWindow() failed");
   //     glfwTerminate();
   //     return 1;
   // }
   // glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
   // LOG_INFO("Window created ({}x{})", INITIAL_WIDTH, INITIAL_HEIGHT);

   // // ---- Vulkan context + swapchain ----
   // VkContext   ctx = {};
   // VkSwapchain sc  = {};

   // vk_context_init(ctx, window);
   // vk_swapchain_create(sc, ctx, INITIAL_WIDTH, INITIAL_HEIGHT);
   // create_command_infrastructure(ctx);

   // // ---- Render pass + depth image + framebuffers ----
   // VkRenderPass               render_pass  = create_render_pass(ctx, sc.format);
   // AllocatedImage             depth_image  = create_depth_image(ctx, sc.extent);
   // std::vector<VkFramebuffer> framebuffers = create_framebuffers(
   //     ctx.device, render_pass, sc.image_views, depth_image.view, sc.extent);

   // // ---- Per-frame resources (set 0: camera UBO) ----
   // VkDescriptorSetLayout frame_set_layout = vk_create_frame_set_layout(ctx.device);
   // VkDescriptorPool      frame_pool       = vk_create_frame_descriptor_pool(ctx.device);
   // FrameResources        frames[MAX_FRAMES_IN_FLIGHT] = {};
   // vk_create_frame_resources(ctx.allocator, ctx.device, frame_pool, frame_set_layout, frames);

   // // ---- Pipeline layout + graphics pipeline ----
   // VkPipelineLayout pipeline_layout =
   //     vk_create_pipeline_layout(ctx.device, frame_set_layout, mat_set_layout);

   // VkShaderModule vert_mod = vk_load_shader(ctx.device, "src/shader/cube.vert.spv");
   // VkShaderModule frag_mod = vk_load_shader(ctx.device, "src/shader/cube.frag.spv");

   // PipelineCreateInfo pipe_info = {};
   // pipe_info.render_pass     = render_pass;
   // pipe_info.layout          = pipeline_layout;
   // pipe_info.vert_module     = vert_mod;
   // pipe_info.frag_module     = frag_mod;
   // pipe_info.viewport_extent = sc.extent;
   // pipe_info.material_type   = MaterialType::PBROpaque;

   // VkPipeline pipeline = vk_create_graphics_pipeline(ctx.device, pipe_info);

   // // Shader modules are no longer needed after pipeline creation
   // vk_destroy_shader(ctx.device, vert_mod);
   // vk_destroy_shader(ctx.device, frag_mod);

   // LOG_INFO("Initialisation complete, entering main loop");

   // // =========================================================================
   // // Main loop
   // // =========================================================================
   // u64 frame_number = 0;

   // while (!glfwWindowShouldClose(window)) {
   //     glfwPollEvents();

   //     int fb_w = 0, fb_h = 0;
   //     glfwGetFramebufferSize(window, &fb_w, &fb_h);
   //     if (fb_w == 0 || fb_h == 0)
   //         continue; // window is minimized

   //     u32 image_index = vk_swapchain_acquire(sc, ctx);
   //     if (image_index == UINT32_MAX || g_framebuffer_resized) {
   //         g_framebuffer_resized = false;
   //         LOG_INFO_TO("render", "Swapchain out of date - recreating");
   //         recreate_swapchain_resources(
   //             ctx, sc, framebuffers, render_pass, depth_image, (u32)fb_w, (u32)fb_h);
   //         continue;
   //     }

   //     // Rotate cube over time
   //     float     angle       = (float)glfwGetTime();
   //     glm::mat4 orientation = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
   //     static glm::mat4 spin        = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),  glm::vec3(0.0f, 1.0f, 0.0f));
   //     cube_obj.transform    = spin * orientation;

   //     if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
			//spin = glm::rotate(spin, glm::radians(-1.f), glm::vec3(0.0f, 1.0f, 0.0f));

   //     if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
   //         spin = glm::rotate(spin, glm::radians(1.f), glm::vec3(0.0f, 1.0f, 0.0f));

   //     // Write camera data for this frame slot
   //     glm::vec3 eye  = glm::vec3(2.0f, 2.0f, 2.0f);
   //     glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   //     glm::mat4 proj = glm::perspective(
   //         glm::radians(45.0f), sc.extent.width / (f32)sc.extent.height, 0.1f, 10.0f);
   //     proj[1][1] *= -1; // Vulkan NDC Y points down; flip to match GLM's OpenGL convention

			//

   //     CameraData* cam = frames[sc.current_frame].camera_mapped;
   //     cam->view       = view;
   //     cam->proj       = proj;
   //     cam->view_proj  = proj * view;
   //     cam->camera_pos = eye;

   //     // Record and submit
   //     VkCommandBuffer cmd = frame_data[sc.current_frame].command_buffer;
   //     vkResetCommandBuffer(cmd, 0);
   //     record_commands(cmd, framebuffers[image_index],
   //         render_pass, pipeline, pipeline_layout,
   //         frames[sc.current_frame].frame_set,
   //         cube_obj, sc.current_frame, sc.extent);

   //     bool ok = vk_swapchain_submit_and_present(sc, ctx, cmd, image_index);
   //     if (!ok) {
   //         glfwGetFramebufferSize(window, &fb_w, &fb_h);
   //         LOG_WARN_TO("render", "Present returned out-of-date - recreating swapchain");
   //         recreate_swapchain_resources(
   //             ctx, sc, framebuffers, render_pass, depth_image, (u32)fb_w, (u32)fb_h);
   //     }

   //     ++frame_number;
   // }

   // LOG_INFO("Main loop exited after {} frames", frame_number);

   // // =========================================================================
   // // Cleanup
   // // =========================================================================
   // vkDeviceWaitIdle(ctx.device);

   // vkDestroyPipeline(ctx.device, pipeline, nullptr);
   // vkDestroyPipelineLayout(ctx.device, pipeline_layout, nullptr);
   // vkDestroyRenderPass(ctx.device, render_pass, nullptr);
   // for (auto fb : framebuffers)
   //     vkDestroyFramebuffer(ctx.device, fb, nullptr);
   // vk_destroy_image(ctx.allocator, ctx.device, depth_image);

   // destroy_texture(ctx, cube_tex);
   // vkDestroySampler(ctx.device, sampler, nullptr);
   // vkDestroyDescriptorPool(ctx.device, mat_pool, nullptr);
   // vkDestroyDescriptorSetLayout(ctx.device, mat_set_layout, nullptr);

   // vk_destroy_frame_resources(ctx.allocator, frames);
   // vkDestroyDescriptorPool(ctx.device, frame_pool, nullptr);
   // vkDestroyDescriptorSetLayout(ctx.device, frame_set_layout, nullptr);

   // vk_destroy_mesh(ctx.allocator, cube);
   // destroy_command_infrastructure(ctx);
   // vk_swapchain_destroy(sc, ctx);
   // vk_context_destroy(ctx);

   // glfwDestroyWindow(window);
   // glfwTerminate();

   // LOG_INFO("Shutdown complete");
   // spdlog::shutdown();
    return 0;
}
