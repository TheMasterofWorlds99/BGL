#pragma once

// Include
#include "../queues.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include <vulkan/vulkan_core.h>

// Gloal debug flag to turn things on and off
#ifdef NDEBUG
constexpr bool DebugEnabled = false;
#else
constexpr bool DebugEnabled = true;
#endif

/*

   vkInit.hpp is a simply header that handles all the initalization of vulkan
   and all the checks, whilst also sending data to engine to handle the rest.
   It simply contains all the initalization functions that will be run in
   main.cpp, where these functions take Engine as a reference

*/

// Helper function for seeing if a layer is supported or not based on name
inline bool isSupportedLayer(const char *layerName) {
  uint32_t layerCount = 0;

  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);

  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const auto &layer : availableLayers) {
    if (strcmp(layer.layerName, layerName) == 0)
      return true;
  }

  return false;
}

// Debug callback function for VkDebugUtilsMessengerCreateInfoEXT
inline VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  std::cerr << "Validation Layer: " << pCallbackData->pMessage << "\n";
  return VK_FALSE;
}

// Function to create debugMessengerInfo
inline VkDebugUtilsMessengerCreateInfoEXT createDebugMessengerInfo() {
  VkDebugUtilsMessengerCreateInfoEXT info{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugCallback,
  };

  return info;
}

// Functions to gather extensions and grab layers
inline std::vector<const char *> getExtensions() {
  std::vector<const char *> extensions;
  if (DebugEnabled)
    extensions.push_back(
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Add Support for debug messenger
  // Grab count of glfw extensions and send them to a vector to return
  uint32_t count = 0;
  const char **requiedWindowExtensions =
      glfwGetRequiredInstanceExtensions(&count);

  for (uint32_t i = 0; i < count; i++) {
    std::cout << requiedWindowExtensions[i] << "\n";
    extensions.push_back(requiedWindowExtensions[i]);
  }

  return extensions;
}

inline std::vector<const char *> getLayers() {
  std::vector<const char *> layers;

  if (DebugEnabled) {
    const char *validationName = "VK_LAYER_KHRONOS_validation";
    if (isSupportedLayer(validationName))
      layers.push_back(validationName);
  }

  return layers;
}

// We must first create a Vulkan Instance for engine. During this, we can also
// check GLFW, ensure volk is working, and create the debugutilmessenger for
// engine as well
inline void createVulkanInstance(Engine &engine) {
  // Before calling glfwInit or volkInit, make sure glfw loads the vulkan loader
  glfwInitVulkanLoader(vkGetInstanceProcAddr);

  // Now initalize and check if glfw and volk work
  checkGlfw(glfwInit());
  checkVolk(volkInitialize());

  // Make debugMessengerInfo and grab the extensions and layers for the vulkan
  // instance
  VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo =
      createDebugMessengerInfo();
  std::vector<const char *> extensions = getExtensions();
  std::vector<const char *> layers = getLayers();

  // Create Vk appInfo and use it in our vulkan instance creation for it's info
  VkApplicationInfo appInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .pApplicationName = "Vulkan-Test",
                            .apiVersion = VK_API_VERSION_1_3};

  const VkInstanceCreateInfo instanceCreateInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = &debugMessengerInfo,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
  };

  // Create vulkan instance and give it to engine
  checkVk(
      vkCreateInstance(&instanceCreateInfo, nullptr, &engine.vulkanInstance),
      "Failed to create vulkan instance!");

  // Use volk's load instance to load the instance (duh)
  volkLoadInstance(engine.vulkanInstance);
  if (DebugEnabled) {
    checkVk(vkCreateDebugUtilsMessengerEXT(engine.vulkanInstance,
                                           &debugMessengerInfo, nullptr,
                                           &engine.debugMessenger),
            "Failed to create VkDebugUtilsMessengerEXT");
  }
}

// Now with the vulkan instance created, we can create the GLFW window and the
// vulkan surface
inline void createWindowAndSurface(Engine &engine, const char *title,
                                   int32_t width, int32_t height) {
  // Tell glfw not to create default OpenGL context'
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  GLFWwindow *glfwWindow =
      glfwCreateWindow(width, height, title, nullptr, nullptr);

  if (!glfwWindow) {
    std::cerr << "ERROR: Failed to create Glfw Window!\n";
    exit(EXIT_FAILURE);
  }

  VkSurfaceKHR surface; // Vulkan abstraction over the native window
  checkVk(glfwCreateWindowSurface(engine.vulkanInstance, glfwWindow, nullptr,
                                  &surface),
          "Failed to create window and surface");

  Window window{
      .glfwWindow = glfwWindow,
      .surface = surface,
  };

  engine.windows.push_back(window);

  engine.activeWindow = &engine.windows.back();
}

