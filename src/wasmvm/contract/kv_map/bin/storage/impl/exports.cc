#include <stdint.h>
#include <map>
#include <tuple>
#include <vector>
#include <string>

using key_type = uint64_t;
using value_type = std::map<std::string, std::string>;
using storage_type = std::map<key_type, value_type>;

using value_iterator = value_type::iterator;
using storage_iterator = storage_type::iterator;

storage_type storage;

struct IteratorHandle {
    std::string prefix;
    value_iterator index;
    value_type* map;
};
std::vector<IteratorHandle*> IteratorHandles;

#define HANDLE(itr) \
    IteratorHandle *handle = (IteratorHandle*)((void*)itr);

value_iterator __IT__;

extern "C"
{
    uint64_t contract = 100;
    void chainsql_assert(int32_t test, const void *msg)
    {
    }

    uint64_t chainsql_contract_address()
    {
        return 0;
    }

    int64_t kv_erase(const void *key, uint32_t key_size)
    {
        auto it = storage.find(contract);
        if (it != storage.end()) {
            auto key_it = it->second.find(std::string((const char*)key, key_size));
            it->second.erase(key_it);
        }
        return 0;
    }

    int64_t kv_set(const void *key, uint32_t key_size,
                   const void *value, uint32_t value_size)
    {
        auto it = storage.find(contract);
        if(it == storage.end()) {
            value_type mv;
            mv[std::string((const char*)key, key_size)] = std::string((const char*)value, value_size);
            storage.emplace(std::make_pair(contract, mv));
        } else {
            it->second.insert_or_assign(std::string((const char*)key, key_size), std::string((const char*)value, value_size));
        }
        
        return 0;
    }

    /*bool*/ int32_t kv_get(const void *key, uint32_t key_size, uint32_t &value_size)
    {
        int ret = 0;
        do {
            auto it = storage.find(contract);
            if (it == storage.end())
                break;
            auto key_str = std::string((const char*)key, key_size);
            auto key_it = it->second.find(key_str);
            if (key_it == it->second.end())
                break;
            value_size = key_it->second.size();
            __IT__ = key_it;
            ret = 1;
        } while(0);
        return ret;
    }

    uint32_t kv_get_data(uint32_t offset, void *data, uint32_t data_size)
    {
        std::string c = __IT__->second;
        std::memcpy(data, c.c_str(), data_size);
        return c.size();
    }

    
    uint64_t kv_it_create(const void *prefix, uint32_t size)
    {
        auto it = storage.find(contract);
        if (it == storage.end())
            return 0;
        
        IteratorHandle* handle = new IteratorHandle{
            std::string((const char*)prefix, size),
            it->second.begin(),
            &it->second};
        
        
        return (uint64_t)((void*)handle);
    }

    void kv_it_destroy(uint64_t itr)
    {
        HANDLE(itr)
        delete handle;
    }

    int32_t kv_it_status(uint64_t itr)
    {
        HANDLE(itr)
        if (handle->index == handle->map->end())
            return -1;
        return 0;
    }

    int32_t kv_it_compare(uint64_t itr_a, uint64_t itr_b)
    {
        IteratorHandle *handle_a = (IteratorHandle*)((void*)itr_a);
        IteratorHandle *handle_b = (IteratorHandle*)((void*)itr_b);
        if (handle_a->index == handle_b->index
            && handle_a->map == handle_b->map)
            return 0;
        return 1;
    }

    int32_t kv_it_key_compare(uint64_t itr, const void *key, uint32_t size)
    {
        std::string key_str = std::string((const char*)key, size);
        HANDLE(itr)
        if(handle->index == handle->map->end()) {
            return -1;
        }
        if(handle->index->first != key_str)
            return 1;
        return 0;
    }

    int32_t kv_it_move_to_end(uint64_t itr)
    {
        HANDLE(itr)
        handle->index = handle->map->end();
        return -2;
    }

    int32_t kv_it_next(uint64_t itr, uint32_t &found_key_size, uint32_t &found_value_size)
    {
        HANDLE(itr)
        if(handle->index == handle->map->end()) {
            return  -1;
        }
        handle->index++;
        if(handle->index == handle->map->end()) {
            return -1;
        }
        
        found_key_size = handle->index->first.size();
        found_value_size = handle->index->second.size();
        
        return 0;
    }

    int32_t kv_it_prev(uint64_t itr, uint32_t &found_key_size, uint32_t &found_value_size)
    {
        HANDLE(itr)
        if(handle->index == handle->map->begin()) {
            return -1;
        }
        
        handle->index--;
        if(handle->index == handle->map->end()) {
            return -1;
        }
        
        found_key_size = handle->index->first.size();
        found_value_size = handle->index->second.size();
        
        return 0;
    }

    int32_t kv_it_lower_bound(uint64_t itr, const void *key, uint32_t size,
                              uint32_t &found_key_size, uint32_t &found_value_size)
    {
        HANDLE(itr)
        if (size == 0) {
            handle->index = handle->map->begin();
            found_key_size = handle->index->first.size();
            found_value_size = handle->index->second.size();
            return 0;
        }
        
        std::string key_str = std::string((const char*)key, size);
        handle->index = handle->map->lower_bound(key_str);
        if(handle->index == handle->map->end()) {
            return -1;
        }
        found_key_size = handle->index->first.size();
        found_value_size = handle->index->second.size();
        return 0;
    }

    int32_t kv_it_key(uint64_t itr, uint32_t offset, void *dest, uint32_t size, uint32_t &actual_size)
    {
        HANDLE(itr)
        uint32_t prefix_size = handle->prefix.size();
        std::string key = handle->index->first.substr(prefix_size);
        if(size >= key.size()) {
            std::memcpy(dest, key.c_str(), key.size());
        }
        actual_size = key.size();
        return handle->index == handle->map->end() ? -1 : 0;
    }

    int32_t kv_it_value(uint64_t itr, uint32_t offset, void *dest, uint32_t size, uint32_t &actual_size)
    {
        HANDLE(itr)
        std::string value = handle->index->second;
        if(size >= value.size()) {
            std::memcpy(dest, value.c_str(), value.size());
        }
        actual_size = value.size();
        return handle->index == handle->map->end() ? -1 : 0;
    }
}
