#pragma once

#include <map>
#include <mutex>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Bucket {
        std::map<Key, Value> bucket_map;
        std::mutex m;
    };

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
        
        Access(std::map<Key, Value>& bucket, std::mutex& m, const Key& key)
            : guard(m), ref_to_value(bucket[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count) 
        : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        size_t bucket_index = static_cast<size_t>(key) % buckets_.size();
        
        return {buckets_[bucket_index].bucket_map, buckets_[bucket_index].m, key};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> res;

        for (size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i].m.lock();
        }
        
        for (size_t i = 0; i < buckets_.size(); ++i) {
            res.insert(buckets_[i].bucket_map.begin(), buckets_[i].bucket_map.end());
        }
        
        for (size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i].m.unlock();
        }
        
        return res;
    }

private:
    std::vector<Bucket> buckets_;
};
