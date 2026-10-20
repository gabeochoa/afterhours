// capture_vulkan.h
// Vulkan texture readback and PNG export for the sokol/Vulkan backend.
//
// Include this file from your sokol_impl.cc AFTER the sokol headers:
//
//   #include <afterhours/src/backends/sokol/capture_vulkan.h>
//
// Provides extern "C" implementations called by drawing_helpers.h. PNG uses
// stb_image_write (no CoreGraphics on Windows); STB_IMAGE_WRITE_IMPLEMENTATION
// is defined here only. Reads sokol's internals (_sg_lookup_image, _sg.vk,
// _sg_vk_image_layout, _sg_vk_*_mask, _sg_vk_format,
// _sg_vk_mem_find_memory_type_index) directly; reachable because this header
// lives in the SOKOL_IMPL TU.

#pragma once

#if defined(SOKOL_VULKAN) && defined(SOKOL_IMPL)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <cstdint>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <vector>

static void vulkan_image_barrier(VkCommandBuffer cmd, VkImage image,
                                 VkImageLayout old_layout, VkImageLayout new_layout,
                                 VkPipelineStageFlags2 src_stage,
                                 VkAccessFlags2 src_access,
                                 VkPipelineStageFlags2 dst_stage,
                                 VkAccessFlags2 dst_access) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask = src_stage;
  barrier.srcAccessMask = src_access;
  barrier.oldLayout = old_layout;
  barrier.dstStageMask = dst_stage;
  barrier.dstAccessMask = dst_access;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(cmd, &dep);
}

static bool vulkan_alloc_memory(VkMemoryRequirements reqs,
                                VkMemoryPropertyFlags props,
                                VkDeviceMemory *out_mem) {
  int index = _sg_vk_mem_find_memory_type_index(reqs.memoryTypeBits, props);
  if (index < 0)
    return false;
  VkMemoryAllocateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.allocationSize = reqs.size;
  info.memoryTypeIndex = static_cast<uint32_t>(index);
  return vkAllocateMemory(_sg.vk.dev, &info, nullptr, out_mem) == VK_SUCCESS;
}

static bool vulkan_write_png(const char *path, const uint8_t *rgba,
                             int width, int height) {
  return stbi_write_png(path, width, height, 4, rgba, width * 4) != 0;
}

// Read pixels from a sokol sg_image (color attachment / render texture).
// Returns an RGBA8 pixel buffer. Caller must free() the returned pointer.
// Returns nullptr on failure. Handles both MSAA and non-MSAA textures.
static uint8_t *vulkan_read_image_pixels(uint32_t img_id, int width, int height) {
  // Guard invalid dimensions: width/height <= 0 would malloc(0) + copy a
  // zero/negative extent, and a huge width*height*4 can overflow size_t and
  // under-allocate, so the copy then writes past the buffer (heap corruption).
  if (width <= 0 || height <= 0)
    return nullptr;
  const size_t px = static_cast<size_t>(width) * static_cast<size_t>(height);
  if (px > SIZE_MAX / 4u) // width*height*4 (RGBA8) would overflow
    return nullptr;

  _sg_image_t *img = _sg_lookup_image(img_id);
  if (!img || !img->vk.img)
    return nullptr;
  if (img->cmn.width != width || img->cmn.height != height)
    return nullptr;
  // Only 4-byte RGBA/BGRA formats: the staging buffer is width*height*4, and
  // copying any other format into it would overflow.
  if (img->cmn.pixel_format != SG_PIXELFORMAT_RGBA8 &&
      img->cmn.pixel_format != SG_PIXELFORMAT_BGRA8)
    return nullptr;

  VkDevice dev = _sg.vk.dev;
  if (vkQueueWaitIdle(_sg.vk.queue) != VK_SUCCESS)
    return nullptr;

  const VkDeviceSize buf_size =
      static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
  const bool msaa = img->cmn.sample_count > 1;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory buffer_mem = VK_NULL_HANDLE;
  VkImage temp_img = VK_NULL_HANDLE;
  VkDeviceMemory temp_mem = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  uint8_t *out = nullptr;

  {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = buf_size;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &info, nullptr, &buffer) != VK_SUCCESS)
      goto done;
  }
  {
    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(dev, buffer, &reqs);
    if (!vulkan_alloc_memory(reqs,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             &buffer_mem))
      goto done;
    if (vkBindBufferMemory(dev, buffer, buffer_mem, 0) != VK_SUCCESS)
      goto done;
  }
  if (msaa) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = _sg_vk_format(img->cmn.pixel_format);
    info.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(dev, &info, nullptr, &temp_img) != VK_SUCCESS)
      goto done;
    VkMemoryRequirements reqs{};
    vkGetImageMemoryRequirements(dev, temp_img, &reqs);
    if (!vulkan_alloc_memory(reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             &temp_mem))
      goto done;
    if (vkBindImageMemory(dev, temp_img, temp_mem, 0) != VK_SUCCESS)
      goto done;
  }
  {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = _sg.vk.queue_family_index;
    if (vkCreateCommandPool(dev, &info, nullptr, &pool) != VK_SUCCESS)
      goto done;
  }
  {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(dev, &info, &cmd) != VK_SUCCESS)
      goto done;
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
      goto done;
  }
  {
    const VkImageLayout src_layout = _sg_vk_image_layout(img->vk.cur_access);
    const _sg_vk_access_t orig_access = img->vk.cur_access;
    VkImage copy_src = img->vk.img;
    if (msaa) {
      vulkan_image_barrier(cmd, temp_img, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                           VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT);
      vulkan_image_barrier(cmd, img->vk.img, src_layout,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           _sg_vk_src_stage_mask(orig_access),
                           _sg_vk_src_access_mask(orig_access),
                           VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_ACCESS_2_TRANSFER_READ_BIT);
      VkImageResolve resolve{};
      resolve.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      resolve.srcOffset = {0, 0, 0};
      resolve.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      resolve.dstOffset = {0, 0, 0};
      resolve.extent = {static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height), 1};
      vkCmdResolveImage(cmd, img->vk.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        temp_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                        &resolve);
      vulkan_image_barrier(cmd, img->vk.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           src_layout, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_ACCESS_2_TRANSFER_READ_BIT,
                           _sg_vk_dst_stage_mask(orig_access),
                           _sg_vk_dst_access_mask(orig_access));
      vulkan_image_barrier(cmd, temp_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_ACCESS_2_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_ACCESS_2_TRANSFER_READ_BIT);
      copy_src = temp_img;
    } else {
      vulkan_image_barrier(cmd, img->vk.img, src_layout,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           _sg_vk_src_stage_mask(orig_access),
                           _sg_vk_src_access_mask(orig_access),
                           VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_ACCESS_2_TRANSFER_READ_BIT);
    }
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height), 1};
    vkCmdCopyImageToBuffer(cmd, copy_src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer, 1, &region);
    if (!msaa) {
      vulkan_image_barrier(cmd, img->vk.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           src_layout, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                           VK_ACCESS_2_TRANSFER_READ_BIT,
                           _sg_vk_dst_stage_mask(orig_access),
                           _sg_vk_dst_access_mask(orig_access));
    }
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
      goto done;
  }
  {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(dev, &info, nullptr, &fence) != VK_SUCCESS)
      goto done;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    if (vkQueueSubmit(_sg.vk.queue, 1, &submit, fence) != VK_SUCCESS)
      goto done;
    if (vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
      goto done;
  }
  {
    void *mapped = nullptr;
    if (vkMapMemory(dev, buffer_mem, 0, buf_size, 0, &mapped) != VK_SUCCESS)
      goto done;
    uint8_t *pixels = static_cast<uint8_t *>(malloc(static_cast<size_t>(buf_size)));
    if (!pixels) {
      vkUnmapMemory(dev, buffer_mem);
      goto done;
    }
    std::memcpy(pixels, mapped, static_cast<size_t>(buf_size));
    vkUnmapMemory(dev, buffer_mem);
    if (img->cmn.pixel_format == SG_PIXELFORMAT_BGRA8) {
      for (size_t i = 0; i < px; i++) {
        uint8_t tmp = pixels[i * 4];
        pixels[i * 4] = pixels[i * 4 + 2];
        pixels[i * 4 + 2] = tmp;
      }
    }
    out = pixels;
  }
