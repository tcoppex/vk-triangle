/* ----------------------------------------------------------------------------


This code is based on the LunarG's Vulkan SDK demo 'cube.c'


 ---------------------------------------------------------------------------- */


#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <vector>

#include "vulkan/vulkan.h"

#include "common.h"
#include "setup.h"
#include "render.h"


// ============================================================================

static const char* sAppName = "Hello Triangle";

static
const char* GetErrMsg(VkResult res) {
  if (res >= 0) {
    return "";
  }

  switch(res) {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "VK_ERROR_INCOMPATIBLE_DRIVER";

    default:
      return "Unknown VK error";
  };
}

#define CHECK_VK(res) if (res<0) fprintf(stderr, "%s\n", GetErrMsg(res)); assert(!err);


// ============================================================================

/**
* Create a connection beetween XCB and the X server
*/
void init_xcb_connection(XCBHandler &xcb) {
  int scr;
  xcb.connection = xcb_connect(nullptr, &scr);

  int err = xcb_connection_has_error(xcb.connection); 
  if (err != 0) {
    fprintf(stderr, "X Connection error (%d) : cannot connect to the X server.\n", err);
    exit(EXIT_FAILURE);
  }

  const xcb_setup_t *setup = xcb_get_setup(xcb.connection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  while (scr--) xcb_screen_next(&iter);

  xcb.screen = iter.data;
}

// ----------------------------------------------------------------------------

/**
* Create a XCB window with basic event signals.
*/
void create_xcb_window(const unsigned int width, const unsigned int height, XCBHandler &xcb) {
  xcb.window = xcb_generate_id(xcb.connection);

  // Enable background pixels and event handling on the window
  uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  
  // values for the mask (set in the bitmask order)
  uint32_t value_list[2u];
  
  value_list[0u] = xcb.screen->black_pixel;

  value_list[1u] =  XCB_EVENT_MASK_KEY_RELEASE
                  | XCB_EVENT_MASK_EXPOSURE
                  | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

  xcb_create_window(  xcb.connection,
                      XCB_COPY_FROM_PARENT,
                      xcb.window,
                      xcb.screen->root,
                      0u, 0u,
                      width, height,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      value_mask,
                      value_list );


  /* Destruction events handling */
  xcb_intern_atom_cookie_t cookie =
      xcb_intern_atom(xcb.connection, 1, 12, "WM_PROTOCOLS");
  xcb_intern_atom_reply_t *reply =
      xcb_intern_atom_reply(xcb.connection, cookie, 0);

  xcb_intern_atom_cookie_t cookie2 =
      xcb_intern_atom(xcb.connection, 0, 16, "WM_DELETE_WINDOW");
  xcb.atom_wm_delete_window =
      xcb_intern_atom_reply(xcb.connection, cookie2, 0);

  xcb_change_property(xcb.connection, XCB_PROP_MODE_REPLACE, xcb.window,
                      (*reply).atom, 4, 32, 1,
                      &(*xcb.atom_wm_delete_window).atom);
  free(reply);


  /* Map the window to the screen (xserver) */
  xcb_map_window(xcb.connection, xcb.window);

  /* Flush XCB pending events, which will display the window */
  xcb_flush(xcb.connection);
}

// ----------------------------------------------------------------------------

void init_wm(WindowContext &winContext) {
  init_xcb_connection(winContext.xcb);
}

// ----------------------------------------------------------------------------

void create_window(VulkanContext &vkContext, WindowContext &winContext) {
  VkResult err;

  /* Init XCB and create a window */
  create_xcb_window(vkContext.app.width, vkContext.app.height, winContext.xcb);

  /* Create the Vulkan - XCB surface */
  VkXcbSurfaceCreateInfoKHR createInfo;
  createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = nullptr;
  createInfo.flags = 0;
  createInfo.connection = winContext.xcb.connection;
  createInfo.window = winContext.xcb.window;

  err = vkCreateXcbSurfaceKHR(vkContext.inst, &createInfo, nullptr, &vkContext.surface);
  CHECK_VK(err);

  // This is a WM specific call, typically for GLFW we'll call
  // glfwCreateWindowSurface(vkContext.inst, &glfw_window, nullptr, &vkContext.surface)
}

// ----------------------------------------------------------------------------

void wm_mainloop(VulkanContext &vkContext, WindowContext &winContext) {
  while (true) {
    /* handle events */
    xcb_generic_event_t *event = xcb_poll_for_event(winContext.xcb.connection);
    if (event) {
      free(event);
    }

    /**/
    render_frame(vkContext);
  }
}

// ----------------------------------------------------------------------------

/**
* Search for specific Vulkan extensions and fill an array with the found ones.
* @return false if at least one extension has not been found.
*/
bool search_vk_extensions(const VkExtensionProperties vk_extensions[],
                          const unsigned int vk_extension_count,
                          char const* const requested_extensions[],
                          const unsigned int requested_extension_count,
                          std::vector<char const*> &enabled_extensions)
{
  std::vector<bool> bFoundExt(requested_extension_count, false);
  unsigned int enabled_count = 0u;

  /* Search for requested extensions */
  for (unsigned int j = 0u; j < vk_extension_count; ++j) {
    const char* instance_ext_name = vk_extensions[j].extensionName;
    for (unsigned int i = 0u; i < requested_extension_count; ++i) {
      if (!strcmp(requested_extensions[i], instance_ext_name)) {
        // Use Vulkan's constant extension name pointers
        enabled_extensions[enabled_count++] = requested_extensions[i];
        bFoundExt[i] = true;
      }
    }
  }
  enabled_extensions.resize(enabled_count);

  /* Check if the requested extensions were found */
  bool bNotFound = false;
  for (unsigned int i = 0u; i < requested_extension_count; ++i) {
    if (!bFoundExt[i]) {
      fprintf(stderr, "Vulkan error : extensions %s not found.\n", requested_extensions[i]);
      bNotFound = true;
    }
  }
  
  return !bNotFound;
}

// ----------------------------------------------------------------------------

/**
* Set extensions for a Vulkan instance.
*/
void set_vk_instance_extensions(char const* const requested_exts[],
                                const unsigned int requested_exts_count,
                                std::vector<char const *> &enabled_extension_names) 
{
  VkResult err;

  uint32_t instance_ext_count(0u);
  err = vkEnumerateInstanceExtensionProperties(nullptr, &instance_ext_count, nullptr);
  CHECK_VK(err);

  if (instance_ext_count == 0u) {
    fprintf(stderr, "Vulkan error : no instance extensions found.\n");
    exit(EXIT_FAILURE);
  }

  VkExtensionProperties *extensions = new VkExtensionProperties[instance_ext_count];
  enabled_extension_names.resize(instance_ext_count, nullptr);

  err = vkEnumerateInstanceExtensionProperties(nullptr, &instance_ext_count, extensions);
  CHECK_VK(err);

  bool bFound = search_vk_extensions(extensions, instance_ext_count,
                                     requested_exts, requested_exts_count,
                                     enabled_extension_names);
  delete [] extensions;

  if (!bFound) {
    fprintf(stderr, "Vulkan error : requested instance extension(s) not found.\n");
    exit(EXIT_FAILURE);
  }
}

// ----------------------------------------------------------------------------

/**
* Set extensions for a Vulkan device.
*/
void set_vk_device_extensions(char const* const requested_exts[],
                              const unsigned int requested_exts_count,
                              VulkanContext &ctx)
{
  VkResult err;

  uint32_t extension_count(0u);
  err = vkEnumerateDeviceExtensionProperties(ctx.gpu, nullptr, &extension_count, nullptr);
  CHECK_VK(err);

  if (extension_count == 0u) {
    fprintf(stderr, "Vulkan error : no device extensions found.\n");
    exit(EXIT_FAILURE);
  }

  VkExtensionProperties *extensions = new VkExtensionProperties[extension_count];
  ctx.device_extension_names.resize(extension_count);

  err = vkEnumerateDeviceExtensionProperties(ctx.gpu, nullptr, &extension_count, extensions);
  CHECK_VK(err);

  bool bFound = search_vk_extensions(extensions, extension_count,
                                     requested_exts, requested_exts_count,
                                     ctx.device_extension_names);
  delete [] extensions;

  if (!bFound) {
    fprintf(stderr, "Vulkan error : requested device extension(s) not found.\n");
    exit(EXIT_FAILURE);
  }
}

// ----------------------------------------------------------------------------

void retrieve_vk_instance_ext_entrypoints(const VkInstance &inst, VulkanExtensionFP &ext) {
# define GET_INSTANCE_PROC_ADDR(entrypoint)                                     \
    ext.fp##entrypoint = (PFN_vk##entrypoint)                                   \
      vkGetInstanceProcAddr(inst, "vk" #entrypoint);                            \
    if (ext.fp##entrypoint == nullptr) {                                        \
      fprintf(stderr, "Vulkan error : entrypoint " #entrypoint                  \
                      " not found.\n");                                         \
    }

  GET_INSTANCE_PROC_ADDR(GetDeviceProcAddr)
  GET_INSTANCE_PROC_ADDR(GetPhysicalDeviceSurfaceSupportKHR)
  GET_INSTANCE_PROC_ADDR(GetPhysicalDeviceSurfaceCapabilitiesKHR)
  GET_INSTANCE_PROC_ADDR(GetPhysicalDeviceSurfaceFormatsKHR)
  GET_INSTANCE_PROC_ADDR(GetPhysicalDeviceSurfacePresentModesKHR)

# undef GET_INSTANCE_PROC_ADDR
}

// ----------------------------------------------------------------------------

void retrieve_vk_device_ext_entrypoints(const VkDevice &device, VulkanExtensionFP &ext) {
  assert(ext.fpGetDeviceProcAddr != nullptr);

# define GET_DEVICE_PROC_ADDR(entrypoint)                                       \
    ext.fp##entrypoint = (PFN_vk##entrypoint)                                   \
      ext.fpGetDeviceProcAddr(device, "vk" #entrypoint);                        \
    if (ext.fp##entrypoint == nullptr) {                                        \
      fprintf(stderr, "Vulkan error : device entrypoint " #entrypoint           \
                      " not found.\n");                                         \
    }

  GET_DEVICE_PROC_ADDR(CreateSwapchainKHR)
  GET_DEVICE_PROC_ADDR(DestroySwapchainKHR)
  GET_DEVICE_PROC_ADDR(GetSwapchainImagesKHR)
  GET_DEVICE_PROC_ADDR(AcquireNextImageKHR)
  GET_DEVICE_PROC_ADDR(QueuePresentKHR)

# undef GET_DEVICE_PROC_ADDR
}

// ----------------------------------------------------------------------------

// shared between instance and device
static
const char *g_validation_layers[] = {
    //"VK_LAYER_LUNARG_standard_validation",

    
    "VK_LAYER_GOOGLE_threading",       "VK_LAYER_LUNARG_parameter_validation",
    "VK_LAYER_LUNARG_object_tracker",  "VK_LAYER_LUNARG_image",
    "VK_LAYER_LUNARG_core_validation", "VK_LAYER_LUNARG_swapchain",
    "VK_LAYER_GOOGLE_unique_objects"
};

static
const unsigned int g_layer_count = sizeof(g_validation_layers) / sizeof(g_validation_layers[0u]);

// ----------------------------------------------------------------------------

/*
  Setup the Vulkan instance and device with their requested validations layers 
  and extensions.
*/
void init_vk(VulkanContext &ctx) {
  VkResult err;

  /* Set instance's validation layers */
  // see globals

  /* Set instance's extensions */
  const std::array<char const*, 2u> requestedInstanceExts({
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_XCB_SURFACE_EXTENSION_NAME,
  });
  std::vector<char const *> extension_names;
  set_vk_instance_extensions(requestedInstanceExts.data(),
                             requestedInstanceExts.size(),
                             extension_names);

  /* Create a Vulkan instance */
  const VkApplicationInfo app = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = nullptr,
    .pApplicationName = sAppName,
    .applicationVersion = 0,
    .pEngineName = sAppName,
    .engineVersion = 0,
    .apiVersion = VK_API_VERSION_1_0,
  };
  
  VkInstanceCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.pApplicationInfo = &app;
  info.enabledLayerCount = g_layer_count;
  info.ppEnabledLayerNames = g_validation_layers;
  info.enabledExtensionCount = extension_names.size();
  info.ppEnabledExtensionNames = (const char *const *)extension_names.data();

  err = vkCreateInstance(&info, nullptr, &ctx.inst); CHECK_VK(err);


  /* Retrieve the physical device with its properties */
  uint32_t gpu_count(0u);
  err = vkEnumeratePhysicalDevices(ctx.inst, &gpu_count, nullptr); CHECK_VK(err);
  if (gpu_count == 0u) {
    fprintf(stderr, "Vulkan error : no physical device found.\n");
    exit(EXIT_FAILURE);
  }
  VkPhysicalDevice *phDevices = new VkPhysicalDevice[gpu_count];
  err = vkEnumeratePhysicalDevices(ctx.inst, &gpu_count, phDevices); CHECK_VK(err);
  ctx.gpu = phDevices[0u];      // TODO: check the device to use
  delete [] phDevices;
  vkGetPhysicalDeviceProperties(ctx.gpu, &ctx.properties.gpu);

  /* Retrieve the memory properties */
  vkGetPhysicalDeviceMemoryProperties(ctx.gpu, &ctx.properties.memory);

  /* Retrieve device's queue family */
  vkGetPhysicalDeviceQueueFamilyProperties(ctx.gpu, &ctx.queue_count, nullptr);
  if (ctx.queue_count == 0u) {
    fprintf(stderr, "Vulkan error : no queue found for the device.\n");
    exit(EXIT_FAILURE);
  }
  ctx.properties.queue = new VkQueueFamilyProperties[ctx.queue_count];
  vkGetPhysicalDeviceQueueFamilyProperties(ctx.gpu, &ctx.queue_count, ctx.properties.queue);


  /* Check the queue can support graphics operations */
  bool bGraphicsQueueFound = false;
  for (unsigned int i=0u; i < ctx.queue_count; ++i) {
    bGraphicsQueueFound |= ctx.properties.queue[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }
  if (!bGraphicsQueueFound) {
    fprintf(stderr, "Vulkan error : no queue with graphics support found for the device.\n");
    exit(EXIT_FAILURE); 
  }


  /* [Optional] Retrieve device's features */
  //VkPhysicalDeviceFeatures device_features;
  //vkGetPhysicalDeviceFeatures(ctx.gpu, &device_features);

  /* Set device's layers */
  // TODO

  /* Set device's extensions */
  const std::array<char const*, 1u> requestedDeviceExts({
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  });
  set_vk_device_extensions(requestedDeviceExts.data(),
                           requestedDeviceExts.size(),
                           ctx);


  /* Retrieve instance's function pointers */
  retrieve_vk_instance_ext_entrypoints(ctx.inst, ctx.ext);
}

// ----------------------------------------------------------------------------

void init_vk_device(VulkanContext &ctx) {
  VkResult err;

  // The Graphics support is used to render
  // The Surface support is used for display

  /* Retrieve the list of surface support state for queues */
  VkBool32 *surfaceSupport = new VkBool32[ctx.queue_count]();
  for (unsigned int i=0u; i<ctx.queue_count; ++i) {
    ctx.ext.fpGetPhysicalDeviceSurfaceSupportKHR(
      ctx.gpu, i, ctx.surface, &surfaceSupport[i]
    );
  }
  
  /* Search a queue with both graphics and surface presentation support */
  uint32_t valid_queue_index = UINT32_MAX;
  for (unsigned int i=0u; i<ctx.queue_count; ++i) {
    if (    (ctx.properties.queue[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        &&  surfaceSupport[i]) {
      valid_queue_index = i;
      break;
    }
  }
  delete [] surfaceSupport;
  
  if (valid_queue_index == UINT32_MAX) {
    fprintf(stderr, "Vulkan error : no queue were found with both graphics & surface support.\n");
    exit(EXIT_FAILURE);
  }
  ctx.selected_queue_index = valid_queue_index;

  /*------------------------------*/


  /* Create the device with the proper queue */
  {
    float queue_priorities[1u] = {0.0};

    VkDeviceQueueCreateInfo queue;
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.pNext = nullptr;
    queue.flags = 0;
    queue.queueFamilyIndex = ctx.selected_queue_index;
    queue.queueCount = 1u;
    queue.pQueuePriorities = queue_priorities;

    VkDeviceCreateInfo device;
    device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device.pNext = nullptr;
    device.flags = 0;
    device.queueCreateInfoCount = 1u;
    device.pQueueCreateInfos = &queue;
    device.enabledLayerCount = g_layer_count;
    device.ppEnabledLayerNames = g_validation_layers;
    device.enabledExtensionCount = ctx.device_extension_names.size(),
    device.ppEnabledExtensionNames = (const char *const *)ctx.device_extension_names.data();
    device.pEnabledFeatures = nullptr;

    err = vkCreateDevice(ctx.gpu, &device, nullptr, &ctx.device);
    CHECK_VK(err);
  }

  /* Retrieve the device queue */
  vkGetDeviceQueue(ctx.device, ctx.selected_queue_index, 0, &ctx.queue);
  assert(ctx.queue != VK_NULL_HANDLE);

  /* Retrieve device's function pointers */
  retrieve_vk_device_ext_entrypoints(ctx.device, ctx.ext);

  /* Retrieve the surface formats supported */
  uint32_t format_count;
  err = ctx.ext.fpGetPhysicalDeviceSurfaceFormatsKHR(
    ctx.gpu, ctx.surface, &format_count, nullptr
  );
  CHECK_VK(err);

  if (format_count == 0u) {
    fprintf(stderr, "Vulkan error : no surface format found.\n");
    exit(EXIT_FAILURE);
  }

  VkSurfaceFormatKHR *formats = new VkSurfaceFormatKHR[format_count];
  err = ctx.ext.fpGetPhysicalDeviceSurfaceFormatsKHR(
    ctx.gpu, ctx.surface, &format_count, formats
  );
  CHECK_VK(err);

  /* Set surface Format and ColorSpace */
  ctx.format = (formats[0u].format == VK_FORMAT_UNDEFINED) ? VK_FORMAT_B8G8R8A8_UNORM
                                                           : formats[0u].format;
  ctx.color_space = formats[0u].colorSpace;
    
  delete [] formats;
}

// ----------------------------------------------------------------------------

void init_app(VulkanContext &ctx) {
  vec3 eye    = {0.0f, 0.0f, 5.0f};
  vec3 origin = {0.0f, 0.0f, 0.0f};
  vec3 up     = {0.0f, 1.0f, 0.0f};

  mat4x4_perspective(
    ctx.scene.projection,
    static_cast<float>(degreesToRadians(60.0f)),
    ctx.app.width / static_cast<float>(ctx.app.height),
    0.1f,
    500.0f
  );
  
  mat4x4_look_at(ctx.scene.view, eye, origin, up);
  
  mat4x4_identity(ctx.scene.model);
}

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  WindowContext windowContext;
  VulkanContext vkContext;

  vkContext.app.width = 800u;
  vkContext.app.height = 450u;


  /// 1 - Initialize Vulkan / WM

  /* Initialize the window manager */
  init_wm(windowContext);

  /* Initialize Vulkan */
  init_vk(vkContext);

  /* Create a window with a surface bound to Vulkan */
  create_window(vkContext, windowContext);

  /* Initialize the Vulkan device */
  init_vk_device(vkContext);


  /// 2 - Initialize Application datas

  /* Initialize Vulkan objects */
  setup_vk_data(vkContext);

  /* Initialize the application parameters */
  init_app(vkContext);


  /// 3 - Application updates

  /* Mainloop */
  wm_mainloop(vkContext, windowContext);

  /* Clean exit */
  // TODO

  return EXIT_SUCCESS;
}

// ============================================================================
