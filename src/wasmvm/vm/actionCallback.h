#pragma once

#include <functional>
#include <algorithm>
#include <string>
#include <cstring>
#include <utility>

#include "vm/action.h"
#include "wasm3/platforms/cpp/wasm3_cpp/include/wasm3_cpp.h"

namespace chainsql
{
    template <typename T>
    class actionCallback;

    template <typename T>
    class actionCallback
    {
    private:
        using Result = T;
        const action &action_;
        Result result_;
    public:
        std::function<int32_t(void *, int32_t)> read_action_data =
            std::bind(&actionCallback<T>::read_action_data_impl, this, std::placeholders::_1, std::placeholders::_2);
        std::function<int32_t()> action_data_size = std::bind(&actionCallback<T>::action_data_size_impl, this);
        std::function<void(void *, int32_t)> set_action_return_value =
            std::bind(&actionCallback<T>::set_action_return_value_impl, this, std::placeholders::_1, std::placeholders::_2);

        actionCallback(const action &ac, wasm3::module &mod)
            : action_(ac)
        {
            mod.link("*", "read_action_data", &read_action_data);
            mod.link("*", "action_data_size", &action_data_size);
            mod.link("*", "set_action_return_value", &set_action_return_value);
        }

        const action& Action() const {
            return action_;
        }

        Result result() 
        {
            return result_;
        }

    private:
        int32_t read_action_data_impl(void *msg, int32_t len)
        {
            int32_t size = std::min(len, (int32_t)action_.payload().size());
            std::memcpy(msg, action_.payload().c_str(), size);
            return size;
        }

        int32_t action_data_size_impl()
        {
            return action_.payload().size();
        }

        void set_action_return_value_impl(void *return_value, int32_t size)
        {
            std::memcpy(&result_, return_value, size);
        }

    };

    template <>
    class actionCallback<void>
    {
    private:
        using Result = void;
        const action &action_;

    public:
        std::function<int32_t(void *, int32_t)> read_action_data =
            std::bind(&actionCallback<void>::read_action_data_impl, this, std::placeholders::_1, std::placeholders::_2);

        std::function<int32_t()> action_data_size = std::bind(&actionCallback<void>::action_data_size_impl, this);
        std::function<void(void *, int32_t)> set_action_return_value =
            std::bind(&actionCallback<void>::set_action_return_value_impl, this, std::placeholders::_1, std::placeholders::_2);

        actionCallback(const action &ac, wasm3::module &mod)
            : action_(ac)
        {
            mod.link("*", "read_action_data", &read_action_data);
            mod.link("*", "action_data_size", &action_data_size);
            mod.link_optional("*", "set_action_return_value", &set_action_return_value);
        }

        const action& Action() const {
            return action_;
        }

        Result result() {}

    private:

        int32_t read_action_data_impl(void *msg, int32_t len)
        {
            int32_t size = std::min(len, (int32_t)action_.payload().size());
            std::memcpy(msg, action_.payload().c_str(), size);
            return size;
        }

        int32_t action_data_size_impl()
        {
            return action_.payload().size();
        }

        void set_action_return_value_impl(void *return_value, int32_t size)
        {
        }
    };
} // namespace chainsql
