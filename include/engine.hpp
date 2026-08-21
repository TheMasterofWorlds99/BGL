#pragma once

// Volk before glfw
#include <volk.h>

// VMA after volk
#include <vk_mem_alloc.h>

// Other Includes
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <glm/glm.hpp>
#include <vector>

/*

   Engine.hpp holds the primary engine class, which will handle all the vulkan
   functions, glfw windows, etc.

*/

// -- IMPORTANT CONSTANTS --
constexpr uint32_t MaxFramesInFlight = 2;
constexpr VkFormat SwapchainImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
constexpr uint64_t Timeout = UINT64_MAX;

// Modifiable Settings
inline bool CullBackFace = true;

// Enum class for the different present modes
enum class PresentMode { VSync, Immediate, Mailbox };

// Basic Window struct helper for the engine struct
struct Window {
  GLFWwindow *glfwWindow; // native window
  VkSurfaceKHR surface;   // Abstraction over native window
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  VkSwapchainKHR swapchain; // Provides access to images (via presentation
                            // engine) to render to
  VkExtent2D swapchainExtent;

  // The color format the swapchain was created with (set at creation; read
  // by pipelines so they always match the actual swapchain)
  VkFormat swapchainFormat = VK_FORMAT_R8G8B8A8_SRGB;

  // MSAA sample count the swapchain/pass were created with (1 = off)
  VkSampleCountFlagBits MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  // MSAA color target: rendering goes here, then resolves into the swapchain
  VkImage msaaImage;
  VkImageView msaaImageView;
  VmaAllocation msaaAlloc;

  std::vector<VkImage> images; // swapchain images
  std::vector<VkImageView> imageViews;

  // Depth image format
  VkFormat DepthImageFormat = VK_FORMAT_D32_SFLOAT;

  VkImage depthImage;
  VkImageView depthImageView;
  VmaAllocation depthAlloc;

  // Index of image to target for rendering (can be out of order, so request
  // from swapchain)
  uint32_t activeImageIndex;
  // Current index for reusable frame resources (cmd buffers, syncronization
  // objects) If, for example, 2 frames in flight are used, this value will go:
  // 0, 1, 0, 1, etc.
  uint32_t frameInFlightIndex;

  // Command buffers
  std::array<VkCommandBuffer, MaxFramesInFlight> commandBuffers;

  // -- Synchronization objects for presentation
  // Signalled by presentation engine whem image is available for gpu to start
  // rendering
  std::array<VkSemaphore, MaxFramesInFlight> imageAcquiredSemaphore;
  // Signalled by gpu once commands have been executed, meaning the image can be
  // presented
  std::vector<VkSemaphore> presentationReadySemaphore; // Once per image
  // Signalled by gpu once commands have been executed, meaning the cpu can
  // reuse that frame's resources
  std::array<VkFence, MaxFramesInFlight> frameFinishedFence;
};

// Settings struct to allow for easy modifying of the engine

// Storage for one arbitrary VkPhysicalDevice*Features struct. Every Vulkan
// feature struct starts with sType (offset 0) and pNext (offset 8), so the
// engine can chain any of them without knowing the concrete type.
struct FeatureBlock {
  alignas(16) std::array<uint8_t, 128> data;
  size_t size = 0;
};

// A list of arbitrary Vulkan feature structs to chain into vkCreateDevice's
// pNext. Request features with the typed request<T> helper — no need to ever
// edit BGL to enable a new feature; just request it here.
struct FeatureRequests {
  std::deque<FeatureBlock> storage; // deque: stable addresses as it grows

  // Copy any VkPhysicalDevice*Features struct into the list and return a
  // reference to the stored copy (so the caller can tweak it after). Remember
  // to set .sType yourself — that's the one contract.
  template <typename T>
  T &request(const T &features) {
    static_assert(sizeof(T) <= 128, "feature struct too large for the block");
    storage.push_back(FeatureBlock{});
    auto &block = storage.back();
    std::memcpy(block.data.data(), &features, sizeof(T));
    block.size = sizeof(T);
    return *reinterpret_cast<T *>(block.data.data());
  }
};

