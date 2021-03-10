/*
 * Copyright (C) 2021 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gapii/cc/vulkan_external_memory.h"
#include "gapii/cc/vulkan_extras.h"
#include "gapii/cc/vulkan_spy.h"
#include "gapis/api/vulkan/vulkan_pb/extras.pb.h"

#define LOG_ERROR(...) GAPID_ERROR("[External Memory] " __VA_ARGS__)

namespace gapii {

ExternalMemory::ExternalMemory(VulkanSpy* spy, CallObserver* observer,
                               VkQueue queue, uint32_t submitCount,
                               const VkSubmitInfo* pSubmits, VkFence fence)
    : spy(spy), observer(observer), queue(queue), fence(fence) {
  QueueObject& queueObj = *spy->mState.Queues[queue];
  queueFamily = queueObj.mFamily;
  device = queueObj.mDevice;
  fn = &spy->mImports.mVkDeviceFunctions[device];

  stagingSize = 0;
  submits.resize(submitCount);
  for (uint32_t i = 0; i < submitCount; i++) {
    submits[i].submitInfo = &pSubmits[i];
    uint32_t commandBufferCount = pSubmits[i].mcommandBufferCount;
    submits[i].commandBuffers.resize(commandBufferCount);
    for (uint32_t j = 0; j < commandBufferCount; j++) {
      ExternalMemoryCommandBuffer& cmdBuf = submits[i].commandBuffers[j];
      cmdBuf.commandBuffer = pSubmits[i].mpCommandBuffers[j];
      auto bufIt = spy->mExternalBufferBarriers.find(cmdBuf.commandBuffer);
      if (bufIt != spy->mExternalBufferBarriers.end()) {
        for (const auto& barrier : bufIt->second) {
          cmdBuf.buffers.push_back(
              ExternalBufferMemoryStaging(barrier, stagingSize));
        }
      }

      auto imgIt = spy->mExternalImageBarriers.find(cmdBuf.commandBuffer);
      if (imgIt != spy->mExternalImageBarriers.end()) {
        for (const auto& barrier : imgIt->second) {
          ExternalImageMemoryStaging imgStaging(barrier);
          imgStaging.copies =
              spy->BufferImageCopies(spy->mState.Images[barrier.mimage],
                                     barrier.msubresourceRange, stagingSize);
          cmdBuf.images.push_back(std::move(imgStaging));
        }
      }
    }
  }
}

uint32_t ExternalMemory::CreateResources() {
  VkCommandPoolCreateInfo commandPoolCreateInfo{
      VkStructureType::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      nullptr,  // pNext
      VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      queueFamily,
  };
  uint32_t res = fn->vkCreateCommandPool(device, &commandPoolCreateInfo,
                                         nullptr, &stagingCommandPool);
  if (res != VkResult::VK_SUCCESS) {
    stagingCommandPool = 0;
    LOG_ERROR("Error creating command pool: %x", res);
    return res;
  }

  if (fence == 0) {
    VkFenceCreateInfo fenceCreateInfo{
        VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        nullptr,  // pNext
        0,        // flags
    };
    res = fn->vkCreateFence(device, &fenceCreateInfo, nullptr, &stagingFence);
    if (res != VkResult::VK_SUCCESS) {
      stagingFence = 0;
      LOG_ERROR("Error creating fence: %x", res);
      return res;
    }
    fence = stagingFence;
  }

  size_t cmdBufCount = 1;
  for (const auto& submit : submits) {
    for (const auto& cmdBuf : submit.commandBuffers) {
      if (!cmdBuf.empty()) {
        cmdBufCount++;
      }
    }
  }
  std::vector<VkCommandBuffer> commandBuffers(cmdBufCount);
  VkCommandBufferAllocateInfo commandBufferAllocInfo{
      VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      nullptr,  // pNext
      stagingCommandPool,
      VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      (uint32_t)commandBuffers.size(),
  };
  res = fn->vkAllocateCommandBuffers(device, &commandBufferAllocInfo,
                                     commandBuffers.data());
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Error allocating command buffer: %x", res);
    return res;
  }
  for (auto cmdBuf : commandBuffers) {
    set_dispatch_from_parent((void*)cmdBuf, (void*)device);
  }

  stagingCommandBuffer = commandBuffers.back();
  commandBuffers.pop_back();
  for (auto& submit : submits) {
    for (auto& cmdBuf : submit.commandBuffers) {
      if (!cmdBuf.empty()) {
        cmdBuf.stagingCommandBuffer = commandBuffers.back();
        commandBuffers.pop_back();
      }
    }
  }

  VkBufferCreateInfo bufferCreateInfo = {
      VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      nullptr,  // pNext
      0,        // flags
      stagingSize,
      VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VkSharingMode::VK_SHARING_MODE_EXCLUSIVE,
      0,        // queueFamilyIndexCount
      nullptr,  // pQueueFamilyIndices
  };
  res = fn->vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer);
  if (res != VkResult::VK_SUCCESS) {
    stagingBuffer = 0;
    LOG_ERROR("Failed creating staging buffer: %x", res);
    return res;
  }

  VkMemoryRequirements memReqs(spy->arena());
  fn->vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

  VkPhysicalDevice physDevice = spy->mState.Devices[device]->mPhysicalDevice;
  VkPhysicalDeviceMemoryProperties memProps =
      spy->mState.PhysicalDevices[physDevice]->mMemoryProperties;
  uint32_t memoryTypeIndex =
      GetMemoryTypeIndexForStagingResources(memProps, memReqs.mmemoryTypeBits);
  if (memoryTypeIndex == kInvalidMemoryTypeIndex) {
    LOG_ERROR("Failed finding memory type index");
    return res;
  }

  VkMemoryAllocateInfo memoryAllocInfo = {
      VkStructureType::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      nullptr,  // pNext
      memReqs.msize,
      memoryTypeIndex,
  };
  res = fn->vkAllocateMemory(device, &memoryAllocInfo, nullptr, &stagingMemory);
  if (res != VkResult::VK_SUCCESS) {
    stagingMemory = 0;
    LOG_ERROR("Failed allocating staging buffer memory: %x", res);
    return res;
  }

  res = fn->vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Failed binding staging buffer: %x", res);
    return res;
  }

  return VkResult::VK_SUCCESS;
}

uint32_t ExternalMemory::RecordCommandBuffers() {
  for (auto submitIt = submits.begin(); submitIt != submits.end(); submitIt++) {
    for (auto cmdBufIt = submitIt->commandBuffers.begin();
         cmdBufIt != submitIt->commandBuffers.end(); cmdBufIt++) {
      if (!cmdBufIt->empty()) {
        uint32_t res = RecordStagingCommandBuffer(*cmdBufIt);
        if (res != VkResult::VK_SUCCESS) {
          return res;
        }
      }
    }
  }

  VkCommandBufferBeginInfo beginInfo{
      VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,  // pNext
      VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr,  // pInheritanceInfo
  };
  uint32_t res = fn->vkBeginCommandBuffer(stagingCommandBuffer, &beginInfo);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Failed to begin command buffer: %x", res);
    return res;
  }

  // Make staging buffer writes visible to the host
  VkBufferMemoryBarrier barrier{
      VkStructureType::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      nullptr,  // pNext
      VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT,
      VkAccessFlagBits::VK_ACCESS_HOST_READ_BIT,
      queueFamily,
      queueFamily,
      stagingBuffer,
      0,  // offset
      stagingSize,
  };

  fn->vkCmdPipelineBarrier(
      stagingCommandBuffer,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_HOST_BIT,
      0,         // dependencyFlags
      0,         // memoryBarrierCount
      nullptr,   // pMemoryBarriers
      1,         // bufferMemoryBarrierCount
      &barrier,  // pBufferMemoryBarriers
      0,         // imageMemoryBarrierCount
      nullptr    // pImageMemoryBarriers
  );

  res = fn->vkEndCommandBuffer(stagingCommandBuffer);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Failed to end command buffer: %x", res);
    return res;
  }
  return VkResult::VK_SUCCESS;
}

uint32_t ExternalMemory::RecordStagingCommandBuffer(
    const ExternalMemoryCommandBuffer& cb) {
  VkCommandBufferBeginInfo beginInfo{
      VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      nullptr,  // pNext
      VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      nullptr,  // pInheritanceInfo
  };
  uint32_t res = fn->vkBeginCommandBuffer(cb.stagingCommandBuffer, &beginInfo);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Failed to begin staging command buffer: %x", res);
    return res;
  }

  std::vector<VkBufferMemoryBarrier> acquireBufferBarriers;
  acquireBufferBarriers.reserve(cb.buffers.size());
  std::vector<VkBufferMemoryBarrier> releaseBufferBarriers;
  releaseBufferBarriers.reserve(cb.buffers.size());

  std::vector<VkImageMemoryBarrier> acquireImageBarriers;
  acquireImageBarriers.reserve(cb.images.size());
  std::vector<VkImageMemoryBarrier> releaseImageBarriers;
  releaseImageBarriers.reserve(cb.images.size());

  for (const auto& buffer : cb.buffers) {
    VkBufferMemoryBarrier barrier = buffer.barrier;
    barrier.msrcAccessMask = 0;
    barrier.mdstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
    acquireBufferBarriers.push_back(barrier);
    std::swap(barrier.msrcAccessMask, barrier.mdstAccessMask);
    std::swap(barrier.msrcQueueFamilyIndex, barrier.mdstQueueFamilyIndex);
    releaseBufferBarriers.push_back(barrier);
  }

  for (const auto& image : cb.images) {
    VkImageMemoryBarrier barrier = image.barrier;
    barrier.msrcAccessMask = 0;
    barrier.mdstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
    barrier.mnewLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    acquireImageBarriers.push_back(barrier);

    std::swap(barrier.msrcAccessMask, barrier.mdstAccessMask);
    std::swap(barrier.msrcQueueFamilyIndex, barrier.mdstQueueFamilyIndex);
    std::swap(barrier.moldLayout, barrier.mnewLayout);
    releaseImageBarriers.push_back(barrier);
  }

  // acquire from external queue family
  fn->vkCmdPipelineBarrier(
      cb.stagingCommandBuffer,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
      0,        // dependencyFlags
      0,        // memoryBarrierCount
      nullptr,  // pMemoryBarriers
      (uint32_t)acquireBufferBarriers.size(), acquireBufferBarriers.data(),
      (uint32_t)acquireImageBarriers.size(), acquireImageBarriers.data());

  // copy external buffer barrier regions to staging buffer
  for (const auto& buffer : cb.buffers) {
    fn->vkCmdCopyBuffer(cb.stagingCommandBuffer, buffer.buffer, stagingBuffer,
                        1,  // regionCount
                        &buffer.copy);
  }

  // copy external image barrier regions to staging buffer
  for (const auto& image : cb.images) {
    fn->vkCmdCopyImageToBuffer(
        cb.stagingCommandBuffer, image.image,
        VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer,
        (uint32_t)image.copies.size(), image.copies.data());
  }

  // release external barrier regions back to external queue family
  // (so that the original barriers run correctly when they execute later)
  fn->vkCmdPipelineBarrier(
      cb.stagingCommandBuffer,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      0,        // dependencyFlags
      0,        // memoryBarrierCount
      nullptr,  // pMemoryBarriers
      (uint32_t)releaseBufferBarriers.size(), releaseBufferBarriers.data(),
      (uint32_t)releaseImageBarriers.size(), releaseImageBarriers.data());

  res = fn->vkEndCommandBuffer(cb.stagingCommandBuffer);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Failed to end staging command buffer: %x", res);
    return res;
  }

  return VkResult::VK_SUCCESS;
}

uint32_t ExternalMemory::Submit() {
  std::vector<VkCommandBuffer> commandBuffers;
  std::vector<VkSubmitInfo> submitInfos;
  for (const auto& submit : submits) {
    size_t first = commandBuffers.size();
    for (const auto& cmdBuf : submit.commandBuffers) {
      if (!cmdBuf.empty()) {
        commandBuffers.push_back(cmdBuf.stagingCommandBuffer);
      }
      commandBuffers.push_back(cmdBuf.commandBuffer);
    }
    size_t count = commandBuffers.size() - first;
    submitInfos.push_back(*submit.submitInfo);
    submitInfos.back().mpCommandBuffers = &commandBuffers[first];
    submitInfos.back().mcommandBufferCount = (uint32_t)count;
  }
  submitInfos.push_back({
      VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO,
      nullptr,  // pNext
      0,        // waitSemaphoreCount
      nullptr,  // pWaitSemaphores
      nullptr,  // pWaitDstStageMask
      1,        // commandBufferCount
      &stagingCommandBuffer,
      0,        // signalSemaphoreCount
      nullptr,  // pSignalSemaphores
  });
  uint32_t res =
      fn->vkQueueSubmit(queue, submitInfos.size(), submitInfos.data(), fence);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Queue submission failed: %x", res);
    return res;
  }
  return VkResult::VK_SUCCESS;
}

void ExternalMemory::SendData() {
  uint32_t res = fn->vkWaitForFences(device, 1, &fence, 0, UINT64_MAX);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Error waiting for fence: %x", res);
    return;
  }

  static const VkDeviceSize VK_WHOLE_SIZE = ~0ULL;

  uint8_t* data = nullptr;
  res = fn->vkMapMemory(device, stagingMemory, 0, VK_WHOLE_SIZE, 0,
                        (void**)&data);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Error mapping memory: %x", res);
    return;
  }

  VkMappedMemoryRange range{
      VkStructureType::VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
      nullptr,  // pNext
      stagingMemory,
      0,  // offset
      VK_WHOLE_SIZE,
  };
  res = fn->vkInvalidateMappedMemoryRanges(device, 1, &range);
  if (res != VkResult::VK_SUCCESS) {
    LOG_ERROR("Error invalidating memory: %x", res);
    return;
  }

  auto resIndex = spy->sendResource(VulkanSpy::kApiIndex, data, stagingSize);
  auto extra = new vulkan_pb::ExternalMemoryData();
  extra->set_res_index(resIndex);
  extra->set_res_size(stagingSize);
  for (uint32_t i = 0; i < submits.size(); i++) {
    const ExternalMemorySubmitInfo& submit = submits[i];
    for (uint32_t j = 0; j < submit.commandBuffers.size(); j++) {
      const ExternalMemoryCommandBuffer& cmdBuf = submit.commandBuffers[j];
      for (auto buffer : cmdBuf.buffers) {
        auto bufMsg = extra->add_buffers();
        bufMsg->set_buffer(buffer.buffer);
        bufMsg->set_buffer_offset(buffer.copy.msrcOffset);
        bufMsg->set_data_offset(buffer.copy.mdstOffset);
        bufMsg->set_size(buffer.copy.msize);
        bufMsg->set_submit_index(i);
        bufMsg->set_command_buffer_index(j);
      }
      for (const auto& image : cmdBuf.images) {
        auto imgMsg = extra->add_images();
        imgMsg->set_image(image.image);
        const VkImageSubresourceRange& barrierRng =
            image.barrier.msubresourceRange;
        imgMsg->set_aspect_mask(barrierRng.maspectMask);
        imgMsg->set_base_mip_level(barrierRng.mbaseMipLevel);
        imgMsg->set_level_count(barrierRng.mlevelCount);
        imgMsg->set_base_array_layer(barrierRng.mbaseArrayLayer);
        imgMsg->set_layer_count(barrierRng.mlayerCount);
        imgMsg->set_old_layout(image.barrier.moldLayout);
        imgMsg->set_new_layout(image.barrier.mnewLayout);
        imgMsg->set_submit_index(i);
        imgMsg->set_command_buffer_index(j);

        for (const auto& copy : image.copies) {
          auto copyMsg = imgMsg->add_ranges();
          copyMsg->set_data_offset(copy.mbufferOffset);
          const VkImageSubresourceLayers& copyRng = copy.mimageSubresource;
          copyMsg->set_aspect_mask(copyRng.maspectMask);
          copyMsg->set_mip_level(copyRng.mmipLevel);
          copyMsg->set_base_array_layer(copyRng.mbaseArrayLayer);
          copyMsg->set_layer_count(copyRng.mlayerCount);
        }
      }
    }
  }
  observer->encodeAndDelete(extra);

  fn->vkUnmapMemory(device, stagingMemory);
}

void ExternalMemory::Cleanup() {
  if (stagingCommandPool != 0) {
    fn->vkDestroyCommandPool(device, stagingCommandPool, nullptr);
    stagingCommandPool = 0;
  }

  if (stagingFence != 0) {
    fn->vkDestroyFence(device, stagingFence, nullptr);
    stagingFence = 0;
  }

  if (stagingBuffer != 0) {
    fn->vkDestroyBuffer(device, stagingBuffer, nullptr);
    stagingBuffer = 0;
  }

  if (stagingMemory != 0) {
    fn->vkFreeMemory(device, stagingMemory, nullptr);
    stagingMemory = 0;
  }
}

// TODO: This is duplicate code from vulkan_mid_execution.cpp.
std::vector<VkBufferImageCopy> VulkanSpy::BufferImageCopies(
    gapil::Ref<ImageObject> img, const VkImageSubresourceRange& img_rng,
    VkDeviceSize& offset) {
  const ImageInfo& image_info = img->mInfo;

  if (img->mIsSwapchainImage) {
    // Don't bind and fill swapchain images memory here
    return {};
  }
  if (image_info.mSamples != VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT) {
    // TODO: Handle multisampled images.
    return {};
  }
  if (image_info.mFormat == VkFormat::VK_FORMAT_UNDEFINED) {
    // TODO: support external formats.
    return {};
  }

  auto get_element_size = [this](uint32_t format, uint32_t aspect_bit,
                                 bool in_buffer) -> uint32_t {
    if (VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT == aspect_bit) {
      return subGetDepthElementSize(nullptr, nullptr, format, in_buffer);
    }
    return subGetElementAndTexelBlockSizeForAspect(nullptr, nullptr, format,
                                                   aspect_bit)
        .mElementSize;
  };

  auto next_multiple_of_8 = [](size_t value) -> size_t {
    return (value + 7) & (~7);
  };

  struct pitch {
    size_t row_pitch;
    size_t depth_pitch;
    size_t linear_layout_row_pitch;
    size_t linear_layout_depth_pitch;
    uint32_t texel_width;
    uint32_t texel_height;
    uint32_t element_size;
  };

  struct byte_size_and_extent {
    size_t level_size;
    size_t aligned_level_size;
    size_t level_size_in_buf;
    size_t aligned_level_size_in_buf;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
  };

  auto level_size = [this, &get_element_size, &next_multiple_of_8](
                        const VkExtent3D& extent, uint32_t format,
                        uint32_t mip_level, uint32_t aspect_bit,
                        bool account_for_plane) -> byte_size_and_extent {
    auto elementAndTexelBlockSize =
        subGetElementAndTexelBlockSize(nullptr, nullptr, format);
    auto divisor =
        subGetAspectSizeDivisor(nullptr, nullptr, format, aspect_bit);
    if (!account_for_plane) {
      divisor.mWidth = 1;
      divisor.mHeight = 1;
    }
    const uint32_t texel_width =
        elementAndTexelBlockSize.mTexelBlockSize.mWidth;
    const uint32_t texel_height =
        elementAndTexelBlockSize.mTexelBlockSize.mHeight;
    const uint32_t width =
        subGetMipSize(nullptr, nullptr, extent.mwidth, mip_level) /
        divisor.mWidth;
    const uint32_t height =
        subGetMipSize(nullptr, nullptr, extent.mheight, mip_level) /
        divisor.mHeight;
    const uint32_t depth =
        subGetMipSize(nullptr, nullptr, extent.mdepth, mip_level);
    const uint32_t width_in_blocks =
        subRoundUpTo(nullptr, nullptr, width, texel_width);
    const uint32_t height_in_blocks =
        subRoundUpTo(nullptr, nullptr, height, texel_height);
    const uint32_t element_size = get_element_size(format, aspect_bit, false);
    const uint32_t element_size_in_buf =
        get_element_size(format, aspect_bit, true);
    const size_t size =
        width_in_blocks * height_in_blocks * depth * element_size;
    const size_t size_in_buf =
        width_in_blocks * height_in_blocks * depth * element_size_in_buf;

    return byte_size_and_extent{size,        next_multiple_of_8(size),
                                size_in_buf, next_multiple_of_8(size_in_buf),
                                width,       height,
                                depth};
  };

  std::unordered_map<ImageLevel*, byte_size_and_extent> level_sizes;
  walkImageSubRng(
      img, img_rng,
      [&level_size, &img, &level_sizes](uint32_t aspect, uint32_t layer,
                                        uint32_t level) {
        auto img_level = img->mAspects[aspect]->mLayers[layer]->mLevels[level];
        level_sizes[img_level.get()] = level_size(
            img->mInfo.mExtent, img->mInfo.mFormat, level, aspect, true);
      });

  // TODO: Handle multi-planar images
  bool denseBound =
      subGetImagePlaneMemoryInfo(nullptr, nullptr, img, 0) != nullptr &&
      subGetImagePlaneMemoryInfo(nullptr, nullptr, img, 0)->mBoundMemory !=
          nullptr;
  bool sparseBound = (img->mOpaqueSparseMemoryBindings.count() > 0) ||
                     (img->mSparseImageMemoryBindings.count() > 0);
  bool sparseBinding =
      (image_info.mFlags &
       VkImageCreateFlagBits::VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0;
  bool sparseResidency =
      sparseBinding &&
      (image_info.mFlags &
       VkImageCreateFlagBits::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) != 0;
  if (!denseBound && !sparseBound) {
    return {};
  }
  // First check for validity before we go any further.
  if (sparseBound) {
    if (sparseResidency) {
      bool is_valid = true;
      // If this is a sparsely resident image, then at least ALL metadata
      // must be bound.
      for (const auto& req : img->mSparseMemoryRequirements) {
        const auto& prop = req.second.mformatProperties;
        if (prop.maspectMask ==
            VkImageAspectFlagBits::VK_IMAGE_ASPECT_METADATA_BIT) {
          if (!IsFullyBound(req.second.mimageMipTailOffset,
                            req.second.mimageMipTailSize,
                            img->mOpaqueSparseMemoryBindings)) {
            is_valid = false;
            break;
          }
        }
      }
      if (!is_valid) {
        return {};
      }
    } else {
      // If we are not sparsely-resident, then all memory must
      // be bound before we are used.
      // TODO: Handle multi-planar images
      auto planeMemInfo = subGetImagePlaneMemoryInfo(nullptr, nullptr, img, 0);
      if (!IsFullyBound(0, planeMemInfo->mMemoryRequirements.msize,
                        img->mOpaqueSparseMemoryBindings)) {
        return {};
      }
    }
  }

  struct opaque_piece {
    uint32_t aspect_bit;
    uint32_t layer;
    uint32_t level;
  };
  std::vector<opaque_piece> opaque_pieces;
  auto append_image_level_to_opaque_pieces = [&img, &opaque_pieces](
                                                 uint32_t aspect_bit,
                                                 uint32_t layer,
                                                 uint32_t level) {
    auto& img_level = img->mAspects[aspect_bit]->mLayers[layer]->mLevels[level];
    if (img_level->mLayout == VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED) {
      return;
    }
    opaque_pieces.push_back(opaque_piece{aspect_bit, layer, level});
  };
  if (denseBound || !sparseResidency) {
    walkImageSubRng(img, img_rng, append_image_level_to_opaque_pieces);
  } else {
    for (const auto& req : img->mSparseMemoryRequirements) {
      const auto& prop = req.second.mformatProperties;
      if (prop.maspectMask == img->mImageAspect) {
        if (prop.mflags & VkSparseImageFormatFlagBits::
                              VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) {
          if (!IsFullyBound(req.second.mimageMipTailOffset,
                            req.second.mimageMipTailSize,
                            img->mOpaqueSparseMemoryBindings)) {
            continue;
          }
          VkImageSubresourceRange bound_rng = VkImageSubresourceRange{
              img->mImageAspect,
              req.second.mimageMipTailFirstLod,
              image_info.mMipLevels - req.second.mimageMipTailFirstLod,
              0,  // baseArrayLayer
              image_info.mArrayLayers,
          };
          walkImageSubRng(img, bound_rng, append_image_level_to_opaque_pieces);
        } else {
          for (uint32_t i = 0; i < uint32_t(image_info.mArrayLayers); i++) {
            VkDeviceSize offset = req.second.mimageMipTailOffset +
                                  i * req.second.mimageMipTailStride;
            if (!IsFullyBound(offset, req.second.mimageMipTailSize,
                              img->mOpaqueSparseMemoryBindings)) {
              continue;
            }
            VkImageSubresourceRange bound_rng = VkImageSubresourceRange{
                img->mImageAspect,
                req.second.mimageMipTailFirstLod,
                image_info.mMipLevels - req.second.mimageMipTailFirstLod,
                i,
                1,
            };
            walkImageSubRng(img, bound_rng,
                            append_image_level_to_opaque_pieces);
          }
        }
      }
    }
  }

  // Don't capture images with undefined layout for all its subresources.
  // The resulting data itself will be undefined.
  if (opaque_pieces.size() == 0) {
    return {};
  }

  offset = next_multiple_of_8(offset);
  std::vector<VkBufferImageCopy> copies_in_order;
  for (auto& piece : opaque_pieces) {
    auto img_level = img->mAspects[piece.aspect_bit]
                         ->mLayers[piece.layer]
                         ->mLevels[piece.level];
    auto copy = VkBufferImageCopy{
        offset,
        0,  // bufferRowLength
        0,  // bufferImageHeight,
        {
            VkImageAspectFlags(piece.aspect_bit), piece.level, piece.layer,
            1,  // layerCount
        },
        {0, 0, 0},
        {level_sizes[img_level.get()].width,
         level_sizes[img_level.get()].height,
         level_sizes[img_level.get()].depth}};
    copies_in_order.push_back(copy);
    offset += level_sizes[img_level.get()].aligned_level_size_in_buf;
  }

  if (sparseResidency) {
    for (auto& aspect_i :
         subUnpackImageAspectFlags(nullptr, nullptr, img, img->mImageAspect)) {
      uint32_t aspect_bit = aspect_i.second;
      if (img->mSparseImageMemoryBindings.find(aspect_bit) !=
          img->mSparseImageMemoryBindings.end()) {
        for (const auto& layer_i :
             img->mSparseImageMemoryBindings[aspect_bit]->mLayers) {
          for (const auto& level_i : layer_i.second->mLevels) {
            auto img_level = img->mAspects[aspect_bit]
                                 ->mLayers[layer_i.first]
                                 ->mLevels[level_i.first];
            for (const auto& block_i : level_i.second->mBlocks) {
              auto copy = VkBufferImageCopy{
                  offset,
                  0,  // bufferRowLength,
                  0,  // bufferImageHeight,
                  VkImageSubresourceLayers{
                      aspect_bit, level_i.first, layer_i.first,
                      1  // layerCount
                  },
                  block_i.second->mOffset,
                  block_i.second->mExtent};

              copies_in_order.push_back(copy);
              byte_size_and_extent e =
                  level_size(block_i.second->mExtent, image_info.mFormat, 0,
                             aspect_bit, false);
              offset += e.aligned_level_size_in_buf;
            }
          }
        }
      }
    }
  }

  return copies_in_order;
}

}  // namespace gapii
