// SPDX-License-Identifier: Apache-2.0
// With shard_dirs, the connector remembers the shard directories it created.
// When a directory is removed later, e.g. by purging the cache directory while
// the connector runs, a store under it creates the directory again.
#include "csrc/storage_backends/hf3fs/connector.h"
#include "hf3fs_mock.h"
#include "test_util.h"
#include <filesystem>
#include <string>
#include <vector>

namespace {
using lmcache::connector::Hf3fsConnector;

bool store(Hf3fsConnector& connector, const std::string& key,
           std::vector<char>& payload) {
  return wait_for_completion(connector, connector.submit_batch_set(
                                            {key}, {payload.data()},
                                            {payload.size()}, payload.size()))
      .ok;
}
}  // namespace

int main(int argc, char** argv) {
  require(argc == 2, "pass an empty scratch directory");
  hf3fs_mock::mount_point = argv[1];
  constexpr size_t kIovSize = 4096;
  Hf3fsConnector connector(argv[1], argv[1], 1, 256, 0, -1, kIovSize, 200,
                           false, true);
  std::vector<char> payload(64, 'x');
  const std::string key = "model@00000000@0@abcd1234";
  const std::filesystem::path base(argv[1]);
  const auto file = base / "ab" / "cd" / "model@0x00000000@0@abcd1234.data";

  require(store(connector, key, payload), "first store failed");
  require(std::filesystem::exists(file), "first store wrote no file");

  std::filesystem::remove_all(base / "ab");
  require(store(connector, key, payload),
          "store after the shard directory was removed failed");
  require(std::filesystem::exists(file), "second store wrote no file");

  std::vector<char> loaded(payload.size());
  auto get = wait_for_completion(
      connector, connector.submit_batch_get({key}, {loaded.data()},
                                            {loaded.size()}, loaded.size()));
  require(get.result_bytes == std::vector<uint8_t>{1} && loaded == payload,
          "load after the second store failed");

  connector.close();
}
