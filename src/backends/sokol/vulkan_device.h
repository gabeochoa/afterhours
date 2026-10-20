// vulkan_device.h
// Headless Vulkan device creation for the sokol/Vulkan backend.
//
// Include this file from your sokol_impl.cc AFTER the sokol headers
// (sokol_gfx.h with SOKOL_VULKAN + SOKOL_IMPL already included):
//
//   #include <afterhours/src/backends/sokol/vulkan_device.h>
//
// Provides extern "C" implementations called by backend.h (metal_init /
// metal_shutdown). Mirrors sokol_app's Vulkan setup (sokol_app.h
// _sapp_vk_create_instance / _sapp_vk_pick_physical_device /
// _sapp_vk_create_device), minus surface/swapchain: no window, no present.
// On any failure the create function cleans up what it made and returns false.

#pragma once

#if defined(SOKOL_VULKAN) && defined(SOKOL_IMPL)

#include "../../logging.h"

#include <cstring>
#include <vector>

static bool vulkan_has_device_extension(VkPhysicalDevice pdev, const char *name) {
  uint32_t count = 0;
  VkResult res = vkEnumerateDeviceExtensionProperties(pdev, nullptr, &count, nullptr);
  if (res != VK_SUCCESS || count == 0)
    return false;
  std::vector<VkExtensionProperties> exts(count);
  res = vkEnumerateDeviceExtensionProperties(pdev, nullptr, &count, exts.data());
  if (res != VK_SUCCESS)
    return false;
  for (const auto &ext : exts) {
    if (std::strcmp(name, ext.extensionName) == 0)
      return true;
  }
  return false;
}

static bool vulkan_find_queue_family(VkPhysicalDevice pdev, uint32_t *out_index) {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(pdev, &count, nullptr);
  if (count == 0)
    return false;
  std::vector<VkQueueFamilyProperties> props(count);
  vkGetPhysicalDeviceQueueFamilyProperties(pdev, &count, props.data());
  const VkQueueFlags required =
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
  for (uint32_t i = 0; i < count; i++) {
    if ((props[i].queueFlags & required) == required) {
      *out_index = i;
      return true;
    }
  }
  return false;
}

static bool vulkan_create_instance(VkInstance *out) {
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "afterhours-headless";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "afterhours";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_3;

  const char *layer_names[1];
  uint32_t enabled_layer_count = 0;
#if defined(SOKOL_DEBUG)
  layer_names[enabled_layer_count++] = "VK_LAYER_KHRONOS_validation";
#endif
  const char *ext_names[1];
  uint32_t ext_count = 0;
#if defined(SOKOL_DEBUG)
  ext_names[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledLayerCount = enabled_layer_count;
  create_info.ppEnabledLayerNames = layer_names;
  create_info.enabledExtensionCount = ext_count;
  create_info.ppEnabledExtensionNames = ext_names;
  VkResult res = vkCreateInstance(&create_info, nullptr, out);
  if (res != VK_SUCCESS) {
    if (res == VK_ERROR_LAYER_NOT_PRESENT) {
      log_error("vulkan_create_headless_environment: vkCreateInstance failed: "
                "VK_LAYER_KHRONOS_validation not found; install the LunarG "
                "Vulkan SDK for debug builds");
    } else {
      log_error("vulkan_create_headless_environment: vkCreateInstance failed ({})",
                (int)res);
    }
    return false;
  }
  return true;
}

static bool vulkan_pick_physical_device(VkInstance instance, VkPhysicalDevice *out_pdev,
                                       uint32_t *out_queue_family) {
  uint32_t count = 0;
  VkResult res = vkEnumeratePhysicalDevices(instance, &count, nullptr);
  if (res != VK_SUCCESS || count == 0) {
    log_error("vulkan_create_headless_environment: no physical devices found");
    return false;
  }
  std::vector<VkPhysicalDevice> devices(count);
  res = vkEnumeratePhysicalDevices(instance, &count, devices.data());
  if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
    log_error("vulkan_create_headless_environment: vkEnumeratePhysicalDevices "
              "failed ({})",
              (int)res);
    return false;
  }
  VkPhysicalDevice first = VK_NULL_HANDLE;
  uint32_t first_queue = 0;
  VkPhysicalDevice first_discrete = VK_NULL_HANDLE;
  uint32_t discrete_queue = 0;
  for (VkPhysicalDevice pdev : devices) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(pdev, &props);
    if (props.apiVersion < VK_API_VERSION_1_3)
      continue;
    if (!vulkan_has_device_extension(pdev, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME))
      continue;
    uint32_t queue_family = 0;
    if (!vulkan_find_queue_family(pdev, &queue_family))
      continue;
    if (first == VK_NULL_HANDLE) {
      first = pdev;
      first_queue = queue_family;
    }
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        first_discrete == VK_NULL_HANDLE) {
      first_discrete = pdev;
      discrete_queue = queue_family;
    }
  }
  if (first == VK_NULL_HANDLE) {
    log_error("vulkan_create_headless_environment: no suitable physical device "
              "found (need Vulkan 1.3 + VK_EXT_descriptor_buffer + a "
              "graphics/compute/transfer queue)");
    return false;
  }
  if (first_discrete != VK_NULL_HANDLE) {
    *out_pdev = first_discrete;
    *out_queue_family = discrete_queue;
  } else {
    *out_pdev = first;
    *out_queue_family = first_queue;
  }
  return true;
}

