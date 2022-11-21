#pragma once

#include <memory>
#include <vector>
#include <tuple>

#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/base_uint.h>
#include <ripple/json/json_value.h>

#include <peersafe/schema/Schema.h>

namespace ripple {
namespace Helper {

std::tuple<Json::Value, bool>
getLogsByLedger(Schema& schame, const Ledger* ledger);

Json::Value
filterLogs(const Json::Value& unfilteredLogs,
           const std::uint32_t from,
           const std::uint32_t to,
           const std::vector<uint160>& addresses,
           const std::vector<std::vector<uint256>>& topics);

} // namespace Helper
} // namespace ripple
