/*
 * Copyright (C) 2017 Google Inc.
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

#ifndef GAPII_VULKAN_EXTRAS_H_
#define GAPII_VULKAN_EXTRAS_H_

static inline void set_dispatch_from_parent(void* child, void* parent) {
  *((const void**)child) = *((const void**)parent);
}

namespace gapii {

// An invalid value of memory type index
constexpr uint32_t kInvalidMemoryTypeIndex = 0xFFFFFFFF;
// The queue family value when it is ignored
constexpr uint32_t kQueueFamilyIgnore = 0xFFFFFFFF;

uint32_t GetMemoryTypeIndexForStagingResources(
    const VkPhysicalDeviceMemoryProperties& phy_dev_prop,
    uint32_t requirement_type_bits);

// Returns true if the resource range from |offset| with |size| is fully
// covered in the |bindings|.
bool IsFullyBound(VkDeviceSize offset, VkDeviceSize size,
                  const U64ToVkSparseMemoryBind& bindings);

}  // namespace gapii

#endif