static bool vulkan_create_device(VkPhysicalDevice pdev, uint32_t queue_family,
                                 VkDevice *out_device, VkQueue *out_queue) {
  const float queue_priority = 0.0f;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = queue_family;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &queue_priority;

  const char *ext_names[] = {VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME};

  VkPhysicalDeviceFeatures2 supported{};
  supported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  vkGetPhysicalDeviceFeatures2(pdev, &supported);

  VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer{};
  descriptor_buffer.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
  descriptor_buffer.descriptorBuffer = VK_TRUE;

  VkPhysicalDeviceExtendedDynamicStateFeaturesEXT xds{};
  xds.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
  xds.pNext = &descriptor_buffer;
  xds.extendedDynamicState = VK_TRUE;

  VkPhysicalDeviceVulkan12Features vk12{};
  vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vk12.pNext = &xds;
  vk12.bufferDeviceAddress = VK_TRUE;

  VkPhysicalDeviceVulkan13Features vk13{};
  vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vk13.pNext = &vk12;
  vk13.dynamicRendering = VK_TRUE;
  vk13.synchronization2 = VK_TRUE;

  VkPhysicalDeviceFeatures2 required{};
  required.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  required.pNext = &vk13;
  required.features.samplerAnisotropy = VK_TRUE;
  required.features.dualSrcBlend = VK_TRUE;
  if (supported.features.textureCompressionBC)
    required.features.textureCompressionBC = VK_TRUE;
  if (supported.features.textureCompressionETC2)
    required.features.textureCompressionETC2 = VK_TRUE;
  if (supported.features.textureCompressionASTC_LDR)
    required.features.textureCompressionASTC_LDR = VK_TRUE;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = &required;
  create_info.queueCreateInfoCount = 1;
  create_info.pQueueCreateInfos = &queue_info;
  create_info.enabledExtensionCount = 1;
  create_info.ppEnabledExtensionNames = ext_names;

  VkDevice device = VK_NULL_HANDLE;
  VkResult res = vkCreateDevice(pdev, &create_info, nullptr, &device);
  if (res != VK_SUCCESS) {
    log_error("vulkan_create_headless_environment: vkCreateDevice failed ({})",
              (int)res);
    return false;
  }
  VkQueue queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(device, queue_family, 0, &queue);
  *out_device = device;
  *out_queue = queue;
  return true;
}

static VkInstance g_vulkan_headless_instance = VK_NULL_HANDLE;
static VkDevice g_vulkan_headless_device = VK_NULL_HANDLE;

extern "C" bool vulkan_create_headless_environment(sg_vulkan_environment *out) {
  if (!out)
    return false;
  if (g_vulkan_headless_instance != VK_NULL_HANDLE ||
      g_vulkan_headless_device != VK_NULL_HANDLE) {
    log_error("vulkan_create_headless_environment: already created; destroy "
              "first");
    return false;
  }
  VkInstance instance = VK_NULL_HANDLE;
  if (!vulkan_create_instance(&instance))
    return false;
  VkPhysicalDevice pdev = VK_NULL_HANDLE;
  uint32_t queue_family = 0;
  if (!vulkan_pick_physical_device(instance, &pdev, &queue_family)) {
    vkDestroyInstance(instance, nullptr);
    return false;
  }
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  if (!vulkan_create_device(pdev, queue_family, &device, &queue)) {
    vkDestroyInstance(instance, nullptr);
    return false;
  }
  g_vulkan_headless_instance = instance;
  g_vulkan_headless_device = device;
  out->instance = instance;
  out->physical_device = pdev;
  out->device = device;
  out->queue = queue;
  out->queue_family_index = queue_family;
  return true;
}

extern "C" void vulkan_destroy_headless_environment() {
  if (g_vulkan_headless_device != VK_NULL_HANDLE) {
    vkDestroyDevice(g_vulkan_headless_device, nullptr);
    g_vulkan_headless_device = VK_NULL_HANDLE;
  }
  if (g_vulkan_headless_instance != VK_NULL_HANDLE) {
    vkDestroyInstance(g_vulkan_headless_instance, nullptr);
    g_vulkan_headless_instance = VK_NULL_HANDLE;
  }
}

#endif
