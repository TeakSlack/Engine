#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_buffer.h"
#include "logger.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <cstdio>   // only for the --help printf before logger is up
#include <fstream>

static constexpr u32 INITIAL_WIDTH  = 1280;
static constexpr u32 INITIAL_HEIGHT = 720;

// Basic vertex
struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    // Defines how the data is passed to the vertex shader
    static VkVertexInputBindingDescription get_binding_description()
    {
        // Defines the rate to load data from memory
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;				// Binding: only one--all per-vertex data is in one array
		bindingDescription.stride = sizeof(Vertex);	// Stride: number of bytes from one entry to the next
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Input rate: either vertex or instance for instanced rendering

        return bindingDescription;
    }

    // Describes how to retrieve vertex attribute from a chunk of vertex data
    static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // For pos (vec2)
        attributeDescriptions[0].binding = 0;							// Which binding does this come from?
        attributeDescriptions[0].location = 0;							// References (location = 0) in vtex shader
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;	    // Type of data for attribute, vec2, specified using same enum as colors
        attributeDescriptions[0].offset = offsetof(Vertex, pos);		// Specifies number of bytes since start of per-vertex data

        // For color (vec3)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;	    // Type of data for attribute, vec2, specified using same enum as colors
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

// Vertex data for a quad (4 unique vertices with color)
const std::vector<Vertex> vertices =
{
    { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } }, // Bottom left - Red
    { {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } }, // Bottom right - Green
    { {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } }, // Top right - Blue
    { { -0.5f,  0.5f }, { 1.0f, 1.0f, 0.0f } }  // Top left - Yellow
};

// Index data defining two triangles from the 4 vertices
const std::vector<u16> indices =
{
    0, 1, 2,  // First triangle (bottom left -> bottom right -> top right)
    2, 3, 0   // Second triangle (top right -> top left -> bottom left)
};

// -------------------------------------------------------------------------
// AppConfig — populated from argv before anything else runs
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
            // Logger not up yet — use fprintf for this one early error
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
// Command infrastructure — one pool + one primary cmd buffer per frame slot
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

        LOG_DEBUG_TO("render", "Frame slot {} — command pool + buffer allocated", i);
    }
}

static void destroy_command_infrastructure(const VkContext& ctx)
{
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        vkDestroyCommandPool(ctx.device, frame_data[i].command_pool, nullptr);
    LOG_DEBUG_TO("render", "Command infrastructure destroyed");
}

// Reads a binary file (e.g., SPIR-V shader) into a byte buffer.
std::vector<char> read_file(const std::string& fileName)
{
    std::ifstream file(fileName, std::ifstream::ate | std::ifstream::binary);

    if (!file.is_open())
    {
		LOG_ERROR_TO("render", "Failed to open file: {}", fileName);
        exit(EXIT_FAILURE);
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

// Creates a Vulkan shader module from SPIR-V bytecode.
VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const u32*>(code.data());
    VkShaderModule mod;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &mod));
    return mod;
}

VkRenderPass create_render_pass(VkDevice device, VkFormat swapchain_format) {
    // One color attachment — the swapchain image we'll present
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // clear on begin
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // keep after end
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // don't care what was there
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // ready to present

    // The subpass references the color attachment by index
    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;  // index into pAttachments array
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    // Subpass dependency: wait for the swapchain image to be available
    // before writing color, and signal when color writing is done.
    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;  // implicit subpass before the render pass
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &color_attachment;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dep;

    VkRenderPass render_pass;
    VK_CHECK(vkCreateRenderPass(device, &rp_info, nullptr, &render_pass));
    return render_pass;
}

static std::vector<VkFramebuffer> create_framebuffers(
    VkDevice                         device,
    VkRenderPass                     render_pass,
    const std::vector<VkImageView>& image_views,
    VkExtent2D                       extent)
{
    std::vector<VkFramebuffer> framebuffers(image_views.size());
    for (u32 i = 0; i < (u32)image_views.size(); ++i) {
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = render_pass;  // must be compatible with the render pass
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = &image_views[i];
        fb_info.width = extent.width;
        fb_info.height = extent.height;
        fb_info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffers[i]));
    }
    return framebuffers;
}

