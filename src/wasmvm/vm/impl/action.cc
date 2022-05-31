#include <functional>
#include <algorithm>
#include <cstring>
#include <utility>

#include "vm/action.h"

namespace chainsql {
    //action::action(
    //    const name &code,
    //    const name &func,
    //    const std::string &payload)
    //    : code_(code)
    //    , function_(func)
    //    , payload_(payload)
    //{
    //}

    //action::~action()
    //{
    //    
    //}

    //void action::load(wasm3::module &module)
    //{
    //    native::intrinsics::get().set_intrinsic<native::intrinsics::read_action_data>(
    //        std::bind(&action::read_action_data_impl, this, std::placeholders::_1, std::placeholders::_2));

    //    native::intrinsics::get().set_intrinsic<native::intrinsics::action_data_size>(
    //        std::bind(&action::action_data_size_impl, this));

    //    native::intrinsics::get().set_intrinsic<native::intrinsics::set_action_return_value>(
    //        std::bind(&action::set_action_return_value_impl, this, std::placeholders::_1, std::placeholders::_2));

    //    native::intrinsics::get().set_intrinsic<native::intrinsics::chainsql_assert>(
    //        std::bind(&action::chainsql_assert_impl, this, std::placeholders::_1, std::placeholders::_2));

    //    module.link("*", "read_action_data", read_action_data);
    //    module.link("*", "action_data_size", action_data_size);
    //    module.link("*", "set_action_return_value", set_action_return_value);
    //    module.link("*", "chainsql_assert", chainsql_assert);
    //}

    //int32_t action::read_action_data_impl(void *msg, int32_t len) {
    //    std::cout << "read_action_data" << std::endl;
    //    int32_t size = std::min(len, (int32_t)payload_.size());
    //    std::memcpy(msg, payload_.c_str(), size);
    //    return size;
    //}

    //int32_t action::action_data_size_impl() {
    //    return payload_.size();
    //}

    //void action::set_action_return_value_impl(void *return_value, int32_t size) {
    //    std::memcpy(&result_, return_value, size);
    //    std::cout << "result = " << p << std::endl;
    //}

    //void action::chainsql_assert_impl(int32_t test, const void *msg) {

    //}
} // ns chainsql