// After inializing a vk instance, along with our window and surface, we can
// then select our device. For simplicity we'll by default look for discrete
// gpu's and if none are found, chose the first device that works
inline VkPhysicalDevice selectPhysicalDevice(Engine &engine) {
  // Get list of connect GPUs
  uint32_t deviceCount{0};
  checkVk(
      vkEnumeratePhysicalDevices(engine.vulkanInstance, &deviceCount, nullptr),
      "Failed to gather list of all devices");
  std::vector<VkPhysicalDevice> devices(deviceCount);
  checkVk(vkEnumeratePhysicalDevices(engine.vulkanInstance, &deviceCount,
                                     devices.data()),
          "Failed to gather list of all vulkan supporting devices");

  // Choose a GPU, where we want discrete if it's available
  // Later, we we'll do better selection logic
  for (VkPhysicalDevice device : devices) {
    QueueFamilyIndices indicies = findQueueFamilies(device);

    // If we found no families with the ability to send graphics commands, we're
    // sad
    if (!indicies.foundGraphics)
      continue;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    // Store the maxPushConstantSize!
    engine.maxPushConstantSize = properties.limits.maxPushConstantsSize;

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      engine.queueFamilyIndex = indicies.graphicsFamilyIndex;
      return device;
    }
  }

  QueueFamilyIndices indices = findQueueFamilies(devices[0]);

  engine.queueFamilyIndex = indices.graphicsFamilyIndex;

  return devices[0];
}

inline VkDeviceQueueCreateInfo getQueueCreationInfo(Engine &engine) {
  static float queuePriority = 1.0f;

  return VkDeviceQueueCreateInfo{.sType =
                                     VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                 .queueFamilyIndex = engine.queueFamilyIndex,
                                 .queueCount = 1,
                                 .pQueuePriorities = &queuePriority};
}

inline GPUFeatures getDesiredGPUFeatures() { return GPUFeatures{}; }

// With the selectGPU function, we can now create a logical device to
// communicate with it
inline void createLogicalGPUDeviceAndQueue(Engine &engine) {
  // Get desired gpu features + swapcahin extension (to enable drawing to the
  // window)
  GPUFeatures features = getDesiredGPUFeatures();
  const std::vector<const char *> deviceExtensions{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  const VkDeviceQueueCreateInfo queueCreateInfo = getQueueCreationInfo(engine);

  // Logical device creation info
  const VkDeviceCreateInfo deviceCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &features.features12,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueCreateInfo,
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data(),
  };

  checkVk(vkCreateDevice(engine.physicalGPU, &deviceCreateInfo, nullptr,
                         &engine.gpu),
          "Failed to create logical devide!");

  vkGetDeviceQueue(engine.gpu, engine.queueFamilyIndex, 0, &engine.queue);
}

inline void initVMA(Engine &engine) {
  VmaVulkanFunctions vulkanFunctions = {};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = engine.physicalGPU;
  allocatorInfo.device = engine.gpu;
  allocatorInfo.instance = engine.vulkanInstance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  checkVk(vmaCreateAllocator(&allocatorInfo, &engine.allocator),
          "Failed to create VMA allocator!");
}

// Create the command pool for window
inline void createCommandPool(Engine &engine) {
  VkCommandPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags =
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // Allows reseting
                                                           // individual command
                                                           // buffers
      .queueFamilyIndex = engine.queueFamilyIndex};

  checkVk(
      vkCreateCommandPool(engine.gpu, &poolInfo, nullptr, &engine.commandPool),
      "Failed to create command pool");
}

