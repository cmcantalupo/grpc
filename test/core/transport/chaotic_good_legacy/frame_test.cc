// Copyright 2022 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/core/ext/transport/chaotic_good_legacy/frame.h"

#include <cstdint>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "src/core/lib/resource_quota/memory_quota.h"
#include "src/core/lib/resource_quota/resource_quota.h"

namespace grpc_core {
namespace chaotic_good_legacy {
namespace {

FrameLimits TestFrameLimits() { return FrameLimits{1024 * 1024 * 1024, 63}; }

template <typename T>
void AssertRoundTrips(const T& input, FrameType expected_frame_type) {
  HPackCompressor hpack_compressor;
  bool saw_encoding_errors = false;
  auto serialized = input.Serialize(&hpack_compressor, saw_encoding_errors);
  CHECK_GE(serialized.control.Length(),
           24);  // Initial output buffer size is 64 byte.
  uint8_t header_bytes[24];
  serialized.control.MoveFirstNBytesIntoBuffer(24, header_bytes);
  auto header = FrameHeader::Parse(header_bytes);
  if (!header.ok()) {
    Crash("Failed to parse header");
  }
  CHECK_EQ(header->type, expected_frame_type);
  T output;
  HPackParser hpack_parser;
  absl::BitGen bitgen;
  MemoryAllocator allocator = MakeResourceQuota("test-quota")
                                  ->memory_quota()
                                  ->CreateMemoryAllocator("test-allocator");
  RefCountedPtr<Arena> arena = SimpleArenaAllocator()->MakeArena();
  auto deser =
      output.Deserialize(&hpack_parser, header.value(), absl::BitGenRef(bitgen),
                         arena.get(), std::move(serialized), TestFrameLimits());
  CHECK_OK(deser);
  if (!saw_encoding_errors) CHECK_EQ(output, input);
}

TEST(FrameTest, SettingsFrameRoundTrips) {
  AssertRoundTrips(SettingsFrame{}, FrameType::kSettings);
}

}  // namespace
}  // namespace chaotic_good_legacy
}  // namespace grpc_core

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int r = RUN_ALL_TESTS();
  return r;
}
