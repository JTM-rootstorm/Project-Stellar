#include "VulkanGraphicsDevicePrivate.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace stellar::graphics::vulkan {

namespace {

// Generated from minimal GLSL 450 solid-color shaders with glslangValidator -V. The SPIR-V is
// embedded to keep default configure/build/test flows independent from a shader compiler.
constexpr std::uint32_t kSolidVertexShaderSpv[] = {
    0x07230203U, 0x00010000U, 0x0008000bU, 0x00000037U, 0x00000000U, 0x00020011U,
    0x00000001U, 0x0006000bU, 0x00000001U, 0x4c534c47U, 0x6474732eU, 0x3035342eU,
    0x00000000U, 0x0003000eU, 0x00000000U, 0x00000001U, 0x0009000fU, 0x00000000U,
    0x00000004U, 0x6e69616dU, 0x00000000U, 0x00000009U, 0x00000018U, 0x00000023U,
    0x0000002aU, 0x00030003U, 0x00000002U, 0x000001c2U, 0x00040005U, 0x00000004U,
    0x6e69616dU, 0x00000000U, 0x00040005U, 0x00000009U, 0x6f635f76U, 0x00726f6cU,
    0x00060005U, 0x0000000cU, 0x77617244U, 0x736e6f43U, 0x746e6174U, 0x00000073U,
    0x00040006U, 0x0000000cU, 0x00000000U, 0x0070766dU, 0x00060006U, 0x0000000cU,
    0x00000001U, 0x65736162U, 0x6c6f635fU, 0x0000726fU, 0x00080006U, 0x0000000cU,
    0x00000002U, 0x5f736168U, 0x74726576U, 0x635f7865U, 0x726f6c6fU, 0x00000000U,
    0x00030005U, 0x0000000eU, 0x00006370U, 0x00040005U, 0x00000018U, 0x6f635f61U,
    0x00726f6cU, 0x00060005U, 0x00000021U, 0x505f6c67U, 0x65567265U, 0x78657472U,
    0x00000000U, 0x00060006U, 0x00000021U, 0x00000000U, 0x505f6c67U, 0x7469736fU,
    0x006e6f69U, 0x00070006U, 0x00000021U, 0x00000001U, 0x505f6c67U, 0x746e696fU,
    0x657a6953U, 0x00000000U, 0x00070006U, 0x00000021U, 0x00000002U, 0x435f6c67U,
    0x4470696cU, 0x61747369U, 0x0065636eU, 0x00070006U, 0x00000021U, 0x00000003U,
    0x435f6c67U, 0x446c6c75U, 0x61747369U, 0x0065636eU, 0x00030005U, 0x00000023U,
    0x00000000U, 0x00050005U, 0x0000002aU, 0x6f705f61U, 0x69746973U, 0x00006e6fU,
    0x00040047U, 0x00000009U, 0x0000001eU, 0x00000000U, 0x00030047U, 0x0000000cU,
    0x00000002U, 0x00040048U, 0x0000000cU, 0x00000000U, 0x00000005U, 0x00050048U,
    0x0000000cU, 0x00000000U, 0x00000007U, 0x00000010U, 0x00050048U, 0x0000000cU,
    0x00000000U, 0x00000023U, 0x00000000U, 0x00050048U, 0x0000000cU, 0x00000001U,
    0x00000023U, 0x00000040U, 0x00050048U, 0x0000000cU, 0x00000002U, 0x00000023U,
    0x00000050U, 0x00040047U, 0x00000018U, 0x0000001eU, 0x00000005U, 0x00030047U,
    0x00000021U, 0x00000002U, 0x00050048U, 0x00000021U, 0x00000000U, 0x0000000bU,
    0x00000000U, 0x00050048U, 0x00000021U, 0x00000001U, 0x0000000bU, 0x00000001U,
    0x00050048U, 0x00000021U, 0x00000002U, 0x0000000bU, 0x00000003U, 0x00050048U,
    0x00000021U, 0x00000003U, 0x0000000bU, 0x00000004U, 0x00040047U, 0x0000002aU,
    0x0000001eU, 0x00000000U, 0x00020013U, 0x00000002U, 0x00030021U, 0x00000003U,
    0x00000002U, 0x00030016U, 0x00000006U, 0x00000020U, 0x00040017U, 0x00000007U,
    0x00000006U, 0x00000004U, 0x00040020U, 0x00000008U, 0x00000003U, 0x00000007U,
    0x0004003bU, 0x00000008U, 0x00000009U, 0x00000003U, 0x00040018U, 0x0000000aU,
    0x00000007U, 0x00000004U, 0x00040015U, 0x0000000bU, 0x00000020U, 0x00000000U,
    0x0005001eU, 0x0000000cU, 0x0000000aU, 0x00000007U, 0x0000000bU, 0x00040020U,
    0x0000000dU, 0x00000009U, 0x0000000cU, 0x0004003bU, 0x0000000dU, 0x0000000eU,
    0x00000009U, 0x00040015U, 0x0000000fU, 0x00000020U, 0x00000001U, 0x0004002bU,
    0x0000000fU, 0x00000010U, 0x00000002U, 0x00040020U, 0x00000011U, 0x00000009U,
    0x0000000bU, 0x0004002bU, 0x0000000bU, 0x00000014U, 0x00000000U, 0x00020014U,
    0x00000015U, 0x00040020U, 0x00000017U, 0x00000001U, 0x00000007U, 0x0004003bU,
    0x00000017U, 0x00000018U, 0x00000001U, 0x0004002bU, 0x00000006U, 0x0000001aU,
    0x3f800000U, 0x0007002cU, 0x00000007U, 0x0000001bU, 0x0000001aU, 0x0000001aU,
    0x0000001aU, 0x0000001aU, 0x00040017U, 0x0000001cU, 0x00000015U, 0x00000004U,
    0x0004002bU, 0x0000000bU, 0x0000001fU, 0x00000001U, 0x0004001cU, 0x00000020U,
    0x00000006U, 0x0000001fU, 0x0006001eU, 0x00000021U, 0x00000007U, 0x00000006U,
    0x00000020U, 0x00000020U, 0x00040020U, 0x00000022U, 0x00000003U, 0x00000021U,
    0x0004003bU, 0x00000022U, 0x00000023U, 0x00000003U, 0x0004002bU, 0x0000000fU,
    0x00000024U, 0x00000000U, 0x00040020U, 0x00000025U, 0x00000009U, 0x0000000aU,
    0x00040017U, 0x00000028U, 0x00000006U, 0x00000003U, 0x00040020U, 0x00000029U,
    0x00000001U, 0x00000028U, 0x0004003bU, 0x00000029U, 0x0000002aU, 0x00000001U,
    0x00040020U, 0x00000032U, 0x00000003U, 0x00000006U, 0x00050036U, 0x00000002U,
    0x00000004U, 0x00000000U, 0x00000003U, 0x000200f8U, 0x00000005U, 0x00050041U,
    0x00000011U, 0x00000012U, 0x0000000eU, 0x00000010U, 0x0004003dU, 0x0000000bU,
    0x00000013U, 0x00000012U, 0x000500abU, 0x00000015U, 0x00000016U, 0x00000013U,
    0x00000014U, 0x0004003dU, 0x00000007U, 0x00000019U, 0x00000018U, 0x00070050U,
    0x0000001cU, 0x0000001dU, 0x00000016U, 0x00000016U, 0x00000016U, 0x00000016U,
    0x000600a9U, 0x00000007U, 0x0000001eU, 0x0000001dU, 0x00000019U, 0x0000001bU,
    0x0003003eU, 0x00000009U, 0x0000001eU, 0x00050041U, 0x00000025U, 0x00000026U,
    0x0000000eU, 0x00000024U, 0x0004003dU, 0x0000000aU, 0x00000027U, 0x00000026U,
    0x0004003dU, 0x00000028U, 0x0000002bU, 0x0000002aU, 0x00050051U, 0x00000006U,
    0x0000002cU, 0x0000002bU, 0x00000000U, 0x00050051U, 0x00000006U, 0x0000002dU,
    0x0000002bU, 0x00000001U, 0x00050051U, 0x00000006U, 0x0000002eU, 0x0000002bU,
    0x00000002U, 0x00070050U, 0x00000007U, 0x0000002fU, 0x0000002cU, 0x0000002dU,
    0x0000002eU, 0x0000001aU, 0x00050091U, 0x00000007U, 0x00000030U, 0x00000027U,
    0x0000002fU, 0x00050041U, 0x00000008U, 0x00000031U, 0x00000023U, 0x00000024U,
    0x0003003eU, 0x00000031U, 0x00000030U, 0x00060041U, 0x00000032U, 0x00000033U,
    0x00000023U, 0x00000024U, 0x0000001fU, 0x0004003dU, 0x00000006U, 0x00000034U,
    0x00000033U, 0x0004007fU, 0x00000006U, 0x00000035U, 0x00000034U, 0x00060041U,
    0x00000032U, 0x00000036U, 0x00000023U, 0x00000024U, 0x0000001fU, 0x0003003eU,
    0x00000036U, 0x00000035U, 0x000100fdU, 0x00010038U,
};

constexpr std::uint32_t kSolidFragmentShaderSpv[] = {
    0x07230203U, 0x00010000U, 0x0008000bU, 0x00000018U, 0x00000000U, 0x00020011U,
    0x00000001U, 0x0006000bU, 0x00000001U, 0x4c534c47U, 0x6474732eU, 0x3035342eU,
    0x00000000U, 0x0003000eU, 0x00000000U, 0x00000001U, 0x0007000fU, 0x00000004U,
    0x00000004U, 0x6e69616dU, 0x00000000U, 0x00000009U, 0x00000015U, 0x00030010U,
    0x00000004U, 0x00000007U, 0x00030003U, 0x00000002U, 0x000001c2U, 0x00040005U,
    0x00000004U, 0x6e69616dU, 0x00000000U, 0x00050005U, 0x00000009U, 0x5f74756fU,
    0x6f6c6f63U, 0x00000072U, 0x00060005U, 0x0000000cU, 0x77617244U, 0x736e6f43U,
    0x746e6174U, 0x00000073U, 0x00040006U, 0x0000000cU, 0x00000000U, 0x0070766dU,
    0x00060006U, 0x0000000cU, 0x00000001U, 0x65736162U, 0x6c6f635fU, 0x0000726fU,
    0x00080006U, 0x0000000cU, 0x00000002U, 0x5f736168U, 0x74726576U, 0x635f7865U,
    0x726f6c6fU, 0x00000000U, 0x00030005U, 0x0000000eU, 0x00006370U, 0x00040005U,
    0x00000015U, 0x6f635f76U, 0x00726f6cU, 0x00040047U, 0x00000009U, 0x0000001eU,
    0x00000000U, 0x00030047U, 0x0000000cU, 0x00000002U, 0x00040048U, 0x0000000cU,
    0x00000000U, 0x00000005U, 0x00050048U, 0x0000000cU, 0x00000000U, 0x00000007U,
    0x00000010U, 0x00050048U, 0x0000000cU, 0x00000000U, 0x00000023U, 0x00000000U,
    0x00050048U, 0x0000000cU, 0x00000001U, 0x00000023U, 0x00000040U, 0x00050048U,
    0x0000000cU, 0x00000002U, 0x00000023U, 0x00000050U, 0x00040047U, 0x00000015U,
    0x0000001eU, 0x00000000U, 0x00020013U, 0x00000002U, 0x00030021U, 0x00000003U,
    0x00000002U, 0x00030016U, 0x00000006U, 0x00000020U, 0x00040017U, 0x00000007U,
    0x00000006U, 0x00000004U, 0x00040020U, 0x00000008U, 0x00000003U, 0x00000007U,
    0x0004003bU, 0x00000008U, 0x00000009U, 0x00000003U, 0x00040018U, 0x0000000aU,
    0x00000007U, 0x00000004U, 0x00040015U, 0x0000000bU, 0x00000020U, 0x00000000U,
    0x0005001eU, 0x0000000cU, 0x0000000aU, 0x00000007U, 0x0000000bU, 0x00040020U,
    0x0000000dU, 0x00000009U, 0x0000000cU, 0x0004003bU, 0x0000000dU, 0x0000000eU,
    0x00000009U, 0x00040015U, 0x0000000fU, 0x00000020U, 0x00000001U, 0x0004002bU,
    0x0000000fU, 0x00000010U, 0x00000001U, 0x00040020U, 0x00000011U, 0x00000009U,
    0x00000007U, 0x00040020U, 0x00000014U, 0x00000001U, 0x00000007U, 0x0004003bU,
    0x00000014U, 0x00000015U, 0x00000001U, 0x00050036U, 0x00000002U, 0x00000004U,
    0x00000000U, 0x00000003U, 0x000200f8U, 0x00000005U, 0x00050041U, 0x00000011U,
    0x00000012U, 0x0000000eU, 0x00000010U, 0x0004003dU, 0x00000007U, 0x00000013U,
    0x00000012U, 0x0004003dU, 0x00000007U, 0x00000016U, 0x00000015U, 0x00050085U,
    0x00000007U, 0x00000017U, 0x00000013U, 0x00000016U, 0x0003003eU, 0x00000009U,
    0x00000017U, 0x000100fdU, 0x00010038U,
};

std::expected<VkShaderModule, stellar::platform::Error>
create_shader_module(VkDevice device, const std::uint32_t* code, std::size_t word_count) {
    const VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = word_count * sizeof(std::uint32_t),
        .pCode = code,
    };

    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result = vkCreateShaderModule(device, &create_info, nullptr, &module);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateShaderModule", result));
    }
    return module;
}

} // namespace

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_pipeline_layout() {
    const VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(VulkanDrawPushConstants),
    };
    const VkPipelineLayoutCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };

    const VkResult result = vkCreatePipelineLayout(device_, &create_info, nullptr,
                                                   &pipeline_layout_);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreatePipelineLayout", result));
    }
    return {};
}

