#include <cassert>
#include <cstdio>
#include <cstring>

#include "render.h"
#include "setup.h"

// ============================================================================

static
void update(VulkanContext &ctx) {
  mat4x4 vp;
  mat4x4_mul(vp, ctx.scene.projection, ctx.scene.view);
  
  mat4x4 mvp;
  mat4x4_mul(mvp, vp, ctx.scene.model);


  VkResult err;  
  void *pData;
  err = vkMapMemory(ctx.device,
                    ctx.uniformData.mem,
                    0,
                    ctx.uniformData.memAllocInfo.allocationSize,
                    0,
                    (void **)&pData
  );
  assert(!err);

  memcpy(pData, (const void *)&mvp[0][0], sizeof(mvp));

  vkUnmapMemory(ctx.device, ctx.uniformData.mem);
}

// ----------------------------------------------------------------------------

static
void draw(VulkanContext &ctx) {
  VkResult err;

  /* Create a semaphore for presentation */
  VkSemaphoreCreateInfo sem_info;
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sem_info.pNext = nullptr;
  sem_info.flags = 0u;

  VkSemaphore sem_presentComplete;
  err = vkCreateSemaphore(ctx.device, &sem_info, nullptr, &sem_presentComplete);
  assert(!err);

  /**/
  uint32_t buffer_id;
  err = ctx.ext.fpAcquireNextImageKHR(
    ctx.device, ctx.swapchain, UINT64_MAX, sem_presentComplete, VK_NULL_HANDLE, &buffer_id
  );
  assert(err != VK_ERROR_OUT_OF_DATE_KHR);
  assert(!err);


  //
  set_buffer_image_layout(ctx,
                          ctx.swapchainBuffers[buffer_id].image,
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  //
  flush_init_cmd(ctx);

  /**/
  VkPipelineStageFlags dst_stage_flags[1u] = {
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
  };
  
  VkSubmitInfo submit_info;
  memset(&submit_info, 0, sizeof(submit_info));
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  submit_info.waitSemaphoreCount = 1u;
  submit_info.pWaitSemaphores = &sem_presentComplete;
  submit_info.pWaitDstStageMask = dst_stage_flags;
  submit_info.commandBufferCount = 1u;
  submit_info.pCommandBuffers = &ctx.swapchainBuffers[buffer_id].cmd;
  submit_info.signalSemaphoreCount = 0u;
  submit_info.pSignalSemaphores = nullptr;

  err = vkQueueSubmit(ctx.queue, 1u, &submit_info, VK_NULL_HANDLE);
  assert(!err);

  /**/
  VkPresentInfoKHR present_info;
  memset(&present_info, 0, sizeof(present_info));
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;
  present_info.swapchainCount = 1u;
  present_info.pSwapchains = &ctx.swapchain;
  present_info.pImageIndices = &buffer_id;

  err = ctx.ext.fpQueuePresentKHR(ctx.queue, &present_info);
  assert(!err);

  /**/
  err = vkQueueWaitIdle(ctx.queue);
  assert(!err);

  vkDestroySemaphore(ctx.device, sem_presentComplete, nullptr);
}

// ----------------------------------------------------------------------------

void render_frame(VulkanContext &ctx) {
  vkDeviceWaitIdle(ctx.device);
  
  update(ctx);  
  draw(ctx);
}

// ============================================================================
