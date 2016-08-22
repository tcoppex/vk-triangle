#ifndef SETUP_H_
#define SETUP_H_

#include "common.h"

/* Initialize app specific vulkan objects */
void setup_vk_data(VulkanContext &ctx);

/**/
void set_buffer_image_layout(VulkanContext &ctx,
                             VkImage image,
                             VkImageAspectFlags aspectMask,
                             VkImageLayout old_image_layout,
                             VkImageLayout new_image_layout);

/**/
void flush_init_cmd(VulkanContext &ctx);

#endif  // SETUP_H_