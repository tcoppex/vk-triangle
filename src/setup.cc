#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "vulkan/vulkan.h"
#include "setup.h"

// ============================================================================


void setup_init_cmd_buffer(VulkanContext &ctx) {
  if (ctx.initCmdBuffer != VK_NULL_HANDLE) {
    fprintf(stderr, "dev warning : init cmd buffer already setupped\n");
    return;
  }

  /**/
  VkCommandBufferAllocateInfo cmdInfo;
  cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdInfo.pNext = nullptr;
  cmdInfo.commandPool = ctx.cmdPool;
  cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // TODOC
  cmdInfo.commandBufferCount = 1;

  VkResult err;
  err = vkAllocateCommandBuffers(ctx.device, &cmdInfo, &ctx.initCmdBuffer);
  assert(!err);

  /**/
  VkCommandBufferInheritanceInfo hinfo;
  hinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  hinfo.pNext = nullptr;
  hinfo.renderPass = VK_NULL_HANDLE;
  hinfo.subpass = 0;
  hinfo.framebuffer = VK_NULL_HANDLE;
  hinfo.occlusionQueryEnable = VK_FALSE;
  hinfo.queryFlags = 0;
  hinfo.pipelineStatistics = 0;

  /**/
  VkCommandBufferBeginInfo beginInfo;
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.pNext = nullptr;
  beginInfo.flags = 0;
  beginInfo.pInheritanceInfo = &hinfo;

  err = vkBeginCommandBuffer(ctx.initCmdBuffer, &beginInfo);
  assert(!err);
}

// ----------------------------------------------------------------------------

void flush_init_cmd(VulkanContext &ctx) {
  VkResult err;

  if (ctx.initCmdBuffer == VK_NULL_HANDLE) {
    return;
  }

  err = vkEndCommandBuffer(ctx.initCmdBuffer);
  assert(!err);

  VkSubmitInfo info;
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext = nullptr;
  info.waitSemaphoreCount = 0u;
  info.pWaitSemaphores = nullptr;
  info.pWaitDstStageMask = nullptr;
  info.commandBufferCount = 1u;
  info.pCommandBuffers = &ctx.initCmdBuffer;
  info.signalSemaphoreCount = 0u;
  info.pSignalSemaphores = nullptr;

  err = vkQueueSubmit(ctx.queue, 1u, &info, VK_NULL_HANDLE);
  assert(!err);

  err = vkQueueWaitIdle(ctx.queue);
  assert(!err);

  vkFreeCommandBuffers(ctx.device, ctx.cmdPool, 1u, &ctx.initCmdBuffer);
  ctx.initCmdBuffer = VK_NULL_HANDLE;
}

// ----------------------------------------------------------------------------

/** Set the swapchain buffers' images */
void set_buffer_image_layout(VulkanContext &ctx,
                             VkImage image,
                             VkImageAspectFlags aspectMask,
                             VkImageLayout old_image_layout,
                             VkImageLayout new_image_layout) {

  //assert(ctx.initCmdBuffer != VK_NULL_HANDLE);
  if (ctx.initCmdBuffer == VK_NULL_HANDLE) {
    setup_init_cmd_buffer(ctx);
  }

  VkImageMemoryBarrier image_memory_barrier;
  image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_memory_barrier.pNext = nullptr;
  image_memory_barrier.srcAccessMask = 0;
  image_memory_barrier.dstAccessMask = 0;
  image_memory_barrier.oldLayout = old_image_layout;
  image_memory_barrier.newLayout = new_image_layout;
  image_memory_barrier.image = image;
  image_memory_barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};

  VkAccessFlags dstAccess = 0;
  switch (new_image_layout)
  {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
    break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      dstAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    break;

    default:
    break;
  };

  image_memory_barrier.dstAccessMask = dstAccess;

  vkCmdPipelineBarrier(ctx.initCmdBuffer,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &image_memory_barrier);
}

// ----------------------------------------------------------------------------

