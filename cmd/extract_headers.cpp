/*
   Copyright 2021 The Silkworm Authors

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

#include <chrono>
#include <iostream>
#include <string>

#include <CLI/CLI.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/endian/conversion.hpp>

#include <silkworm/chain/config.hpp>
#include <silkworm/common/util.hpp>
#include <silkworm/db/access_layer.hpp>
#include <silkworm/db/tables.hpp>
#include <silkworm/db/util.hpp>
#include <silkworm/types/transaction.hpp>

using namespace silkworm;

// Definitions
class Hash : public evmc::bytes32 {
  public:
    Hash() {}

    Hash(ByteView bv) { std::memcpy(bytes, bv.data(), 32); }

    operator Bytes() { return {bytes, 32}; }
    operator ByteView() { return {bytes, 32}; }

    std::string to_hex() { return silkworm::to_hex(*this); }
};

using Header = BlockHeader;
using BlockNum = uint64_t;

class Db {
    std::shared_ptr<lmdb::Environment> env;
    std::unique_ptr<lmdb::Transaction> txn;

  public:
    Db(std::string db_path) {
        lmdb::DatabaseConfig db_config{db_path};
        // db_config.set_readonly(false);
        env = lmdb::get_env(db_config);
        txn = env->begin_ro_transaction();
    }

    std::optional<Hash> read_canonical_hash(BlockNum b) {  // throws db exceptions
        auto header_table = txn->open(db::table::kBlockHeaders);
        // accessing this table with only b we will get the hash of the canonical block at height b
        std::optional<ByteView> hash = header_table->get(db::header_hash_key(b));
        if (!hash) return std::nullopt;  // not found
        assert(hash->size() == kHashLength);
        return hash.value();  // copy
    }

    Bytes head_header_key() {  // todo: add to db::util.h?
        std::string table_name = db::table::kHeadHeader.name;
        Bytes key{table_name.begin(), table_name.end()};
        return key;
    }

    std::optional<Hash> read_head_header_hash() {
        auto head_header_table = txn->open(db::table::kHeadHeader);
        std::optional<ByteView> hash = head_header_table->get(head_header_key());
        if (!hash) return std::nullopt;  // not found
        assert(hash->size() == kHashLength);
        return hash.value();  // copy
    }

    std::optional<BlockHeader> read_header(BlockNum b, Hash h) { return db::read_header(*txn, b, h.bytes); }

    std::optional<ByteView> read_rlp_encoded_header(BlockNum b, Hash h) {
        auto header_table = txn->open(db::table::kBlockHeaders);
        std::optional<ByteView> rlp = header_table->get(db::block_key(b, h.bytes));
        return rlp;
    }

    Bytes header_numbers_key(Hash h) {  // todo: add to db::util.h?
        return {h.bytes, 32};
    }

    std::optional<BlockHeader> read_header(Hash h) {
        auto blockhashes_table = txn->open(db::table::kHeaderNumbers);
        auto encoded_block_num = blockhashes_table->get(header_numbers_key(h));
        if (!encoded_block_num) return {};
        BlockNum block_num = boost::endian::load_big_u64(encoded_block_num->data());
        return read_header(block_num, h);
    }
};

class HeaderListFile {
  public:
    HeaderListFile(std::string file_name) { output_file.open(file_name); }
    void add_header(const std::string& header) {
        if (empty)
            output_file << template_begin();  // write first part of the template
        else
            output_file << ",\n";  // terminate last line

        output_file << "    \"" << header << "\"";  // output the header

        empty = false;
    }
    void close() {
        if (!output_file.is_open()) return;
        if (!empty)
            output_file << "\n"             // terminate last line
                        << template_end();  // write final part of the template
        output_file.close();
    }
    ~HeaderListFile() { close(); }

  private:
    std::string template_begin() {
        std::size_t pos = file_template.find('@');
        return file_template.substr(0, pos);
    }
    std::string template_end() {
        std::size_t pos = file_template.find('@');
        return file_template.substr(pos + 1);
    }

    std::ofstream output_file;
    bool empty = true;

    std::string file_template =
        R"TEMPLATE(
    // hard coded headers
    #include <silkworm/db/header_download.hpp> // ?

    const char* hard_coded_headers[] = {   // "header1"; "header2"; ...
        @
    };
    )TEMPLATE";  // improvement: handle whitespaces here and at add_header()
};

std::string base64encode(const ByteView& bytes) {
    size_t encoded_len = boost::beast::detail::base64::encoded_size(bytes.length());
    std::string encoded_bytes(encoded_len, '\0');  // since c++11 string.data() is contiguous
    boost::beast::detail::base64::encode(encoded_bytes.data(), bytes.data(),
                                         bytes.length());  // and we can write safely in it
    size_t padding = int(encoded_bytes[encoded_len - 1] == '=') + int(encoded_bytes[encoded_len - 2] == '=');
    if (padding) encoded_bytes.erase(encoded_len - padding);
    return encoded_bytes;
}

// Main
int main(int argc, char* argv[]) {
    using std::string, std::cout, std::cerr, std::optional;
    using namespace std::chrono;

    // Command line parsing
    CLI::App app{
        "Extract Headers. Hard-code historical headers, from block zero to the current block with a certain step"};

    string name = "last";
    string db_path = db::default_path();
    uint64_t block_step = 100'000u;

    app.add_option("-n,--name,name", name, "Name suffix of the output file", true);
    // also accepted as a positional
    app.add_option("--chaindata", db_path, "Path to the chain database", true)->check(CLI::ExistingDirectory);
    app.add_option("-s,--step", block_step, "Block step", true)->check(CLI::Range(uint64_t{1}, UINT64_MAX));

    CLI11_PARSE(app, argc, argv);

    // Main loop
    try {
        Db db{db_path};

        string file_name = "hard_coded_headers_" + name + ".h";
        HeaderListFile output{file_name};
        BlockNum block_num = 0;
        for (; block_num < UINT64_MAX; block_num += block_step) {
            optional<Hash> hash = db.read_canonical_hash(block_num);
            if (!hash) break;
            optional<ByteView> encoded_header = db.read_rlp_encoded_header(block_num, *hash);
            if (!encoded_header) throw std::logic_error("block header not found in db (but its hash is present)");
            output.add_header(base64encode(*encoded_header));
        }
        output.close();

        // Final tasks
        cout << "Last block is " << block_num << "\n";

        auto hash = db.read_head_header_hash();
        if (!hash) throw std::logic_error("hash of head header not found in db");
        auto header = db.read_header(*hash);
        if (!header) throw std::logic_error("head header not found in db");

        auto unix_timestamp = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        cout << "Latest header timestamp: " << header->timestamp << ", current time: " << unix_timestamp << "\n";
        return 0;
    } catch (std::exception& e) {
        cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
