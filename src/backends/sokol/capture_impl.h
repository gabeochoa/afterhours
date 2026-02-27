// capture_impl.h
// Metal texture readback and PNG export for the sokol/Metal backend.
//
// Include this file from your sokol_impl.mm AFTER the sokol headers:
//
//   #include <afterhours/src/backends/sokol/capture_impl.h>
//
// Provides extern "C" implementations called by drawing_helpers.h.

#pragma once

#ifdef AFTER_HOURS_USE_METAL

#import <Metal/Metal.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

// sokol_gfx must already be included (with SOKOL_IMPL) before this header.

// Resolve an MSAA texture to a non-MSAA texture using a blit command encoder.
// Returns nil if the source is already non-MSAA (caller should read directly).
static id<MTLTexture> metal_resolve_msaa(id<MTLTexture> src) {
  if ([src sampleCount] <= 1)
    return nil;

  MTLTextureDescriptor *desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:[src pixelFormat]
                                   width:[src width]
                                  height:[src height]
                               mipmapped:NO];
  desc.usage = MTLTextureUsageShaderRead;
  desc.storageMode = MTLStorageModeShared;

  id<MTLDevice> device = (__bridge id<MTLDevice>)sg_mtl_device();
  id<MTLTexture> resolved = [device newTextureWithDescriptor:desc];
  if (!resolved)
    return nil;

  id<MTLCommandQueue> queue =
      (__bridge id<MTLCommandQueue>)sg_mtl_command_queue();
  id<MTLCommandBuffer> cmd = [queue commandBuffer];
  id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];

  [blit copyFromTexture:src
            sourceSlice:0
            sourceLevel:0
           sourceOrigin:MTLOriginMake(0, 0, 0)
             sourceSize:MTLSizeMake([src width], [src height], 1)
              toTexture:resolved
       destinationSlice:0
       destinationLevel:0
      destinationOrigin:MTLOriginMake(0, 0, 0)];

  [blit endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];
  return resolved;
}

// Read pixels from a sokol sg_image (color attachment / render texture).
// Returns an RGBA8 pixel buffer. Caller must free() the returned pointer.
// Returns nullptr on failure. Handles both MSAA and non-MSAA textures.
static uint8_t *metal_read_image_pixels(sg_image img_id, int width,
                                        int height) {
  sg_mtl_image_info info = sg_mtl_query_image_info(img_id);
  id<MTLTexture> tex =
      (__bridge id<MTLTexture>)info.tex[info.active_slot];
  if (!tex)
    return nullptr;

  // If MSAA, resolve to a single-sample texture first.
  id<MTLTexture> resolved = metal_resolve_msaa(tex);
  id<MTLTexture> read_tex = resolved ? resolved : tex;

  size_t bpp = 4;
  size_t row_bytes = static_cast<size_t>(width) * bpp;
  auto *bgra = static_cast<uint8_t *>(
      malloc(row_bytes * static_cast<size_t>(height)));
  if (!bgra)
    return nullptr;

  MTLRegion region = MTLRegionMake2D(0, 0,
                                     static_cast<NSUInteger>(width),
                                     static_cast<NSUInteger>(height));
  [read_tex getBytes:bgra
         bytesPerRow:row_bytes
          fromRegion:region
         mipmapLevel:0];

  // BGRA -> RGBA in-place
  size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
  for (size_t i = 0; i < total; i++) {
    size_t off = i * 4;
    uint8_t tmp = bgra[off];
    bgra[off] = bgra[off + 2];
    bgra[off + 2] = tmp;
  }

  return bgra;
}

// Write RGBA pixel data to a PNG file using CoreGraphics/ImageIO.
static bool metal_write_png(const char *path, const uint8_t *rgba,
                            int width, int height) {
  size_t bpp = 4;
  size_t row_bytes = static_cast<size_t>(width) * bpp;

  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx = CGBitmapContextCreate(
      const_cast<uint8_t *>(rgba),
      static_cast<size_t>(width), static_cast<size_t>(height),
      8, row_bytes, cs,
      kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
  CGColorSpaceRelease(cs);
  if (!ctx)
    return false;

  CGImageRef cg_img = CGBitmapContextCreateImage(ctx);
  CGContextRelease(ctx);
  if (!cg_img)
    return false;

  @autoreleasepool {
    NSString *ns_path = [NSString stringWithUTF8String:path];
    NSURL *url = [NSURL fileURLWithPath:ns_path];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(
        (__bridge CFURLRef)url, kUTTypePNG, 1, NULL);
    if (!dest) {
      CGImageRelease(cg_img);
      return false;
    }
    CGImageDestinationAddImage(dest, cg_img, NULL);
    bool ok = CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CGImageRelease(cg_img);
    return ok;
  }
}

// ── Extern C entry points called from drawing_helpers.h ──

extern "C" bool metal_capture_render_texture(uint32_t color_img_id,
                                             int width, int height,
                                             const char *path) {
  sg_image img = {color_img_id};
  uint8_t *rgba = metal_read_image_pixels(img, width, height);
  if (!rgba)
    return false;
  bool ok = metal_write_png(path, rgba, width, height);
  free(rgba);
  return ok;
}

extern "C" int metal_capture_render_texture_to_memory(
    uint32_t color_img_id, int width, int height,
    uint8_t **out_data, int *out_size) {
  sg_image img = {color_img_id};
  uint8_t *rgba = metal_read_image_pixels(img, width, height);
  if (!rgba)
    return 0;
  size_t data_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
  *out_data = rgba;
  *out_size = static_cast<int>(data_size);
  return 1;
}

#endif // AFTER_HOURS_USE_METAL
