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

#include "collector.hpp"

#include <iomanip>
#include <queue>

#include <boost/filesystem.hpp>

#include <silkworm/common/log.hpp>

namespace silkworm::etl {

namespace fs = boost::filesystem;

Collector::~Collector() {
    file_providers_.clear();  // Will ensure all files (if any) have been orderly closed and deleted before we remove
                              // the working dir
    fs::path path(work_path_);
    if (fs::exists(path)) {
        fs::remove_all(path);
    }
}

void Collector::flush_buffer() {
    if (buffer_.size()) {
        buffer_.sort();

        /* Build a unique file name to pass FileProvider */
        fs::path new_file_path{fs::path(work_path_) / fs::path(std::to_string(unique_id_) + "-" +
                                                               std::to_string(file_providers_.size()) + ".bin")};

        file_providers_.emplace_back(new FileProvider(new_file_path.string(), file_providers_.size()));
        file_providers_.back()->flush(buffer_);
        buffer_.clear();
    }
}

size_t Collector::size() const { return size_; }

void Collector::collect(Entry& entry) {
    buffer_.put(entry);
    size_++;
    if (buffer_.overflows()) {
        flush_buffer();
    }
}

void Collector::load(silkworm::lmdb::Table* table, LoadFunc load_func, unsigned int db_flags,
                     uint32_t log_every_percent) {
    const auto overall_size{size()};  // Amount of work

    if (!overall_size) {
        SILKWORM_LOG(LogInfo) << "ETL Load called without data to process" << std::endl;
        return;
    }

    const uint32_t progress_step{log_every_percent ? std::min(log_every_percent, 100u) : 100u};
    const size_t progress_increment_count{overall_size / (100 / progress_step)};
    size_t dummy_counter{progress_increment_count};
    uint32_t actual_progress{0};

    if (!file_providers_.size()) {
        buffer_.sort();
        if (load_func) {
            for (const auto& etl_entry : buffer_.get_entries()) {
                load_func(etl_entry, table, db_flags);

                if (!--dummy_counter) {
                    actual_progress += progress_step;
                    dummy_counter = progress_increment_count;
                    SILKWORM_LOG(LogInfo) << "ETL Load Progress "
                                          << " << " << actual_progress << "%" << std::endl;
                }
            }
        } else {
            for (const auto& etl_entry : buffer_.get_entries()) {
                table->put(etl_entry.key, etl_entry.value, db_flags);
                if (!--dummy_counter) {
                    actual_progress += progress_step;
                    dummy_counter = progress_increment_count;
                    SILKWORM_LOG(LogInfo) << "ETL Load Progress "
                                          << " << " << actual_progress << "%" << std::endl;
                }
            }
        }
        buffer_.clear();
        return;
    }

    // Flush not overflown buffer data to file
    flush_buffer();

    // Define a priority queue based on smallest available key
    auto key_comparer = [](std::pair<Entry, int> left, std::pair<Entry, int> right) {
        return left.first.key.compare(right.first.key) > 0;
    };
    std::priority_queue<std::pair<Entry, int>, std::vector<std::pair<Entry, int>>, decltype(key_comparer)> queue(
        key_comparer);

    // Read one "record" from each data_provider and let the queue
    // sort them. On top of the queue the smallest key
    for (auto& file_provider : file_providers_) {
        auto item{file_provider->read_entry()};
        if (item.has_value()) {
            queue.push(*item);
        }
    }

    // Process the queue from smallest to largest key
    while (queue.size()) {
        auto& [etl_entry, provider_index]{queue.top()};           // Pick smallest key by reference
        auto& file_provider{file_providers_.at(provider_index)};  // and set current file provider

        // Process linked pairs
        if (load_func) {
            load_func(etl_entry, table, db_flags);
        } else {
            table->put(etl_entry.key, etl_entry.value, db_flags);
        }

        // Display progress
        if (!--dummy_counter) {
            actual_progress += progress_step;
            dummy_counter = progress_increment_count;
            SILKWORM_LOG(LogInfo) << "ETL Load Progress "
                                  << " << " << actual_progress << "%" << std::endl;
        }

        // From the provider which has served the current key
        // read next "record"
        auto next{file_provider->read_entry()};

        // At this point `current` has been processed.
        // We can remove it from the queue
        queue.pop();

        // Add next item to the queue only if it has
        // meaningful data
        if (next.has_value()) {
            queue.push(*next);
        } else {
            file_provider.reset();
        }
    }
    size_ = 0;  // We have consumed all items
}

std::string Collector::set_work_path(const char* provided_work_path) {
    // If something provided ensure exists as a directory
    if (provided_work_path) {
        fs::path path(provided_work_path);
        if (!fs::exists(path) || !fs::is_directory(path)) {
            throw etl_error("Non existent working directory");
        }
        return path.string();
    }

    // No path provided so we need to get a unique temporary directory
    // to prevent different instances of collector to clash each other
    // with same filenames
    fs::path p{fs::temp_directory_path() / fs::unique_path()};
    fs::create_directories(p);
    return p.string();
}

}  // namespace silkworm::etl