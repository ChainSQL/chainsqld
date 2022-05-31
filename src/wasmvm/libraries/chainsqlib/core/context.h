#pragma once

namespace chainsql {
    namespace internal_use_do_not_use
    {
        extern "C" uint64_t chainsql_contract_address();
    }

    inline uint64_t current_context_contract() { return internal_use_do_not_use::chainsql_contract_address(); }
} // namespace chainsql