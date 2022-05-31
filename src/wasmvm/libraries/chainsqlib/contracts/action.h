#pragma once

namespace chainsql
{
    namespace internal_use_do_not_use
    {
        extern "C"
        {
            int32_t read_action_data(void *msg, int32_t len);
            int32_t action_data_size();
            void set_action_return_value(void *return_value, int32_t size);
        }
    }

    inline int32_t read_action_data(void *msg, int32_t len)
    {
        return internal_use_do_not_use::read_action_data(msg, len);
    }

    inline int32_t action_data_size()
    {
        return internal_use_do_not_use::action_data_size();
    }

    inline void set_action_return_value(void *return_value, int32_t size) {
        internal_use_do_not_use::set_action_return_value(return_value, size);
    }
}