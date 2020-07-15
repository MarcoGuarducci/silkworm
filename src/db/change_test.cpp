/*
   Copyright 2020 The Silkworm Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "change.hpp"

#include <boost/algorithm/hex.hpp>
#include <catch2/catch.hpp>
#include <evmc/evmc.hpp>

#include "common/util.hpp"
#include "util.hpp"

namespace silkworm::db {
TEST_CASE("Find storage change") {
  using namespace std::string_literals;
  using namespace evmc::literals;

  auto contract_a{0x6f0e0cdac6c716a00bd8db4d0eee4f2bfccf8e6a_address};
  auto contract_b{0xc5acb79c258108f288288bc26f7820d06f45f08c_address};
  auto contract_c{0xd88eba4c93123372a9f67215f80477bc3644e6ab_address};
  auto key1{0xa4e69cebbf4f8f3a1c6e493a6983d8a5879d22057a7c73b00e105d7c7e21efbc_bytes32};
  auto key2{0x0000000000000000000000000000000000000000000000000000000000000001_bytes32};
  auto val1{0x33bf0d0c348a2ef1b3a12b6a535e1e25a56d3624e45603e469626d80fd78c762_bytes32};
  auto val2{0x0000000000000000000000000000002506e4b566c5be7dd44e8e2fc7b1f6a99c_bytes32};

  std::string encoded{boost::algorithm::unhex(
      "000000041cbdd8336800dc3fe27daf5fb5188f0502ac1fc7000000016f0e0cdac6c716a00bd8db4d0eee4f2bfccf"
      "8e6a000000036f0e0cdac6c716a00bd8db4d0eee4f2bfccf8e6a00000004c5acb79c258108f288288bc26f7820d0"
      "6f45f08c000000060000000200000000fffffffffffffffa00000001fffffffffffffffd4fdf6c1878d2469b4968"
      "4effe69db8689d88a4f1695055538501ff197bc9e30e000000000000000000000000000000000000000000000000"
      "000000000000df77a4e69cebbf4f8f3a1c6e493a6983d8a5879d22057a7c73b00e105d7c7e21efbcaa2703c3ae5d"
      "0024b2c3ab77e5200bb2a8eb39a140fad01e89a495d73760297c0000000000000000000000000000000000000000"
      "0000000000000000000000010bece5a88f7b038f806dbef77c0b462506e4b566c5be7dd44e8e2fc7b1f6a99c0000"
      "0006000000000000000020406080a0c0207a386cdf40716455365db189633e822d3a7598558901f2255e64cb5e42"
      "4714ec89478783348038046b42cc126a3c4e351977b5f4cf5e3c4f4d8385adbf804633bf0d0c348a2ef1b3a12b6a"
      "535e1e25a56d3624e45603e469626d80fd78c7620000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000002506e4b566c5be7dd44e8e2fc7b1f6a99c0000000000000000"
      "000000000000000000000000000000000000000000000459"s)};

  CHECK(StorageChanges::find(encoded, storage_key(contract_a, 2, key1)) == view_of_hash(val1));
  CHECK(StorageChanges::find(encoded, storage_key(contract_b, 1, key2)) == view_of_hash(val2));

  CHECK(!StorageChanges::find(encoded, storage_key(contract_a, 1, key1)));
  CHECK(!StorageChanges::find(encoded, storage_key(contract_c, 2, key1)));
  CHECK(!StorageChanges::find(encoded, storage_key(contract_b, 1, {})));
}
}  // namespace silkworm::db