static VkPipeline create_triangle_pipeline(
    VkDevice      device,
    VkRenderPass  render_pass,
    VkExtent2D    extent,
    VkShaderModule vert_mod,
    VkShaderModule frag_mod,
    VkPipelineLayout& out_layout)  // we create this too
{
    // --- shader stages ---
    VkPipelineShaderStageCreateInfo vert_stage = {};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_mod;
    vert_stage.pName = "main";  // entry point

    VkPipelineShaderStageCreateInfo frag_stage = vert_stage;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_mod;

    VkPipelineShaderStageCreateInfo stages[] = { vert_stage, frag_stage };

    // --- vertex input: none (positions hardcoded in shader) ---
    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	auto binding_description = Vertex::get_binding_description();
	vertex_input.pVertexBindingDescriptions = &binding_description;
	auto attribute_descriptions = Vertex::get_attribute_descriptions();
	vertex_input.vertexAttributeDescriptionCount = (u32)attribute_descriptions.size();
	vertex_input.pVertexAttributeDescriptions = attribute_descriptions.data();

    // --- input assembly: triangles ---
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // --- viewport and scissor (static, matched to swapchain) ---
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

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // --- dynamic state ---
	std::vector<VkDynamicState> dynamic_states = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.dynamicStateCount = (u32)dynamic_states.size();
	dynamic_state_info.pDynamicStates = dynamic_states.data();

    // --- rasterizer ---
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // --- multisampling: off ---
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --- color blend: no blending, write all channels ---
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // --- pipeline layout: empty (no descriptor sets or push constants yet) ---
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VK_CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &out_layout));

    // --- assemble the pipeline ---
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;  // no depth test yet
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state_info;  // all state is static
    pipeline_info.layout = out_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;  // index of the subpass this pipeline is used in

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
        &pipeline_info, nullptr, &pipeline));
    return pipeline;
}