bool retrieve_memory_type_index(const VkPhysicalDeviceMemoryProperties &props,
                                const uint32_t typeBits,
                                const VkFlags requirementsMask,
                                uint32_t *typeIndex) {
  uint32_t bits = typeBits;

  for (uint32_t i=0u; i<32u; ++i) {
    if (bits & 1u) {
      if ((props.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)
      {
        *typeIndex = i;
        return true;
      }
    }
    bits >>= 1u;
  }
  return false;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void setup_swapchain_buffers(VulkanContext &ctx) {
  VkResult err;

  /* Set swapchains buffers resolutions, try to match with surface resolution */
  // retrieve surface capabilities
  VkSurfaceCapabilitiesKHR capabilities;
  err = 
  ctx.ext.fpGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.gpu, ctx.surface, &capabilities);
  assert(!err);

  VkExtent2D swapchain_extent;
  if (capabilities.currentExtent.width == UINT32_MAX) {
    /* if surface resolution is undefined, set to app resolution */
    swapchain_extent.width = ctx.app.width;
    swapchain_extent.height = ctx.app.height;
  } else {
    swapchain_extent = capabilities.currentExtent;
    ctx.app.width = capabilities.currentExtent.width;
    ctx.app.height = capabilities.currentExtent.height;
  }

  /* Presentation mode allow the surface to be displayed on screen */
  // retrieve presentation modes of the surface
  uint32_t present_mode_count;
  err = ctx.ext.fpGetPhysicalDeviceSurfacePresentModesKHR(
    ctx.gpu, ctx.surface, &present_mode_count, nullptr
  );
  assert(!err); assert(present_mode_count > 0u);

  VkPresentModeKHR *present_modes = new VkPresentModeKHR[present_mode_count];
  err = ctx.ext.fpGetPhysicalDeviceSurfacePresentModesKHR(
    ctx.gpu, ctx.surface, &present_mode_count, present_modes
  );
  assert(!err);

  /* Search for the best available swapchain present mode */
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // default
  for (uint32_t i=0u; i < present_mode_count; ++i) {
    // Best (lowest latency non tearing mode)
    if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
      break;
    }
    // Second best (fastest, with tears)
    if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
  }
  delete [] present_modes;

  /* Number of images tu use on the swachains */
  uint32_t numSwapchainImages = std::min(capabilities.minImageCount + 1u,
                                         capabilities.maxImageCount);

  /* Transform applied to the surface */
  VkSurfaceTransformFlagBitsKHR preTransform = capabilities.currentTransform;
  if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }

  /* Save current swapchain */
  VkSwapchainKHR oldSwapchain = ctx.swapchain;

  /* Create the Swapchain */
  VkSwapchainCreateInfoKHR swapchainInfo;
  swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainInfo.pNext = nullptr;
  swapchainInfo.surface = ctx.surface;
  swapchainInfo.minImageCount = numSwapchainImages;
  swapchainInfo.imageFormat = ctx.format;
  swapchainInfo.imageColorSpace = ctx.color_space;
  swapchainInfo.imageExtent = swapchain_extent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.queueFamilyIndexCount = 0;
  swapchainInfo.pQueueFamilyIndices = nullptr;
  swapchainInfo.preTransform = preTransform;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = present_mode;
  swapchainInfo.clipped = true;
  swapchainInfo.oldSwapchain = oldSwapchain;

  err = ctx.ext.fpCreateSwapchainKHR(
    ctx.device, &swapchainInfo, nullptr, &ctx.swapchain
  );
  assert(!err);


  /* Destroy previous swapchain */
  if (oldSwapchain != VK_NULL_HANDLE) {
    ctx.ext.fpDestroySwapchainKHR(ctx.device, oldSwapchain, nullptr);
  }


  /* Setup swapchain buffers */
  err = ctx.ext.fpGetSwapchainImagesKHR(
    ctx.device, ctx.swapchain, &ctx.numSwapchainImages, nullptr
  );
  assert(!err);

  // retrieve the images
  VkImage *swapchainImages = new VkImage[ctx.numSwapchainImages];
  err = ctx.ext.fpGetSwapchainImagesKHR(
    ctx.device, ctx.swapchain, &ctx.numSwapchainImages, swapchainImages
  );
  assert(!err);

  // create swapchains buffers (image, view and command buffer)
  ctx.swapchainBuffers = new SwapchainBuffer[ctx.numSwapchainImages];

  // generic ImageView
  VkImageViewCreateInfo colorImageView;
  colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  colorImageView.pNext = nullptr;
  colorImageView.format = ctx.format;
  colorImageView.components = { VK_COMPONENT_SWIZZLE_R,
                                VK_COMPONENT_SWIZZLE_G,
                                VK_COMPONENT_SWIZZLE_B,
                                VK_COMPONENT_SWIZZLE_A };
  colorImageView.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
  colorImageView.flags = 0u;

  for (uint32_t i=0u; i<ctx.numSwapchainImages; ++i) {
    const VkImage &img = swapchainImages[i];

    colorImageView.image = img;
    ctx.swapchainBuffers[i].image = img;

    set_buffer_image_layout(ctx,
                            img,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    err = vkCreateImageView(
      ctx.device, &colorImageView, nullptr, &ctx.swapchainBuffers[i].view
    );
    assert(!err);
  }

  //delete [] swapchainImages; //

  // ----------

  assert(ctx.cmdPool != VK_NULL_HANDLE);

  /* Allocate swapchain's buffers command buffer */
  VkCommandBufferAllocateInfo info;
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext = nullptr;
  info.commandPool = ctx.cmdPool;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  info.commandBufferCount = 1u;

  for (uint32_t i=0u; i<ctx.numSwapchainImages; ++i) {
    err = vkAllocateCommandBuffers(ctx.device, &info, &ctx.swapchainBuffers[i].cmd);
    assert(!err);
  }
}