// Create the syncronization objects for the window
inline void createSyncObjects(const Engine &engine, Window &window) {
  VkSemaphoreCreateInfo semaphoreInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                              // Start signaled so the first frame doesn't block
                              // indefinitely waiting for a previous frame
                              .flags = VK_FENCE_CREATE_SIGNALED_BIT};

  // Resize the presentation semaphores to match the swapchain image count
  window.presentationReadySemaphore.resize(window.images.size());

  for (size_t i = 0; i < MaxFramesInFlight; i++) {
    checkVk(vkCreateSemaphore(engine.gpu, &semaphoreInfo, nullptr,
                              &window.imageAcquiredSemaphore[i]),
            "Failed to create acquired semaphore");
    checkVk(vkCreateFence(engine.gpu, &fenceInfo, nullptr,
                          &window.frameFinishedFence[i]),
            "Failed to create frame fence");
  }

  for (size_t i = 0; i < window.images.size(); i++) {
    checkVk(vkCreateSemaphore(engine.gpu, &semaphoreInfo, nullptr,
                              &window.presentationReadySemaphore[i]),
            "Failed to create presentation semaphore");
  }
}

// With the GPUs, families, and command buffer in place, we can then setup the
// swapchain
inline void createSwapChain(const Engine &engine, Window &window) {
  checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
              engine.physicalGPU, window.surface, &window.surfaceCapabilities),
          "Failed to query surface capabilities");

  VkExtent2D swapchainExtent;

  if (window.surfaceCapabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    // The window manager explicitly defined the size
    swapchainExtent = window.surfaceCapabilities.currentExtent;
  } else {
    // The window manager allows us to choose the size (Common on Wayland/X11)
    swapchainExtent = {
        std::clamp(windowWidth(&window),
                   window.surfaceCapabilities.minImageExtent.width,
                   window.surfaceCapabilities.maxImageExtent.width),
        std::clamp(windowHeight(&window),
                   window.surfaceCapabilities.minImageExtent.height,
                   window.surfaceCapabilities.maxImageExtent.height)};
  }
  // Presentation mode determines what happens once an image has been rendered.
  // Some common options are:
  // * VK_PRESENT_MODE_IMMEDIATE_KHR
  //   Presents newly rendered images without waiting. This can cause
  //   screen-tearing, since the screen might be part-way through refreshing the
  //   pixels from one image, when suddenly it's given a new image to read from.
  //   On the plus side, this has the lowest input latency since there's no
  //   waiting around!
  // * VK_PRESENT_MODE_MAILBOX_KHR
  //   Waits for the screen's next refresh (vsync) before presenting a new
  //   image, so no screen-tear occours. This mode presents only the most
  //   recently rendered frame (discarding old ones when rendering outpaces the
  //   screen's refresh rate), which is good for input latency.
  // * VK_PRESENT_MODE_FIFO_KHR
  //   Also waits for the next screen refresh, but each rendered will be
  //   presented in turn, rather than only the most recent. This means that a
  //   (limited) backlog can accumulate if rendering is faster than the screen
  //   can refresh. Such a backlog is worse for input latency, but makes it less
  //   likely that an image has to be shown multiple times (In the case of a
  //   temporary slowdown for example). This is also the only mode with
  //   guarenteed support

  window.swapchainExtent = swapchainExtent;

  VkSwapchainCreateInfoKHR swapchainCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = window.surface,
      .minImageCount = window.surfaceCapabilities.minImageCount,
      .imageFormat = SwapchainImageFormat,
      .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
      .imageExtent = swapchainExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR};

  checkVk(vkCreateSwapchainKHR(engine.gpu, &swapchainCreateInfo, nullptr,
                               &window.swapchain),
          "Failed to create swapchain!");
}

// Next we initalize the swapchain images and imageviews
inline void createSwapchainImagesAndViews(const Engine &engine,
                                          Window &window) {
  // First we fgfet the number of swapchain images
  uint32_t imageCount;
  checkVk(vkGetSwapchainImagesKHR(engine.gpu, window.swapchain, &imageCount,
                                  nullptr),
          "Failed to get swapchain image count");

  // Then we retrieve the images and store them in the window struct
  window.images.resize(imageCount);
  checkVk(vkGetSwapchainImagesKHR(engine.gpu, window.swapchain, &imageCount,
                                  window.images.data()),
          "Failed to get swapchain images");

  // Create an image view for each swapchain image
  window.imageViews.resize(imageCount);
  for (size_t i = 0; i < imageCount; i++) {
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = window.images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = SwapchainImageFormat,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    checkVk(vkCreateImageView(engine.gpu, &viewInfo, nullptr,
                              &window.imageViews[i]),
            "Failed to create image view");
  }
}

