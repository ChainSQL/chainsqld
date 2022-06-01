#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
    int64_t kv_set(const void *key, uint32_t key_size,
                   const void *value, uint32_t value_size);
    /*bool*/ int32_t kv_get(const void *key, uint32_t key_size, uint32_t &value_size);
    uint32_t kv_get_data(uint32_t offset, void *data, uint32_t data_size);
    int64_t kv_erase(const void *key, uint32_t key_size);

    // for iterator
    uint64_t kv_it_create(const void *prefix, uint32_t size);
    void kv_it_destroy(uint64_t itr);
    int32_t kv_it_status(uint64_t itr);
    int32_t kv_it_compare(uint64_t itr_a, uint64_t itr_b);
    int32_t kv_it_key_compare(uint64_t itr, const void *key, uint32_t size);
    int32_t kv_it_move_to_end(uint64_t itr);
    int32_t kv_it_next(uint64_t itr, uint32_t &found_key_size, uint32_t &found_value_size);
    int32_t kv_it_prev(uint64_t itr, uint32_t &found_key_size, uint32_t &found_value_size);
    int32_t kv_it_lower_bound(uint64_t itr, const void *key, uint32_t size,
                              uint32_t &found_key_size, uint32_t &found_value_size);
    int32_t kv_it_key(uint64_t itr, uint32_t offset, void *dest, uint32_t size, uint32_t &actual_size);
    int32_t kv_it_value(uint64_t itr, uint32_t offset, void *dest, uint32_t size, uint32_t &actual_size);
#ifdef __cplusplus
}
#endif