// ----------------------------------------------------------------------------

void setup_depth_buffer(VulkanContext &ctx) {
  VkResult err;

  const VkFormat depth_format = VK_FORMAT_D16_UNORM;

  /**/
  VkImageCreateInfo imageInfo;
  memset(&imageInfo, 0, sizeof(imageInfo));
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.pNext = nullptr;
  imageInfo.flags = 0u;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = depth_format;
  imageInfo.extent = { ctx.app.width, ctx.app.height, 1 };
  imageInfo.mipLevels = 1u;
  imageInfo.arrayLayers = 1u;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // TODOC
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  ctx.depth.format = depth_format;

  err = vkCreateImage(ctx.device, &imageInfo, nullptr, &ctx.depth.image);
  assert(!err);

  /* Retrieve memory requirements for the image */
  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(ctx.device, ctx.depth.image, &memReqs);

  ctx.depth.allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ctx.depth.allocateInfo.pNext = nullptr;
  ctx.depth.allocateInfo.allocationSize = memReqs.size;
  ctx.depth.allocateInfo.memoryTypeIndex = 0u;

  bool res = retrieve_memory_type_index(
    ctx.properties.memory,
    memReqs.memoryTypeBits,
    0,
    &ctx.depth.allocateInfo.memoryTypeIndex
  );
  assert(res);


  /* Allocate the memory */
  err = vkAllocateMemory(ctx.device, &ctx.depth.allocateInfo, nullptr, &ctx.depth.mem);
  assert(!err);

  /* Bind memory */
  err = vkBindImageMemory(ctx.device, ctx.depth.image, ctx.depth.mem, 0);
  assert(!err);

  // TODO : set image layout
  set_buffer_image_layout(ctx,
                          ctx.depth.image,
                          VK_IMAGE_ASPECT_DEPTH_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  /* Create image view */
  VkImageViewCreateInfo viewInfo;
  memset(&viewInfo, 0, sizeof(viewInfo));
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.pNext = nullptr;
  viewInfo.flags = 0;
  viewInfo.image = ctx.depth.image;
  viewInfo.format = depth_format;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

  err = vkCreateImageView(ctx.device, &viewInfo, nullptr, &ctx.depth.view);
  assert(!err);
}

// ----------------------------------------------------------------------------

// XXX rendering issue probably here XXX

void setup_data_buffer(VulkanContext &ctx) {
  /// -----------------------------------------------------
  /// for Vulkan memory management type, see
  /// https://developer.nvidia.com/vulkan-memory-management
  /// -----------------------------------------------------


  /* -- Host data -- */
  
  // meeeh..
  struct DataLayout_t {
    mat4x4 mvp;
    vec4 position[3u];
    vec4 color[3u];
  } data_layout;

  const unsigned int dataSize = sizeof(data_layout);

  // -------

  const float attrib_data[] = {
    // vertex position
    -1.0f, -1.0f, 0.0f, 1.0f,
    +1.0f, -1.0f, 0.0f, 1.0f,
    0.0f, 0.73f, 0.0f, 1.0f,

    // rgba color
    1.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f, 1.0f
  };


  /* -- Device data -- */

  VkResult err;

  /* create the uniform buffer */
  VkBufferCreateInfo bufferInfo;
  memset(&bufferInfo, 0, sizeof(VkBufferCreateInfo));
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = dataSize;
  bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  err = vkCreateBuffer(ctx.device, &bufferInfo, nullptr, &ctx.uniformData.buffer);
  assert(!err);

  /* retrieve memory requirements for the buffer */
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(ctx.device, ctx.uniformData.buffer, &memReqs);
  
  /* allocate memory */
  VkMemoryAllocateInfo &allocInfo = ctx.uniformData.memAllocInfo;
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = nullptr;
  allocInfo.allocationSize = memReqs.size;
  allocInfo.memoryTypeIndex = 0u;

  bool res = retrieve_memory_type_index(
    ctx.properties.memory,
    memReqs.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    &allocInfo.memoryTypeIndex
  );
  assert(res);

  err = vkAllocateMemory(ctx.device, &allocInfo, nullptr, &ctx.uniformData.mem);
  assert(!err);


  /* Copy data from host to device memory */
  {
    size_t offset = sizeof(mat4x4);

    void *pData(nullptr);
    err = vkMapMemory(
      ctx.device, ctx.uniformData.mem, offset, allocInfo.allocationSize - offset, 0u, (void**)&pData
    );
    assert(!err);

    // copy attrib data, skip mvp
    memcpy(pData, attrib_data, sizeof(attrib_data));

    vkUnmapMemory(ctx.device, ctx.uniformData.mem);
  }
  
  /* Bind the buffer to device memory */
  err = vkBindBufferMemory(ctx.device, ctx.uniformData.buffer, ctx.uniformData.mem, 0);
  assert(!err);

  ctx.uniformData.descBufferInfo.buffer = ctx.uniformData.buffer;
  ctx.uniformData.descBufferInfo.offset = 0u;
  ctx.uniformData.descBufferInfo.range = dataSize;
}

// ----------------------------------------------------------------------------

void setup_descriptor_layout(VulkanContext &ctx) {
  VkResult err;

  /* Defines the descriptor set layout binding */
  const unsigned int bindingCount = 1u;
  VkDescriptorSetLayoutBinding layout_bind[bindingCount];

  // uniform buffer layout (used by Vertex shader stage)
  layout_bind[0u].binding = 0u;
  layout_bind[0u].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layout_bind[0u].descriptorCount = 1u;
  layout_bind[0u].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  layout_bind[0u].pImmutableSamplers = nullptr;


  /* Create the descriptor set layout */
  VkDescriptorSetLayoutCreateInfo layout_info;
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.pNext = nullptr;
  layout_info.flags = 0;
  layout_info.bindingCount = bindingCount;
  layout_info.pBindings = layout_bind;

  err = vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &ctx.descLayout);
  assert(!err);


  /* Create the pipeline layout */
  VkPipelineLayoutCreateInfo pipeline_info;
  memset(&pipeline_info, 0, sizeof(pipeline_info));
  pipeline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_info.pNext = nullptr;
  pipeline_info.setLayoutCount = 1u;
  pipeline_info.pSetLayouts = &ctx.descLayout;

  err = vkCreatePipelineLayout(ctx.device, &pipeline_info, nullptr, &ctx.pipelineLayout);
  assert(!err);
}

