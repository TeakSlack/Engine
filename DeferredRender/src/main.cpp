#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_buffer.h"
#include "vk_descriptors.h"
#include "pipeline_builder.h"
#include "logger.h"
#include "vk_mesh.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <cstdio>   // only for the --help printf before logger is up

static constexpr u32 INITIAL_WIDTH  = 1280;
static constexpr u32 INITIAL_HEIGHT = 720;

// -------------------------------------------------------------------------
// AppConfig - populated from argv before anything else runs
// -------------------------------------------------------------------------
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
            // Logger not up yet - use fprintf for this one early error
            fprintf(stderr, "Unknown argument: %s  (use --help)\n", arg.c_str());
            exit(1);
        }
    }
#ifdef DEBUG
	cfg.verbose = true; // force verbose in debug builds, regardless of args
#endif

    return cfg;
}

// -------------------------------------------------------------------------
// Command infrastructure - one pool + one primary cmd buffer per frame slot
// -------------------------------------------------------------------------
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
        VK_CHECK(vkCreateCommandPool(ctx.device, &pool_info, nullptr,
                                     &frame_data[i].command_pool));

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = frame_data[i].command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(ctx.device, &alloc_info,
                                          &frame_data[i].command_buffer));

        LOG_DEBUG_TO("render", "Frame slot {} - command pool + buffer allocated", i);
    }
}

static void destroy_command_infrastructure(const VkContext& ctx)
{
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        vkDestroyCommandPool(ctx.device, frame_data[i].command_pool, nullptr);
    LOG_DEBUG_TO("render", "Command infrastructure destroyed");
}

static VkRenderPass create_render_pass(const VkContext& ctx, VkFormat swapchain_format) {
    // One color attachment - the swapchain image we'll present
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // clear on begin
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // keep after end
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // don't care what was there
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // ready to present

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = VK_FORMAT_D32_SFLOAT;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // clear on begin
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // don't need after end
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // don't care what was there
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // ready for depth attachment

    // The subpass references the color attachment by index
    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;  // index into pAttachments array
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref = {};
	depth_ref.attachment = 1; // index into pAttachments array
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref; // required: transitions depth image layout

    // Subpass dependency: wait for the swapchain image to be available
    // before writing color, and signal when color writing is done.
    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;  // implicit subpass before the render pass
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 2;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dep;

    VkRenderPass render_pass;
    VK_CHECK(vkCreateRenderPass(ctx.device, &rp_info, nullptr, &render_pass));
    return render_pass;
}

std::vector<VkFramebuffer> create_framebuffers(
    VkDevice                        device,
    VkRenderPass                    render_pass,
    const std::vector<VkImageView>& color_views,
    VkImageView                     depth_view,    // new
    VkExtent2D                      extent)
{
    std::vector<VkFramebuffer> framebuffers(color_views.size());
    for (u32 i = 0; i < (u32)color_views.size(); ++i) {
        VkImageView attachments[] = { color_views[i], depth_view }; // order matches render pass

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = render_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = attachments;
        fb_info.width = extent.width;
        fb_info.height = extent.height;
        fb_info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffers[i]));
    }
    return framebuffers;
}


static void record_commands(VkCommandBuffer cmd, 
    VkFramebuffer framebuffer, 
    VkRenderPass render_pass, 
    VkPipeline pipeline, 
    VkPipelineLayout pipeline_layout,
	VkDescriptorSet desc_set,
	const Mesh& mesh,
    VkExtent2D extent)
{
    //LOG_VERBOSE("Recording commands for frame slot {}", frame_slot);

    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    // Dark teal - distinguishable from a crash black at a glance
    VkClearValue clear_values[2] = {};
    clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	clear_values[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo rp_begin = {};
	rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp_begin.renderPass = render_pass;
	rp_begin.framebuffer = framebuffer;
	rp_begin.renderArea.offset = { 0, 0 };
	rp_begin.renderArea.extent = extent;
	rp_begin.clearValueCount = 2;
	rp_begin.pClearValues = clear_values;

	vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &desc_set, 0, nullptr);

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertex_buffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (f32)extent.width;
	viewport.height = (f32)extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdDrawIndexed(cmd, mesh.index_count, 1, 0, 0, 0);
    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));
}

