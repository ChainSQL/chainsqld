#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    int32_t read_action_data(void *msg, int32_t len);
    int32_t action_data_size();
    void set_action_return_value(void *return_value, int32_t size);
#ifdef __cplusplus
}
#endif