// ----------------------------------------------------------------------------

void setup_render_pass(VulkanContext &ctx) {
  const unsigned int attachmentCount = 2u;
  VkAttachmentDescription descs[attachmentCount];
  VkAttachmentReference references[attachmentCount];

  memset(descs, 0, sizeof(VkAttachmentDescription) * attachmentCount);
  memset(references, 0, sizeof(VkAttachmentReference) * attachmentCount);

  /* color attachment */
  descs[0u].format          = ctx.format;
  descs[0u].samples         = VK_SAMPLE_COUNT_1_BIT;
  descs[0u].loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
  descs[0u].storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
  descs[0u].stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  descs[0u].stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  descs[0u].initialLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  descs[0u].finalLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  references[0u].attachment = 0u;
  references[0u].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth attachment */
  descs[1u].format          = ctx.depth.format;
  descs[1u].samples         = VK_SAMPLE_COUNT_1_BIT;
  descs[1u].loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
  descs[1u].storeOp         = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  descs[1u].stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  descs[1u].stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  descs[1u].initialLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  descs[1u].finalLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  references[1u].attachment = 1u;
  references[1u].layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* Defines subpass */
  VkSubpassDescription subpass;
  subpass.flags = 0u;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0u;
  subpass.pInputAttachments = nullptr;
  subpass.colorAttachmentCount = 1u;
  subpass.pColorAttachments = &references[0u];
  subpass.pResolveAttachments = nullptr;
  subpass.pDepthStencilAttachment = &references[1u];
  subpass.preserveAttachmentCount = 0u;
  subpass.pPreserveAttachments = nullptr;

  /* Create the render pass */
  VkRenderPassCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.pNext = nullptr;
  info.attachmentCount = attachmentCount;
  info.pAttachments = descs;
  info.subpassCount = 1u;
  info.pSubpasses = &subpass;
  info.dependencyCount = 0u;
  info.pDependencies = nullptr;

  VkResult err;
  err = vkCreateRenderPass(ctx.device, &info, nullptr, &ctx.renderPass);
  assert(!err);
}

