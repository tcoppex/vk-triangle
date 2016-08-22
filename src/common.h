#ifndef COMMON_H_
#define COMMON_H_

#include <vector>
#include "vulkan/vulkan.h"

#include "linmath.h"


/* Handle to the XCB window manager data */
struct XCBHandler {
  xcb_connection_t *connection;
  xcb_screen_t *screen;
  xcb_window_t window;
  xcb_intern_atom_reply_t *atom_wm_delete_window;
};

/* Handle the window data, hide sublevel API used */
struct WindowContext {
  XCBHandler xcb;
};

/* Holds pointers to Vulkan extension's entrypoints */
struct VulkanExtensionFP {
  // Instance
  PFN_vkGetDeviceProcAddr                       fpGetDeviceProcAddr = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR      fpGetPhysicalDeviceSurfaceSupportKHR = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      fpGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR = nullptr;

  // Device
  PFN_vkCreateSwapchainKHR                      fpCreateSwapchainKHR = nullptr;
  PFN_vkDestroySwapchainKHR                     fpDestroySwapchainKHR = nullptr;
  PFN_vkGetSwapchainImagesKHR                   fpGetSwapchainImagesKHR = nullptr;
  PFN_vkAcquireNextImageKHR                     fpAcquireNextImageKHR = nullptr;
  PFN_vkQueuePresentKHR                         fpQueuePresentKHR = nullptr;
};

/**/
struct SwapchainBuffer {
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
};

/* Vulkan's context data */
struct VulkanContext {

  /* application generic properties */
  struct App {
    App() : width(0u), height(0u) {}
    uint32_t width;
    uint32_t height;
  } app;

  struct Scene {
    mat4x4 projection;
    mat4x4 view;
    mat4x4 model;
  } scene;

  /**/
  VkInstance inst = VK_NULL_HANDLE;
  VkPhysicalDevice gpu = VK_NULL_HANDLE;
  
  /* Surface (screen presentation context) */
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkFormat format;
  VkColorSpaceKHR color_space;

  /* Swapchain (buffers used for rendering and display) */
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  SwapchainBuffer *swapchainBuffers = nullptr;
  uint32_t numSwapchainImages;

  /* Depth buffer */
  struct {
    VkImage image = VK_NULL_HANDLE;
    VkFormat format;
    VkImageView view = VK_NULL_HANDLE;
    VkMemoryAllocateInfo allocateInfo;
    VkDeviceMemory mem;
  } depth;

  /**/
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queue_count;
  uint32_t selected_queue_index;

  /* device objects properties */
  struct {
    VkPhysicalDeviceProperties gpu;
    VkPhysicalDeviceMemoryProperties memory;
    VkQueueFamilyProperties *queue;
  } properties;

  /* Extensions entry points */
  std::vector<char const*> device_extension_names;
  VulkanExtensionFP ext;

  /**/
  VkCommandBuffer initCmdBuffer = VK_NULL_HANDLE;
  VkCommandPool cmdPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkPipelineCache pipelineCache = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkFramebuffer *framebuffers = nullptr;

  VkDescriptorPool descPool = VK_NULL_HANDLE;
  VkDescriptorSet descSet = VK_NULL_HANDLE;

  /**/
  struct {
    VkShaderModule vert_module;
    VkShaderModule frag_module;
  } shader;

  /* Buffer used to store UniformData for geometry */
  struct {
      VkBuffer buffer = VK_NULL_HANDLE;
      VkDeviceMemory mem = VK_NULL_HANDLE;
      VkMemoryAllocateInfo memAllocInfo;
      VkDescriptorBufferInfo descBufferInfo;
  } uniformData;
};

#endif // COMMON_H_