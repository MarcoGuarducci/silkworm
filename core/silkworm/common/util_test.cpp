/*
   Copyright 2020-2021 The Silkworm Authors

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

#include "util.hpp"

#include <catch2/catch.hpp>

namespace silkworm {

TEST_CASE("Padding") {
    Bytes buffer;

    CHECK(to_hex(right_pad(*from_hex("a5"), 3, buffer)) == "a50000");
    CHECK(to_hex(right_pad(*from_hex("5a0b54d5dc17e0aadc383d2db4"), 3, buffer)) == "5a0b54d5dc17e0aadc383d2db4");

    CHECK(to_hex(left_pad(*from_hex("a5"), 3, buffer)) == "0000a5");
    CHECK(to_hex(left_pad(*from_hex("5a0b54d5dc17e0aadc383d2db4"), 3, buffer)) == "5a0b54d5dc17e0aadc383d2db4");

    ByteView repeatedly_padded{right_pad(*from_hex("b8c4"), 3, buffer)};
    CHECK(to_hex(repeatedly_padded) == "b8c400");
    repeatedly_padded.remove_prefix(1);
    CHECK(to_hex(repeatedly_padded) == "c400");
    repeatedly_padded = right_pad(repeatedly_padded, 4, buffer);
    CHECK(to_hex(repeatedly_padded) == "c4000000");

    repeatedly_padded = left_pad(*from_hex("b8c4"), 3, buffer);
    CHECK(to_hex(repeatedly_padded) == "00b8c4");
    repeatedly_padded.remove_suffix(1);
    CHECK(to_hex(repeatedly_padded) == "00b8");
    repeatedly_padded = left_pad(repeatedly_padded, 4, buffer);
    CHECK(to_hex(repeatedly_padded) == "000000b8");
}

TEST_CASE("Zeroless view") {
    CHECK(to_hex(zeroless_view(0x0000000000000000000000000000000000000000000000000000000000000000_bytes32)) == "");
    CHECK(to_hex(zeroless_view(0x000000000000000000000000000000000000000000000000000000000004bc00_bytes32)) ==
          "04bc00");
}

TEST_CASE("to_bytes32") {
    CHECK(to_hex(to_bytes32(*from_hex("05"))) == "0000000000000000000000000000000000000000000000000000000000000005");

    CHECK(to_hex(to_bytes32(*from_hex("0x05"))) == "0000000000000000000000000000000000000000000000000000000000000005");

    CHECK(to_hex(to_bytes32(*from_hex("9d36d8120b564f654564a91259a6ca6d37d6473827d45210190ad10f8ca451f2"))) ==
          "9d36d8120b564f654564a91259a6ca6d37d6473827d45210190ad10f8ca451f2");

    CHECK(to_hex(to_bytes32(*from_hex("0X9d36d8120b564f654564a91259a6ca6d37d6473827d45210190ad10f8ca451f2"))) ==
          "9d36d8120b564f654564a91259a6ca6d37d6473827d45210190ad10f8ca451f2");

    CHECK(to_hex(to_bytes32(*from_hex("7576351873263824fff23784264823469344629364396429864239864938264a"
                                      "8236423964bbb009874e"))) ==
          "7576351873263824fff23784264823469344629364396429864239864938264a");
}

TEST_CASE("iequals") {
    std::string a{"Hello World"};
    std::string b{"Hello wOrld"};
    std::string c{"Hello World "};
    CHECK(iequals(a, b));
    CHECK(!iequals(a, c));
}

TEST_CASE("parse_size") {
    std::optional<uint64_t> size{parse_size("")};
    CHECK((size && *size == 0));

    static_assert(kKibi == 1024ull);
    static_assert(kMebi == 1024ull * 1024ull);
    static_assert(kGibi == 1024ull * 1024ull * 1024ull);
    static_assert(kTebi == 1024ull * 1024ull * 1024ull * 1024ull);

    size = parse_size("128");
    CHECK((size && *size == 128));
    size = parse_size("256B");
    CHECK((size && *size == 256));
    size = parse_size("640KB");
    CHECK((size && *size == 640 * kKibi));
    size = parse_size("75MB");
    CHECK((size && *size == 75 * kMebi));
    size = parse_size("400GB");
    CHECK((size && *size == 400 * kGibi));
    size = parse_size("2TB");
    CHECK((size && *size == 2 * kTebi));
    size = parse_size(".5TB");
    CHECK((size && *size == (kTebi * 0.5)));
    size = parse_size("0.5TB");
    CHECK((size && *size == (kTebi * 0.5)));
    size = parse_size("0.5   TB");
    CHECK((size && *size == (kTebi * 0.5)));
    CHECK(!parse_size("ABBA"));
}

}  // namespace silkworm