VkPipeline create_pipeline(const VkDevice& device, VkRenderPass& render_pass, VkExtent2D extent, VkDescriptorSetLayout& set_layout, VkPipelineLayout& layout)
{
    PipelineBuilder builder(PipelineType::Graphics);

    builder.add_shader_stage("src/shader/cube.vert.spv", vk::ShaderStageFlagBits::eVertex)
        .add_shader_stage("src/shader/cube.frag.spv", vk::ShaderStageFlagBits::eFragment)
        .add_binding_description(Vertex::get_binding_description())
        .add_viewport(0.0f, 0.0f, (f32)extent.width, (f32)extent.height, 0.0f, 1.0f)
        .add_scissor(0, 0, extent.width, extent.height)
        .add_dynamic_state(vk::DynamicState::eViewport)
        .add_dynamic_state(vk::DynamicState::eScissor)
        .set_depth_test(true)
        .set_depth_write(true)
        .set_depth_compare_op(vk::CompareOp::eLess)
        .color_blend_write_all()
		.set_render_pass(render_pass, 0)
        .add_descriptor_set_layout(set_layout);

    for(const auto& desc : Vertex::get_attribute_descriptions())
		builder.add_attribute_description(desc);

    return builder.build(device, layout);
}

// -------------------------------------------------------------------------
// Resize callback
// -------------------------------------------------------------------------
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
    AppConfig cfg = parse_args(argc, argv);

    // Logger is the very first engine system initialised
    Logger::init(cfg.verbose, cfg.log_file);

    LOG_INFO("vk_renderer starting");
    if (cfg.verbose)
        LOG_DEBUG("Verbose mode on, trace/debug messages and source locations visible");
    if (!cfg.log_file.empty())
        LOG_INFO("Logging to file: {}", cfg.log_file);

    // -------- GLFW --------
    LOG_DEBUG("Initialising GLFW");
    if (!glfwInit()) {
        LOG_FATAL("glfwInit() failed");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(
        INITIAL_WIDTH, INITIAL_HEIGHT, "vk_renderer", nullptr, nullptr);
    if (!window) {
        LOG_FATAL("glfwCreateWindow() failed");
        glfwTerminate();
        return 1;
    }
    glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
    LOG_INFO("Window created ({}x{})", INITIAL_WIDTH, INITIAL_HEIGHT);

    // -------- Vulkan init --------
    VkContext   ctx = {};
    VkSwapchain sc  = {};

    vk_context_init(ctx, window);
    vk_swapchain_create(sc, ctx, INITIAL_WIDTH, INITIAL_HEIGHT);
    create_command_infrastructure(ctx);    

	/*

	AllocatedBuffer vertex_buffer = create_vertex_buffer(ctx, vertices.data(), sizeof(Vertex) * vertices.size());
    AllocatedBuffer index_buffer = create_index_buffer(ctx, indices.data(), sizeof(u16) * indices.size());*/

    std::vector<Vertex> verts;
    std::vector<u32>    idxs;
    get_cube_geometry(verts, idxs);
    Mesh cube = create_mesh(ctx, verts, idxs);

    VkRenderPass render_pass = create_render_pass(ctx, sc.format);
	AllocatedImage depth_image = create_depth_image(ctx, sc.extent);

    std::vector<VkFramebuffer> framebuffers = create_framebuffers(
        ctx.device, render_pass, sc.image_views, depth_image.view, sc.extent);

    // Descriptor layout + pool + per-frame UBOs + sets
    VkDescriptorSetLayout set_layout = create_ubo_set_layout(ctx);
    DescriptorAllocator   desc_alloc = create_descriptor_allocator(ctx);

    FrameUBO ubos[MAX_FRAMES_IN_FLIGHT];
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        ubos[i] = create_frame_ubo(ctx);

    auto desc_sets = create_ubo_descriptor_sets(ctx, set_layout, desc_alloc.pool, ubos);

	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipeline = create_pipeline(ctx.device, render_pass, sc.extent, set_layout, layout);

    LOG_INFO("Initialisation complete, entering main loop");

    // -------- Main loop --------
    u64 frame_number = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        if (fb_w == 0 || fb_h == 0) {
            LOG_VERBOSE("Window minimised - skipping frame {}", frame_number);
            continue;
        }

        u32 image_index = vk_swapchain_acquire(sc, ctx);
        if (image_index == UINT32_MAX || g_framebuffer_resized) {
            g_framebuffer_resized = false;
            LOG_INFO_TO("render", "Swapchain out of date - recreating");
            vk_swapchain_recreate(sc, ctx, (u32)fb_w, (u32)fb_h);

            for (auto fb : framebuffers)
                vkDestroyFramebuffer(ctx.device, fb, nullptr);
            vkDestroyRenderPass(ctx.device, render_pass, nullptr);
            vk_destroy_image(ctx.allocator, ctx.device, depth_image);

            render_pass  = create_render_pass(ctx, sc.format);
            depth_image  = create_depth_image(ctx, sc.extent);
            framebuffers = create_framebuffers(ctx.device, render_pass, sc.image_views, depth_image.view, sc.extent);
            continue;
        }

        LOG_VERBOSE("Frame {} - image index {}, frame slot {}",
                    frame_number, image_index, sc.current_frame);

        VkCommandBuffer cmd = frame_data[sc.current_frame].command_buffer;
        vkResetCommandBuffer(cmd, 0);

        float angle = glfwGetTime();

		glm::mat4 model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), sc.extent.width / (float)sc.extent.height, 0.1f, 10.0f);
		proj[1][1] *= -1; // Vulkan NDC Y points down; flip to match GLM's OpenGL convention

		UniformData udata = {};
		udata.model = model;
		udata.view = view;
		udata.proj = proj;
		ubos[sc.current_frame].write(udata);

		// framebuffers are indexed by swapchain image, not by frame-in-flight slot
		record_commands(cmd, framebuffers[image_index],
			render_pass,
			pipeline,
			layout, desc_sets[sc.current_frame], cube, sc.extent);

        bool ok = vk_swapchain_submit_and_present(sc, ctx, cmd, image_index);

        if (!ok) {
            glfwGetFramebufferSize(window, &fb_w, &fb_h);
            LOG_WARN_TO("render", "Present returned out-of-date - recreating swapchain");
            vk_swapchain_recreate(sc, ctx, (u32)fb_w, (u32)fb_h);
            for (auto fb : framebuffers)
                vkDestroyFramebuffer(ctx.device, fb, nullptr);
            vkDestroyRenderPass(ctx.device, render_pass, nullptr);
            vk_destroy_image(ctx.allocator, ctx.device, depth_image);
            render_pass  = create_render_pass(ctx, sc.format);
            depth_image  = create_depth_image(ctx, sc.extent);
            framebuffers = create_framebuffers(ctx.device, render_pass, sc.image_views, depth_image.view, sc.extent);
        }

        ++frame_number;
    }

    LOG_INFO("Main loop exited after {} frames", frame_number);

    // -------- Cleanup --------
    LOG_DEBUG("Waiting for GPU idle before shutdown");

    vkDeviceWaitIdle(ctx.device);

    vkDestroyPipeline(ctx.device, pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, layout, nullptr);
    vkDestroyRenderPass(ctx.device, render_pass, nullptr);
    for (auto fb : framebuffers)
        vkDestroyFramebuffer(ctx.device, fb, nullptr);
    vk_destroy_image(ctx.allocator, ctx.device, depth_image);
    vkDestroyDescriptorSetLayout(ctx.device, set_layout, nullptr);
    destroy_descriptor_allocator(ctx, desc_alloc);
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        destroy_frame_ubo(ctx, ubos[i]);
    destroy_mesh(ctx.allocator, cube);

    destroy_command_infrastructure(ctx);
    vk_swapchain_destroy(sc, ctx);
    vk_context_destroy(ctx);

    glfwDestroyWindow(window);
    glfwTerminate();

    LOG_INFO("Shutdown complete");
    spdlog::shutdown();
    return 0;
}