// Find a depth format the GPU supports
inline VkFormat getSupportedDepthFormat(VkPhysicalDevice physicalGPU) {
  std::array<VkFormat, 3> candidates = {VK_FORMAT_D32_SFLOAT,
                                        VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        VK_FORMAT_D24_UNORM_S8_UINT};

  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalGPU, format, &props);

    if (props.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      return format;
    }
  }

  std::cerr << "Failed to find supported depth format!\n";
  exit(EXIT_FAILURE);
}

// Simple Extent3D converter
inline VkExtent3D getWindowExtent3D(const Window &window) {
  return VkExtent3D{window.swapchainExtent.width, window.swapchainExtent.height,
                    1};
}

// Create the depth image view
inline void createDepthImageView(const Engine &engine, Window &window) {
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (window.DepthImageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
      window.DepthImageFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
    aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  VkImageViewCreateInfo viewInfo{.sType =
                                     VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                 .image = window.depthImage,
                                 .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                 .format = window.DepthImageFormat,
                                 .subresourceRange = {
                                     .aspectMask = aspectMask,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1,
                                 }};

  checkVk(
      vkCreateImageView(engine.gpu, &viewInfo, nullptr, &window.depthImageView),
      "Failed to create depth image view");
}

// Function to create the depth image
inline void createDepthAndStencilImage(const Engine &engine, Window &window) {
  // Configure the depth (and stencil) image, and alloc te memory for it with
  // vma
  window.DepthImageFormat = getSupportedDepthFormat(engine.physicalGPU);

  VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = window.DepthImageFormat,
                              .extent = getWindowExtent3D(window),
                              .mipLevels = 1,
                              .arrayLayers = 1,
                              .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage =
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

  VmaAllocationCreateInfo allocInfo{
      .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
      .usage = VMA_MEMORY_USAGE_AUTO};

  vmaCreateImage(engine.allocator, &imageInfo, &allocInfo, &window.depthImage,
                 &window.depthAlloc, 0);
  createDepthImageView(engine, window);
}

// -- Cleanup Functions --

// We need to cleanup the window
inline void cleanupWindow(const Engine &engine, Window &window) {
  // We first destroy the depth images
  vkDestroyImageView(engine.gpu, window.depthImageView, nullptr);
  vmaDestroyImage(engine.allocator, window.depthImage, window.depthAlloc);

  // Next destroy the Syncronization objects
  for (size_t i = 0; i < MaxFramesInFlight; i++) {
    vkDestroyFence(engine.gpu, window.frameFinishedFence[i], nullptr);
    vkDestroySemaphore(engine.gpu, window.imageAcquiredSemaphore[i], nullptr);
  }
  for (size_t i = 0; i < window.presentationReadySemaphore.size(); i++) {
    vkDestroySemaphore(engine.gpu, window.presentationReadySemaphore[i],
                       nullptr);
  }

  // We then destroy the image views
  for (auto imageview : window.imageViews) {
    vkDestroyImageView(engine.gpu, imageview, nullptr);
  }

  // Destroy the swapchain and surface
  vkDestroySwapchainKHR(engine.gpu, window.swapchain, nullptr);
  vkDestroySurfaceKHR(engine.vulkanInstance, window.surface, nullptr);

  // Destory the OS window
  glfwDestroyWindow(window.glfwWindow);
}

// This function cleans up the entire engine
inline void cleanupEngine(Engine &engine) {
  // Wait for the GPU to finish whatever it's doing before we rip the memory out
  // from under it
  vkDeviceWaitIdle(engine.gpu);

  // Clean up all windows
  for (auto &window : engine.windows) {
    cleanupWindow(engine, window);
  }

  // Destroy Command Pool (this automatically frees all command buffers)
  vkDestroyCommandPool(engine.gpu, engine.commandPool, nullptr);

  // Destroy VMA
  vmaDestroyAllocator(engine.allocator);

  // Destroy Logical Device
  vkDestroyDevice(engine.gpu, nullptr);

  // Destroy Debug Messenger
  if (DebugEnabled && engine.debugMessenger != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(engine.vulkanInstance,
                                    engine.debugMessenger, nullptr);
  }

  // Destroy Vulkan Instance
  vkDestroyInstance(engine.vulkanInstance, nullptr);

  // Terminate GLFW
  glfwTerminate();
}