// ----------------------------------------------------------------------------

void setup_framebuffers(VulkanContext &ctx) {
  assert(ctx.renderPass != VK_NULL_HANDLE);
  assert(ctx.swapchainBuffers != nullptr);

  VkResult err;
  VkImageView attachments[2u];

  VkFramebufferCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.renderPass = ctx.renderPass;
  info.attachmentCount = 2u;
  info.pAttachments = attachments;
  info.width = ctx.app.width;
  info.height = ctx.app.height;
  info.layers = 1u;

  ctx.framebuffers = new VkFramebuffer[ctx.numSwapchainImages];

  attachments[1u] = ctx.depth.view;
  for (uint32_t i = 0u; i < ctx.numSwapchainImages; ++i) {
      attachments[0u] = ctx.swapchainBuffers[i].view;
      err = vkCreateFramebuffer(ctx.device, &info, nullptr, &ctx.framebuffers[i]);
      assert(!err);
  }
}

// ----------------------------------------------------------------------------

static
char* read_binary_file(const char *filename, size_t &filesize) {
    FILE *fd = fopen(filename, "rb");
    if (!fd) {
      return nullptr;
    }

    fseek(fd, 0L, SEEK_END);
    filesize = ftell(fd);
    fseek(fd, 0L, SEEK_SET);

    char* shader_binary = new char[filesize];
    size_t ret = fread(shader_binary, filesize, 1u, fd);
    assert(ret == 1u);

    fclose(fd);
    return shader_binary;
}

// ----------------------------------------------------------------------------

static
VkShaderModule create_shader_module(VkDevice device, const char *filename) {
    size_t codesize;
    char* code = read_binary_file(filename, codesize);

    if (code == nullptr) {
      fprintf(stderr, "Error : shader \"%s\" not found.\n", filename);
      exit(EXIT_FAILURE);
    }

    VkResult err;

    VkShaderModuleCreateInfo moduleInfo;
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.pNext = nullptr;
    moduleInfo.flags = 0;
    moduleInfo.codeSize = codesize;
    moduleInfo.pCode = (uint32_t*)code;

    VkShaderModule module;
    err = vkCreateShaderModule(device, &moduleInfo, nullptr, &module);
    assert(!err);

    delete [] code;

    return module;
}

// ----------------------------------------------------------------------------

