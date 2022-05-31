#pragma once

extern "C"
{

    void chainsql_assert(int32_t test, const void *msg);
    /**
    * 获取合约地址
    * 
    * @return uint64_t 返回地址合约地址指针，实现需要保证地址的有效性
    */
    uint64_t chainsql_contract_address();

    /**
     * @brief 根据健删除 kv-map 中的指定的数据
     * 
     * @param contract 合约地址
     * @param key 键地址
     * @param key_size 键值长度
     * @return int64_t 返回值代表是否删除成功。0 表示成功，-1 表示键值不存在，-2 表示其他错误
     */
    int64_t kv_erase(uint64_t contract, const void *key, uint32_t key_size);

    /**
     * @brief 保存 kv 数据
     * 
     * @param contract 合约地址
     * @param key 键地址。
     * @param key_size 键长度
     * @param value 数据地址
     * @param value_size 数据长度
     * @param payer gas 消费地址
     * @return int64_t 写入 value 的字节数
     */
    int64_t kv_set(uint64_t contract, const void *key, uint32_t key_size,
                   const void *value, uint32_t value_size, uint64_t payer);

    /**
     * @brief 获取指定的键所对应的值是否存在。这个接口需要配合 kv_get 使用。
     * 
     * @param contract 合约地址
     * @param key 键地址
     * @param key_size 键大小
     * @param value_size 键所指向的值的大小，用字节数表示
     * @return int32_t 键所指向的数据是否存在。 0 表示数据不存在，1 表示数据存在
     */
    int32_t kv_get(uint64_t contract, const void *key, uint32_t key_size, uint32_t &value_size);

    /**
     * @brief 获取指定的数据。这个接口需要配合 kv_get 使用。
     * 
     * @param offset 读取数据的偏移量
     * @param data 保存数据的地址，即缓冲区
     * @param data_size 缓冲区大小，用字节数表示。这个值是通过 kv_get获取
     * @return uint32_t 返回获取的数据大小，用字节数表示
     */
    uint32_t kv_get_data(uint32_t offset, void *data, uint32_t data_size);

    /**
     * @brief 创建迭代器
     * 
     * @param contract 合约地址
     * @param prefix 键值前缀。其实在保存 kv 数据的时候，在真正的 key 前面有一段前缀。
     * 通过迭代器获取数据的时候，需要通过这个前缀将真正的 key 给展现出来
     * @param size 前缀长度
     * @return uint32_t 迭代具柄
     */
    uint32_t kv_it_create(uint64_t contract, const void *prefix, uint32_t size);

    /**
     * @brief 销毁迭代器
     * 
     * @param itr 迭代器具柄
     */
    void kv_it_destroy(uint32_t itr);

    /**
     * @brief 获取迭代器指向数据的状态
     * 
     * @param itr 迭代器具柄
     * @return int32_t 0 迭代器指向的数据有效，-1 迭代器指向的数据无效
     */
    int32_t kv_it_status(uint32_t itr);

    /**
     * @brief 比较两个迭代器指向的数据是否相同
     * 
     * @param itr_a 
     * @param itr_b 
     * @return int32_t 0 表示相等，1 表示不想等
     */
    int32_t kv_it_compare(uint32_t itr_a, uint32_t itr_b);

    /**
     * @brief 比较当前迭代器指向数据的 key 和参数指定的相等
     * 
     * @param itr 迭代器具柄
     * @param key 比较的键
     * @param size 键长度，用字节数表示
     * @return int32_t 0 表示相等，1 表示不相等
     */
    int32_t kv_it_key_compare(uint32_t itr, const void *key, uint32_t size);

    /**
     * @brief 将迭代器指向最后一个元素之后的元素
     * 
     * @param itr 迭代器具柄
     * @return int32_t 返回 -1
     */
    int32_t kv_it_move_to_end(uint32_t itr);

    /**
     * @brief 将迭代器移动到当前元素的下一个元素
     * 
     * @param itr 迭代器具柄
     * @param found_key_size 下一个元素的键大小，用字节数表示
     * @param found_value_size 下一个元素的值大小，用字节数表示
     * @return int32_t 表示下一个元素的有效状态。0 表示下一个元素有效，-1 表示下一个元素无效
     */
    int32_t kv_it_next(uint32_t itr, uint32_t &found_key_size, uint32_t &found_value_size);

    /**
     * @brief 将迭代器移动到当前元素的上一个元素 
     * 
     * @param itr 迭代器具柄
     * @param found_key_size 上一个元素的键大小，用字节数表示
     * @param found_value_size 上一个元素的值大小，用字节数表示
     * @return int32_t 表示上一个元素的有效状态。0 表示下一个元素有效，-1 表示下一个元素无效
     */
    int32_t kv_it_prev(uint32_t itr, uint32_t &found_key_size, uint32_t &found_value_size);

    /**
     * @brief 找到等于或大于指定键的值
     * 
     * @param itr 迭代器具柄
     * @param key 键
     * @param size 键大小，用键比较
     * @param found_key_size 找到的健大小，用字节数表示
     * @param found_value_size 找到的值大小，用字节数表示。如果为 0， 表示定位到初始位置
     * @return int32_t 0 表示找到的键值对有效，-1 表示没有找到相关键值对
     */
    int32_t kv_it_lower_bound(uint32_t itr, const void *key, uint32_t size,
                              uint32_t &found_key_size, uint32_t &found_value_size);

    /**
     * @brief 获取当前迭代器指向的键
     * 
     * @param itr 迭代器具柄
     * @param offset 偏移量
     * @param dest 用于存储键的缓冲区
     * @param size 缓冲区大小，用字节数表示
     * @param actual_size 键真实大小，用字节数表示。如果传入的缓冲区为空，那么直接返回键大小
     * @return int32_t 0 表示迭代器当前指向的键有效，-1 表示无效
     */
    int32_t kv_it_key(uint32_t itr, uint32_t offset, void *dest, uint32_t size, uint32_t &actual_size);

    /**
     * @brief 获取当前迭代器指向的值
     * 
     * @param itr 迭代器具柄
     * @param offset 偏移量
     * @param dest 用于存储键的缓冲区
     * @param size 缓冲区大小，用字节数表示
     * @param actual_size 值真实大小，用字节数表示。如果传入的缓冲区为空，那么直接返回值大小
     * @return int32_t 0 表示迭代器当前指向的键有效，-1 表示无效
     */
    int32_t kv_it_value(uint32_t itr, uint32_t offset, void *dest, uint32_t size, uint32_t &actual_size);

}