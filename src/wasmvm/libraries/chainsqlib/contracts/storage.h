#pragma once

#include <vector>

#include "chainsqlib/core/datastream.h"
#include "chainsqlib/core/context.h"

extern "C" {
    int64_t kv_set(const void *key, uint32_t key_size,
                   const void *value, uint32_t value_size);
    /*bool*/ int32_t kv_get(const void *key, uint32_t key_size, uint32_t &value_size);
    uint32_t kv_get_data(uint32_t offset, void *data, uint32_t data_size);
    int64_t kv_erase(const void *key, uint32_t key_size);
}

namespace chainsql {
    /**
     * @brief Set the State object
     * 
     * @tparam KEY Key type
     * @tparam VALUE Value type
     * @param key Key
     * @param value Value
     */
    template <typename KEY, typename VALUE>
    inline void setState(const KEY &key, const VALUE &value) {
        std::vector<char> vecKey(pack_size(key));
        std::vector<char> vecValue(pack_size(value));
        datastream<char*> keyStream(vecKey.data(), vecKey.size());
        datastream<char*> valueStream(vecValue.data(), vecValue.size());
        keyStream << key;
        valueStream << value;
        kv_set(vecKey.data(), vecKey.size(), vecValue.data(), vecValue.size());
    }

    /**
     * @brief Get the State object
     * 
     * @tparam KEY Key type
     * @tparam VALUE Value type
     * @param key Key
     * @param value Value
     * @return size_t Get the length of the data
     */
    template <typename KEY, typename VALUE>
    inline size_t getState(const KEY &key, VALUE &value) {
        std::vector<char> vecKey(pack_size(key));
        datastream<char*> keyStream(vecKey.data(), vecKey.size());
        keyStream << key;
        uint32_t len = 0;
        kv_get(vecKey.data(), vecKey.size(), len);
        if (len == 0){ return 0; }
        std::vector<char> vecValue(len);
        kv_get_data(0, vecValue.data(), vecValue.size());

        datastream<char*> valueStream(vecValue.data(), vecValue.size());
        valueStream >> value;
        return len;
    }

    /**
     * @brief delete State Object
     * 
     * @tparam KEY Key type
     * @param key Key
     */
    template <typename KEY>
    inline void delState( const KEY &key) {
        std::vector<char> vecKey(pack_size(key));
        datastream<char*> keyStream(vecKey.data(), vecKey.size());
        keyStream << key;
        ::kv_erase(vecKey.data(), vecKey.size());
    }

}
