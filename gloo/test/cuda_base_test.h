/**
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include "gloo/test/base_test.h"

#include "gloo/cuda_private.h"

namespace gloo {
namespace test {

void cudaSleep(cudaStream_t stream, size_t clocks);

int cudaNumDevices();

class CudaBaseTest : public BaseTest {};

class CudaFixture {
 public:
  CudaFixture(const std::shared_ptr<Context> context, int devices, int count)
      : context(context),
        count(count) {
    for (int i = 0; i < devices; i++) {
      CudaDeviceScope scope(i);
      srcs.push_back(CudaMemory<float>(count));
      ptrs.push_back(
        CudaDevicePointer<float>::create(*srcs.back(), count));
    }
  }

  CudaFixture(CudaFixture&& other) noexcept
    : context(other.context),
      count(other.count) {
    srcs = std::move(other.srcs);
    ptrs = std::move(other.ptrs);
  }

  void assignValues() {
    const auto stride = context->size * srcs.size();
    for (int i = 0; i < srcs.size(); i++) {
      const auto& stream = ptrs[i].getStream();
      srcs[i].set((context->rank * srcs.size()) + i, stride, stream);
      CUDA_CHECK(cudaStreamSynchronize(stream));
    }
  }

  void assignValuesAsync() {
    const auto stride = context->size * srcs.size();
    for (int i = 0; i < srcs.size(); i++) {
      const auto& stream = ptrs[i].getStream();
      // Insert sleep on stream to force to artificially delay the
      // kernel that actually populates the memory to surface
      // synchronization errors.
      cudaSleep(stream, 100000);
      srcs[i].set((context->rank * srcs.size()) + i, stride, stream);
    }
  }

  std::vector<float*> getFloatPointers() const {
    std::vector<float*> out;
    for (const auto& src : srcs) {
      out.push_back(*src);
    }
    return out;
  }

  std::vector<cudaStream_t> getCudaStreams() const {
    std::vector<cudaStream_t> out;
    for (const auto& ptr : ptrs) {
      out.push_back(ptr.getStream());
    }
    return out;
  }

  std::vector<std::unique_ptr<float[]> > getHostBuffers() {
    std::vector<std::unique_ptr<float[]> > out;
    for (auto& src : srcs) {
      out.push_back(src.copyToHost());
    }
    return out;
  }

  void synchronizeCudaStreams() {
    for (const auto& ptr : ptrs) {
      CudaDeviceScope scope(ptr.getDeviceID());
      CUDA_CHECK(cudaStreamSynchronize(ptr.getStream()));
    }
  }

  std::shared_ptr<Context> context;
  const int count;
  std::vector<CudaDevicePointer<float> > ptrs;
  std::vector<CudaMemory<float> > srcs;
};

} // namespace test
} // namespace gloo