static AllocatedBuffer create_vertex_buffer(const VkContext& ctx, const void* vertex_data, VkDeviceSize data_size)
{
	AllocatedBuffer staging = vk_create_staging_buffer(
		ctx.allocator,
		vertex_data,
        data_size);

	AllocatedBuffer vertex_buffer = vk_create_buffer(
		ctx.allocator,
		data_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    
	vk_copy_buffer(ctx, staging.buffer, vertex_buffer.buffer, data_size);

	vk_destroy_buffer(ctx.allocator, staging);

	return vertex_buffer;
}

static AllocatedBuffer create_index_buffer(const VkContext& ctx, const void* index_data, VkDeviceSize data_size)
{
	AllocatedBuffer staging = vk_create_staging_buffer(
		ctx.allocator,
		index_data,
		data_size);
	AllocatedBuffer index_buffer = vk_create_buffer(
		ctx.allocator,
		data_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	vk_copy_buffer(ctx, staging.buffer, index_buffer.buffer, data_size);
	vk_destroy_buffer(ctx.allocator, staging);
	return index_buffer;
}

static void record_commands(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkRenderPass render_pass, VkPipeline pipeline, VkExtent2D extent, VkBuffer vertex_buffer, VkBuffer index_buffer)
{
    //LOG_VERBOSE("Recording commands for frame slot {}", frame_slot);

    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    // Dark teal — distinguishable from a crash black at a glance
    VkClearValue clear_value = {};
    clear_value.color = { { 0.05f, 0.10f, 0.12f, 1.0f } };

	VkRenderPassBeginInfo rp_begin = {};
	rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp_begin.renderPass = render_pass;
	rp_begin.framebuffer = framebuffer;
	rp_begin.renderArea.offset = { 0, 0 };
	rp_begin.renderArea.extent = extent;
	rp_begin.clearValueCount = 1;
	rp_begin.pClearValues = &clear_value;

	vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkBuffer vertex_buffers[] = { vertex_buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
	vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT16);
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

	vkCmdDrawIndexed(cmd, (u32)indices.size(), 1, 0, 0, 0); // draw indexed with 6 indices, no instancing
	vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));
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
        INITIAL_WIDTH, INITIAL_HEIGHT, "vk_renderer | phase 1", nullptr, nullptr);
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

	VkShaderModule vert_mod = create_shader_module(ctx.device, read_file("src/shader/triangle.vert.spv"));
	VkShaderModule frag_mod = create_shader_module(ctx.device, read_file("src/shader/triangle.frag.spv"));

	VkRenderPass render_pass = create_render_pass(ctx.device, sc.format);
	std::vector<VkFramebuffer> framebuffers = create_framebuffers(
		ctx.device, render_pass, sc.image_views, sc.extent);

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = create_triangle_pipeline(
        ctx.device, render_pass, sc.extent, vert_mod, frag_mod, pipeline_layout);



	AllocatedBuffer vertex_buffer = create_vertex_buffer(ctx, vertices.data(), sizeof(Vertex) * vertices.size());
    AllocatedBuffer index_buffer = create_index_buffer(ctx, indices.data(), sizeof(u16) * indices.size());

    LOG_INFO("Initialisation complete, entering main loop");

    // -------- Main loop --------
    u64 frame_number = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        if (fb_w == 0 || fb_h == 0) {
            LOG_VERBOSE("Window minimised — skipping frame {}", frame_number);
            continue;
        }

        u32 image_index = vk_swapchain_acquire(sc, ctx);
        if (image_index == UINT32_MAX || g_framebuffer_resized) {
            g_framebuffer_resized = false;
            LOG_INFO_TO("render", "Swapchain out of date — recreating");
            vk_swapchain_recreate(sc, ctx, (u32)fb_w, (u32)fb_h);
            continue;
        }

        LOG_VERBOSE("Frame {} — image index {}, frame slot {}",
                    frame_number, image_index, sc.current_frame);

        VkCommandBuffer cmd = frame_data[sc.current_frame].command_buffer;
        vkResetCommandBuffer(cmd, 0);

        record_commands(
            cmd,
            framebuffers[image_index],
            render_pass,
            pipeline,
            sc.extent,
            vertex_buffer.buffer,
            index_buffer.buffer
        );

        bool ok = vk_swapchain_submit_and_present(sc, ctx, cmd, image_index);

        if (!ok) {
            glfwGetFramebufferSize(window, &fb_w, &fb_h);
            LOG_WARN_TO("render", "Present returned out-of-date — recreating swapchain");
            vk_swapchain_recreate(sc, ctx, (u32)fb_w, (u32)fb_h);
        }

        ++frame_number;
    }

    LOG_INFO("Main loop exited after {} frames", frame_number);

    // -------- Cleanup --------
    LOG_DEBUG("Waiting for GPU idle before shutdown");

    vkDeviceWaitIdle(ctx.device);

    vkDestroyPipeline(ctx.device, pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, nullptr);
    vkDestroyRenderPass(ctx.device, render_pass, nullptr);
    for (auto fb : framebuffers)
        vkDestroyFramebuffer(ctx.device, fb, nullptr);
    vkDestroyShaderModule(ctx.device, vert_mod, nullptr);
    vkDestroyShaderModule(ctx.device, frag_mod, nullptr);

    vk_destroy_buffer(ctx.allocator, vertex_buffer);
    vk_destroy_buffer(ctx.allocator, index_buffer);

    destroy_command_infrastructure(ctx);
    vk_swapchain_destroy(sc, ctx);
    vk_context_destroy(ctx);

    glfwDestroyWindow(window);
    glfwTerminate();

    LOG_INFO("Shutdown complete");
    spdlog::shutdown();
    return 0;
}