struct EngineSettings {
  PresentMode presentMode = PresentMode::VSync;
  VkFormat swapchainFormat = VK_FORMAT_R8G8B8A8_SRGB;
  VkSampleCountFlagBits MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  bool validation = true;

  FeatureRequests features; // arbitrary feature structs (ray query, BLAS, ...)
  std::vector<const char *> deviceExtensions; // extra extensions (swapchain
                                              // is always added)
};

// Window helpers
inline uint32_t windowWidth(const Window *window) {
  int width, height;
  glfwGetFramebufferSize(window->glfwWindow, &width, &height);
  return static_cast<uint32_t>(width);
}

inline uint32_t windowHeight(const Window *window) {
  int width, height;
  glfwGetFramebufferSize(window->glfwWindow, &width, &height);
  return static_cast<uint32_t>(height);
}

// True if the OS framebuffer size no longer matches the swapchain's extent
// (i.e. the window was resized). Used to trigger swapchain recreation.
inline bool windowNeedsResize(Window *window) {
  int width, height;
  glfwGetFramebufferSize(window->glfwWindow, &width, &height);
  return width > 0 && height > 0 &&
         (static_cast<uint32_t>(width) != window->swapchainExtent.width ||
          static_cast<uint32_t>(height) != window->swapchainExtent.height);
}

// GPU Features struct for handling the tons of features vulkan can offer
struct GPUFeatures {
  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE,
  };

  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &features13, // Chain it to the 1.3 features!
      .bufferDeviceAddress = VK_TRUE,
  };
};

// Timer struct to hold ticks and other stuff
struct GlobalTimer {
  uint32_t tickCount = 0;
  float deltaTime = 0;
  double lastTime = -1;
};

// Key State struct to handle keyDown and KeyUp states for all keys
struct KeyState {
  uint32_t keyDownFrame = 0;
  uint32_t keyUpFrame = 0;
};

// Engine structure
struct Engine {
public:
  // How the engine was configured at init (present mode, formats, ...)
  EngineSettings settings;

  std::vector<Window>
      windows; // A list of windows that Engine will be able to handle

  Window *activeWindow = nullptr; // Window the engine is focusing on

  // Vulkan types that engine would use
  VkInstance vulkanInstance;               // Vulkan Instance
  VkDebugUtilsMessengerEXT debugMessenger; // debugMessenger

  // Vulkan GPUs
  VkPhysicalDevice physicalGPU;
  VkDevice gpu;

  // Family Queue Stuff
  uint32_t queueFamilyIndex;
  VkQueue queue;

  // Command Stuff
  VkCommandPool commandPool;

  // VMA Allocator
  VmaAllocator allocator;

  // Global Timer
  GlobalTimer globalTimer;

  // Key State array for input states
  std::array<KeyState, GLFW_KEY_LAST + 1> inputStates;

  // A quick push constant size holder so no matter the GPU, the engine knows
  // the maximum gaurenteed memory that it can use for push constants
  uint32_t maxPushConstantSize =
      128; // By default, vulkan only supports a maximum of 128 bytes

private:
};

// Function to tick the global timer
void tickTimer(Engine &engine);

// Helper Functions for rendering and framing
void viewportAndScissorCmd(VkCommandBuffer cmdBuffer, Window *window);

// Rendering and Frame functions
void beginRenderPass(VkCommandBuffer cmdBuffer, Window *window, glm::vec4 col);

// Frame functions
// Prepare the frame: wait for the frame-in-flight fence, acquire the next
// swapchain image, and reset/begin the command buffer for recording. The
// render pass is NOT started, so callers that need to record commands before
// rendering (e.g. a compute pass) can do so, then call beginRenderPass.
VkCommandBuffer beginFrameCommandBuffer(const Engine &engine, Window *window);

void beginFrame(const Engine &engine, Window *window, glm::vec4 col);

void submitCommands(VkQueue queue, VkCommandBuffer cmdBuffer, Window *window);

void endFrame(const Engine &engine, Window *window);