void setup_pipeline(VulkanContext &ctx) {
  VkResult err;

  /* Setup pipeline shader stages */
  ctx.shader.vert_module = create_shader_module(ctx.device, SHADERS_DIR "simple.vert.spv");
  ctx.shader.frag_module = create_shader_module(ctx.device, SHADERS_DIR "simple.frag.spv");

  const unsigned int stageCount = 2u;
  VkPipelineShaderStageCreateInfo shaderStages[stageCount];
  memset(&shaderStages, 0, stageCount * sizeof(VkPipelineShaderStageCreateInfo));

  shaderStages[0u].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0u].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0u].module = ctx.shader.vert_module;
  shaderStages[0u].pName = "main";

  shaderStages[1u].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1u].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1u].module = ctx.shader.frag_module;
  shaderStages[1u].pName = "main";


  /* Setup pipeline states */
  struct States_t {
    VkPipelineVertexInputStateCreateInfo vi;
    VkPipelineInputAssemblyStateCreateInfo ia;
    VkPipelineTessellationStateCreateInfo ts;
    VkPipelineViewportStateCreateInfo vp;
    VkPipelineRasterizationStateCreateInfo rs;
    VkPipelineMultisampleStateCreateInfo ms;
    VkPipelineDepthStencilStateCreateInfo ds;
    VkPipelineColorBlendStateCreateInfo cb;
    VkPipelineDynamicStateCreateInfo dynamic;
  } states;
  memset(&states, 0, sizeof(States_t));

  // vertex input
  states.vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  // input assembly
  states.ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  states.ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // tessellation
  states.ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

  // viewport
  states.vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  states.vp.viewportCount = 1u;
  states.vp.scissorCount = 1u;

  // rasterization
  states.rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  states.rs.flags = 0;
  states.rs.depthClampEnable = VK_FALSE;
  states.rs.rasterizerDiscardEnable = VK_FALSE;
  states.rs.polygonMode = VK_POLYGON_MODE_FILL;
  states.rs.cullMode = VK_CULL_MODE_BACK_BIT;
  states.rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  states.rs.depthBiasEnable = VK_FALSE;
  states.rs.lineWidth = 1.0f;

  // multisample
  states.ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  states.ms.pSampleMask = nullptr;
  states.ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // depth stencil
  states.ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  states.ds.depthTestEnable = VK_TRUE;
  states.ds.depthWriteEnable = VK_TRUE;
  states.ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  states.ds.depthBoundsTestEnable = VK_FALSE;
  states.ds.stencilTestEnable = VK_FALSE;
  states.ds.back.failOp = VK_STENCIL_OP_KEEP;
  states.ds.back.passOp = VK_STENCIL_OP_KEEP;
  states.ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
  states.ds.front = states.ds.back;

  // color blend
  VkPipelineColorBlendAttachmentState blendAttachState;
  memset(&blendAttachState, 0, sizeof(VkPipelineColorBlendAttachmentState));
  blendAttachState.blendEnable = VK_FALSE;
  blendAttachState.colorWriteMask =   VK_COLOR_COMPONENT_R_BIT
                                    | VK_COLOR_COMPONENT_G_BIT
                                    | VK_COLOR_COMPONENT_B_BIT
                                    | VK_COLOR_COMPONENT_A_BIT;

  states.cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  states.cb.attachmentCount = 1u;
  states.cb.pAttachments = &blendAttachState;

  // dynamic states
  VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
  unsigned int dynamicStateCount = 0u;
  memset(dynamicStateEnables, 0, sizeof(VkDynamicState));
  dynamicStateEnables[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
  dynamicStateEnables[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;

  states.dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  states.dynamic.dynamicStateCount = dynamicStateCount;
  states.dynamic.pDynamicStates = dynamicStateEnables;


  /* Create the pipeline cache */
  VkPipelineCacheCreateInfo pipelineCache;
  memset(&pipelineCache, 0, sizeof(pipelineCache));
  pipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

  err =
  vkCreatePipelineCache(ctx.device, &pipelineCache, nullptr, &ctx.pipelineCache);
  assert(!err);


  /* Create the Graphic Pipeline */
  VkGraphicsPipelineCreateInfo pipeline;
  memset(&pipeline, 0, sizeof(pipeline));
  pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline.stageCount = stageCount;
  pipeline.pStages = shaderStages;
  
  pipeline.pVertexInputState = &states.vi;
  pipeline.pInputAssemblyState = &states.ia;
  pipeline.pTessellationState = nullptr;
  pipeline.pViewportState = &states.vp;
  pipeline.pRasterizationState = &states.rs;
  pipeline.pMultisampleState = &states.ms;
  pipeline.pDepthStencilState = &states.ds;
  pipeline.pColorBlendState = &states.cb;
  pipeline.pDynamicState = &states.dynamic;

  pipeline.layout = ctx.pipelineLayout;
  pipeline.renderPass = ctx.renderPass;

  err = vkCreateGraphicsPipelines(
    ctx.device, ctx.pipelineCache, 1u, &pipeline, nullptr, &ctx.pipeline
  );
  assert(!err);


  /* destroy shader modules */
  vkDestroyShaderModule(ctx.device, ctx.shader.vert_module, nullptr);
  vkDestroyShaderModule(ctx.device, ctx.shader.frag_module, nullptr);
}

// ----------------------------------------------------------------------------

void setup_descriptor(VulkanContext &ctx) {
  VkResult err;

  /* Create descriptor pool */
  const unsigned int numPoolSize = 1u;
  VkDescriptorPoolSize desc_pool_sizes[numPoolSize];
  desc_pool_sizes[0u].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  desc_pool_sizes[0u].descriptorCount = 1u;

  VkDescriptorPoolCreateInfo desc_pool_info;
  desc_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  desc_pool_info.pNext = nullptr;
  desc_pool_info.maxSets = 1u;
  desc_pool_info.poolSizeCount = numPoolSize;
  desc_pool_info.pPoolSizes = desc_pool_sizes;

  err = vkCreateDescriptorPool(
    ctx.device, &desc_pool_info, nullptr, &ctx.descPool
  );
  assert(!err);


  /* Create descriptor set */
  assert(ctx.descLayout != VK_NULL_HANDLE);

  VkDescriptorSetAllocateInfo desc_alloc_info;
  desc_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  desc_alloc_info.pNext = nullptr;
  desc_alloc_info.descriptorPool = ctx.descPool;
  desc_alloc_info.descriptorSetCount = 1u;
  desc_alloc_info.pSetLayouts = &ctx.descLayout;

  err = vkAllocateDescriptorSets(ctx.device, &desc_alloc_info, &ctx.descSet);
  assert(!err);

  VkWriteDescriptorSet write_desc;
  memset(&write_desc, 0, sizeof(write_desc));

  write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_desc.dstSet = ctx.descSet;
  write_desc.descriptorCount = 1u;
  write_desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write_desc.pBufferInfo = &ctx.uniformData.descBufferInfo;

  vkUpdateDescriptorSets(ctx.device, 1u, &write_desc, 0, nullptr);
}

// ----------------------------------------------------------------------------

void setup_buffer_draw_cmd(VulkanContext &ctx, const unsigned int buffer_index) {
  VkResult err;

  const VkCommandBuffer &cmdBuffer = ctx.swapchainBuffers[buffer_index].cmd;

  /* Begin the command buffer */
  VkCommandBufferInheritanceInfo hinfo;
  hinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  hinfo.pNext = nullptr;
  hinfo.renderPass = VK_NULL_HANDLE;
  hinfo.subpass = 0u;
  hinfo.framebuffer = VK_NULL_HANDLE;
  hinfo.occlusionQueryEnable = VK_FALSE;
  hinfo.queryFlags = 0u;
  hinfo.pipelineStatistics = 0u;

  VkCommandBufferBeginInfo cmd_buffer_info;
  cmd_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_buffer_info.pNext = nullptr;
  cmd_buffer_info.flags = 0u;
  cmd_buffer_info.pInheritanceInfo = &hinfo;

  err = vkBeginCommandBuffer(cmdBuffer, &cmd_buffer_info);
  assert(!err);


  /* Begin the renderpass */
  const unsigned int numClearValues = 2u;
  VkClearValue clear_values[numClearValues];
  // used by the color buffer attachment (first)
  clear_values[0u].color.float32[0u] = 0.35f;
  clear_values[0u].color.float32[1u] = 0.40f;
  clear_values[0u].color.float32[2u] = 0.50f;
  clear_values[0u].color.float32[3u] = 1.0f;
  // used by the depth-stencil buffer attachment (second)
  clear_values[1u].depthStencil.depth = 1.0f;
  clear_values[1u].depthStencil.stencil = 0u;

  VkRenderPassBeginInfo rp_begin_info;
  rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin_info.pNext = nullptr;
  rp_begin_info.renderPass = ctx.renderPass;
  rp_begin_info.framebuffer = ctx.framebuffers[buffer_index];
  rp_begin_info.renderArea.offset.x = 0u;
  rp_begin_info.renderArea.offset.y = 0u;
  rp_begin_info.renderArea.extent.width = ctx.app.width;
  rp_begin_info.renderArea.extent.height = ctx.app.height;
  rp_begin_info.clearValueCount = numClearValues;
  rp_begin_info.pClearValues = clear_values;

  vkCmdBeginRenderPass(cmdBuffer, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

  /**/
  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline);

  /**/
  vkCmdBindDescriptorSets(
    cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipelineLayout, 0, 1, &ctx.descSet, 0, nullptr
  );

  /* set viewport */
  VkViewport vp;
  vp.x = 0.0f;
  vp.y = 0.0f;
  vp.width = ctx.app.width;
  vp.height = ctx.app.height;
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;
  vkCmdSetViewport(cmdBuffer, 0u, 1u, &vp);

  /* set scissor */
  VkRect2D scissor;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width  = ctx.app.width;
  scissor.extent.height = ctx.app.height;
  vkCmdSetScissor(cmdBuffer, 0u, 1u, &scissor);

  /* set the draw cmd */
  vkCmdDraw(
    cmdBuffer, 
    3u,       // vertexCount
    1u,       // instanceCount
    0u,       // firstVertex
    0u        // firstInstance
  );

  /* End the renderpass */
  vkCmdEndRenderPass(cmdBuffer);


  /* create a memory barrier before surface presentation */
  VkImageMemoryBarrier presentBarrier;
  presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  presentBarrier.pNext = nullptr;
  presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  presentBarrier.subresourceRange.baseMipLevel = 0u;
  presentBarrier.subresourceRange.levelCount = 1u;
  presentBarrier.subresourceRange.baseArrayLayer = 0u;
  presentBarrier.subresourceRange.layerCount = 1u;
  presentBarrier.image = ctx.swapchainBuffers[buffer_index].image;

  vkCmdPipelineBarrier(
    cmdBuffer,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    0u,                 // dependency flags
    0u, nullptr,        // memory barriers
    0u, nullptr,        // buffer memory barriers
    1u, &presentBarrier // image memory barriers
  );

  /* End command buffer */
  err = vkEndCommandBuffer(cmdBuffer);
  assert(!err);
}

// ----------------------------------------------------------------------------

void setup_vk_data(VulkanContext &ctx) {
  VkResult err;

  /* Create the command pool */
  VkCommandPoolCreateInfo cmdPool_info;
  cmdPool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPool_info.pNext = nullptr;
  cmdPool_info.queueFamilyIndex = ctx.selected_queue_index;
  cmdPool_info.flags = 0u;
  err = vkCreateCommandPool(ctx.device, &cmdPool_info, nullptr, &ctx.cmdPool);
  assert(!err);

  /* Buffer used for initializations */
  //setup_init_cmd_buffer(ctx); //

  /* Swapchain buffers for rendering / display */
  setup_swapchain_buffers(ctx);

  /* Depth buffer */
  setup_depth_buffer(ctx);

  // ------

  /* Application's geometry data setup */
  setup_data_buffer(ctx);

  /* Set pipeline input binding layout */
  setup_descriptor_layout(ctx);

  /* Bind render buffer and their use to pipeline passes */
  setup_render_pass(ctx);

  /* Pipeline states, stages and bind layout */
  setup_pipeline(ctx);


  /* Descriptor pool & set for image / texture */
  setup_descriptor(ctx);

  /**/
  setup_framebuffers(ctx);

  for (uint32_t i = 0u; i < ctx.numSwapchainImages; ++i) {
    setup_buffer_draw_cmd(ctx, i);
  }


  //vkDeviceWaitIdle(ctx.device);
  flush_init_cmd(ctx); //
}

// ============================================================================
