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

#include <iostream>
#include <CLI/CLI.hpp>
#include <absl/container/flat_hash_set.h>
#include <absl/time/time.h>

#include <boost/filesystem.hpp>
#include <iostream>
#include <silkworm/db/access_layer.hpp>
#include <silkworm/db/buffer.hpp>
#include <silkworm/execution/execution.hpp>
#include <silkworm/common/util.hpp>
#include <silkworm/types/transaction.hpp>

using namespace evmc::literals;
using namespace silkworm;


struct statistic {
	uint64_t nTxs;
	uint64_t gas;
	uint64_t wastedEthOutOfGas;
	uint64_t nEthOutOfGas;
	uint64_t wastedEthInvalid;
	uint64_t nEthInvalid;
	uint64_t gasEth;
	uint64_t nEth;
	uint64_t wastedContractOutOfGas;
	uint64_t nContractOutOfGas;
	uint64_t wastedContractInvalid;
	uint64_t nContractInvalid;
	uint64_t gasContract;
	uint64_t nContract;
	uint64_t wastedOutOfGas;
	uint64_t nOutOfGas;
	uint64_t nInvalid;
	uint64_t wastedInvalid;
	uint64_t wasted;

	uint64_t nErrors;
};
static struct statistic full, partial;

int main(int argc, char* argv[]) {
	CLI::App app{"Executes Ethereum blocks and collects transactions statistics"};
	std::string db_path{db::default_path()};
	app.add_option("--chaindata", db_path, "Path to a database populated by Turbo-Geth", true)
	->check(CLI::ExistingDirectory);

	uint64_t from{1};
	app.add_option("--from", from, "start from block number (inclusive)");

	uint64_t to{UINT64_MAX};
	app.add_option("--to", to, "check up to block number (exclusive)");

	CLI11_PARSE(app, argc, argv);

	absl::Time t1{absl::Now()};
	std::cout << t1 << " Checking change sets in " << db_path << "\n";

	lmdb::DatabaseConfig db_config{db_path};
	std::shared_ptr<lmdb::Environment> env{lmdb::get_env(db_config)};

	AnalysisCache analysis_cache;
	ExecutionStatePool state_pool;

	uint64_t block_num{from};
	//std::unique_ptr<lmdb::Transaction> txn{env->begin_ro_transaction()};

	setbuf(stdout,NULL);
	uint64_t block_printed = 0;
	printf("#Block,#Err,#Transaction,#Outs-Of-Gas,#Invalid,#Eth,#Eth-Out-Of-Gas,#Eth-Invalid,#Contract,#Contract-Out-Of-Gas,#Contract-Invalid,Gas,Wasted-Gas,Wasted-Out-Of-Gas,Wasted-Invalid,Gas-Eth,Wasted-Eth-Out-Of-Gas,Wasted-Eth-Invalid,Gas-Contract,Wasted-Contract-Out-Of-Gas,Wasted-Contract-Invalid\n");
	for (; block_num < to; ++block_num) {
		std::unique_ptr<lmdb::Transaction> txn{env->begin_ro_transaction()};

		std::optional<BlockWithHash> bh{db::read_block(*txn, block_num, /*read_senders=*/true)};
		if (!bh) break;

		db::Buffer buffer{txn.get(), block_num};

		auto [receipts, err]{execute_block(bh->block, buffer, kMainnetConfig, &analysis_cache, &state_pool)};
		if (err != ValidationResult::kOk) {
			std::cerr << "Failed to execute block " << block_num << " SIZE " << receipts.size() << "\n";
			partial.nErrors++;
			full.nErrors++;
		} else {
			assert(bh->block.transactions.size() == receipts.size());

			int i = 0;
			for (auto receipt : receipts) {
				//auto tx = bh->block.transactions[i++];
				auto tx = bh->block.transactions.at(i++);

				partial.nTxs++;
				partial.gas += receipt.cumulative_gas_used;
				full.nTxs++;
				full.gas += receipt.cumulative_gas_used;
				//if(tx.gas_limit == 21000)  {
				if(tx.data[0] == 0) {
					if(!receipt.success) {
						if(receipt.cumulative_gas_used == tx.gas_limit) {
							partial.wastedEthOutOfGas += receipt.cumulative_gas_used;
							partial.nEthOutOfGas++;
							full.wastedEthOutOfGas += receipt.cumulative_gas_used;
							full.nEthOutOfGas++;
						} else {
							partial.wastedEthInvalid += receipt.cumulative_gas_used;
							partial.nEthInvalid++;
							full.wastedEthInvalid += receipt.cumulative_gas_used;
							full.nEthInvalid++;
						}
					}
					partial.gasEth += receipt.cumulative_gas_used;
					partial.nEth++;
					full.gasEth += receipt.cumulative_gas_used;
					full.nEth++;
				} else {
					if(!receipt.success) {
						if(receipt.cumulative_gas_used == tx.gas_limit) {
							partial.wastedContractOutOfGas += receipt.cumulative_gas_used;
							partial.nContractOutOfGas++;
							full.wastedContractOutOfGas += receipt.cumulative_gas_used;
							full.nContractOutOfGas++;
						} else {
							partial.wastedContractInvalid += receipt.cumulative_gas_used;
							partial.nContractInvalid++;
							full.wastedContractInvalid += receipt.cumulative_gas_used;
							full.nContractInvalid++;
						}
					}
					partial.gasContract += receipt.cumulative_gas_used;
					partial.nContract++;
					full.gasContract += receipt.cumulative_gas_used;
					full.nContract++;
				}
				if(!receipt.success) {
//std::cout << "ERRORE:" << to_hex(*tx.from);
//printf(" => %ld:%d,%ld,%ld\n",block_num,i,receipt.cumulative_gas_used,tx.gas_limit);
					if(receipt.cumulative_gas_used == tx.gas_limit) {
						partial.wastedOutOfGas += receipt.cumulative_gas_used;
						partial.nOutOfGas++;
						full.wastedOutOfGas += receipt.cumulative_gas_used;
						full.nOutOfGas++;
					} else {
						partial.wastedInvalid += receipt.cumulative_gas_used;
						partial.nInvalid++;
						full.wastedInvalid += receipt.cumulative_gas_used;
						full.nInvalid++;
					}
					partial.wasted += receipt.cumulative_gas_used;
					full.wasted += receipt.cumulative_gas_used;
				}
//else
//printf("OK:%ld:%d,%ld,%ld\n",block_num,i,receipt.cumulative_gas_used,tx.gas_limit);
			}
		}
		if(block_printed != block_num && (block_num % 10000) == 0) {
			printf("%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",block_num,partial.nErrors,partial.nTxs,partial.nOutOfGas,partial.nInvalid,partial.nEth,
					partial.nEthOutOfGas,partial.nEthInvalid,partial.nContract, partial.nContractOutOfGas,partial.nContractInvalid,
					partial.gas,partial.wasted,partial.wastedOutOfGas,partial.wastedInvalid,partial.gasEth,partial.wastedEthOutOfGas,
					partial.wastedEthInvalid,partial.gasContract,partial.wastedContractOutOfGas,partial.wastedContractInvalid);
			memset(&partial,'\0',sizeof(partial));
			block_printed = block_num;
		}
	}
	printf("%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",block_num,partial.nErrors,partial.nTxs,partial.nOutOfGas,partial.nInvalid,partial.nEth,
			partial.nEthOutOfGas,partial.nEthInvalid,partial.nContract, partial.nContractOutOfGas,partial.nContractInvalid,
			partial.gas,partial.wasted,partial.wastedOutOfGas,partial.wastedInvalid,partial.gasEth,partial.wastedEthOutOfGas,
			partial.wastedEthInvalid,partial.gasContract,partial.wastedContractOutOfGas,partial.wastedContractInvalid);
	printf("------------------------------------------------------------------------------------------------------\n");
	printf("%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",block_num,full.nErrors,full.nTxs,full.nOutOfGas,full.nInvalid,full.nEth,
			full.nEthOutOfGas,full.nEthInvalid,full.nContract, full.nContractOutOfGas,full.nContractInvalid,
			full.gas,full.wasted,full.wastedOutOfGas,full.wastedInvalid,full.gasEth,full.wastedEthOutOfGas,
			full.wastedEthInvalid,full.gasContract,full.wastedContractOutOfGas,full.wastedContractInvalid);

	//t1 = absl::Now();
	return 0;
}