std::expected<void, stellar::platform::Error> VulkanGraphicsDevice::create_graphics_pipeline() {
    auto vertex_shader = create_shader_module(device_, kSolidVertexShaderSpv,
                                              std::size(kSolidVertexShaderSpv));
    if (!vertex_shader) {
        return std::unexpected(vertex_shader.error());
    }
    auto fragment_shader = create_shader_module(device_, kSolidFragmentShaderSpv,
                                                std::size(kSolidFragmentShaderSpv));
    if (!fragment_shader) {
        vkDestroyShaderModule(device_, *vertex_shader, nullptr);
        return std::unexpected(fragment_shader.error());
    }

    const VkPipelineShaderStageCreateInfo shader_stages[] = {
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = *vertex_shader,
            .pName = "main",
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = *fragment_shader,
            .pName = "main",
        },
    };
    const VkVertexInputBindingDescription binding_description{
        .binding = 0,
        .stride = sizeof(stellar::assets::StaticVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    const VkVertexInputAttributeDescription attribute_descriptions[] = {
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(stellar::assets::StaticVertex, position),
        },
        VkVertexInputAttributeDescription{
            .location = 5,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(stellar::assets::StaticVertex, color),
        },
    };
    const VkPipelineVertexInputStateCreateInfo vertex_input_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = std::size(attribute_descriptions),
        .pVertexAttributeDescriptions = attribute_descriptions,
    };
    const VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    const VkViewport viewport{
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(swapchain_extent_.width),
        .height = static_cast<float>(swapchain_extent_.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
    };
    const VkRect2D scissor{{0, 0}, swapchain_extent_};
    const VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };
    const VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0F,
    };
    const VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    const VkPipelineDepthStencilStateCreateInfo depth_stencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };
    const VkPipelineColorBlendAttachmentState color_blend_attachment{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo color_blending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };
    const VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = std::size(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .layout = pipeline_layout_,
        .renderPass = render_pass_,
        .subpass = 0,
    };

    const VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
                                                      nullptr, &graphics_pipeline_);
    vkDestroyShaderModule(device_, *fragment_shader, nullptr);
    vkDestroyShaderModule(device_, *vertex_shader, nullptr);
    if (result != VK_SUCCESS) {
        return std::unexpected(vulkan_error("vkCreateGraphicsPipelines", result));
    }
    return {};
}
} // namespace stellar::graphics::vulkan
