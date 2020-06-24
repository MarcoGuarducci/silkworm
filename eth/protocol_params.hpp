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

#ifndef SILKWORM_ETH_PROTOCOL_PARAMS_H_
#define SILKWORM_ETH_PROTOCOL_PARAMS_H_

#include <stdint.h>

namespace silkworm::eth::params {

uint64_t kTxGas{21000};
uint64_t kTxGasContractCreation{53000};
uint64_t kTxDataZeroGas{4};
uint64_t kTxDataNonZeroGasFrontier{68};
uint64_t kTxDataNonZeroGasEIP2028{16};

}  // namespace silkworm::eth::params

#endif  // SILKWORM_ETH_PROTOCOL_PARAMS_H_