done:
  if (fence != VK_NULL_HANDLE)
    vkDestroyFence(dev, fence, nullptr);
  if (cmd != VK_NULL_HANDLE)
    vkFreeCommandBuffers(dev, pool, 1, &cmd);
  if (pool != VK_NULL_HANDLE)
    vkDestroyCommandPool(dev, pool, nullptr);
  if (temp_img != VK_NULL_HANDLE)
    vkDestroyImage(dev, temp_img, nullptr);
  if (temp_mem != VK_NULL_HANDLE)
    vkFreeMemory(dev, temp_mem, nullptr);
  if (buffer != VK_NULL_HANDLE)
    vkDestroyBuffer(dev, buffer, nullptr);
  if (buffer_mem != VK_NULL_HANDLE)
    vkFreeMemory(dev, buffer_mem, nullptr);
  return out;
}

// ── Extern C entry points called from drawing_helpers.h ──

extern "C" bool vulkan_capture_render_texture(uint32_t color_img_id,
                                              int width, int height,
                                              const char *path) {
  uint8_t *rgba = vulkan_read_image_pixels(color_img_id, width, height);
  if (!rgba)
    return false;
  bool ok = vulkan_write_png(path, rgba, width, height);
  free(rgba);
  return ok;
}

extern "C" int vulkan_capture_render_texture_to_memory(
    uint32_t color_img_id, int width, int height,
    uint8_t **out_data, int *out_size) {
  if (width <= 0 || height <= 0 ||
      static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4 > INT_MAX)
    return 0;
  uint8_t *rgba = vulkan_read_image_pixels(color_img_id, width, height);
  if (!rgba)
    return 0;
  size_t data_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
  *out_data = rgba;
  *out_size = static_cast<int>(data_size);
  return 1;
}

extern "C" int vulkan_capture_render_texture_png_to_memory(
    uint32_t color_img_id, int width, int height,
    uint8_t **out_data, int *out_size) {
  uint8_t *rgba = nullptr;
  int rgba_size = 0;
  if (!vulkan_capture_render_texture_to_memory(color_img_id, width, height,
                                               &rgba, &rgba_size)) return 0;
  std::vector<uint8_t> png;
  auto write_func = [](void *context, void *data, int size) {
    auto *vec = static_cast<std::vector<uint8_t> *>(context);
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    vec->insert(vec->end(), bytes, bytes + size);
  };
  if (!stbi_write_png_to_func(write_func, &png, width, height, 4, rgba,
                              width * 4)) {
    free(rgba);
    return 0;
  }
  free(rgba);
  if (png.empty() || png.size() > INT_MAX)
    return 0;
  uint8_t *bytes = static_cast<uint8_t *>(malloc(png.size()));
  if (!bytes)
    return 0;
  std::memcpy(bytes, png.data(), png.size());
  *out_data = bytes;
  *out_size = static_cast<int>(png.size());
  return 1;
}

#endif
