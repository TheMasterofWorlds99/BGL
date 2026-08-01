// This one TU provides the volk and VMA implementations, compiled into the
// engine library. Executables link the library, so nobody has to repeat the
// implementation defines in their own main.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
