#pragma once

#include <map>
#include <mutex>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
        
        Access(std::map<Key, Value>& bucket, std::mutex& m, const Key& key)
            : guard(m), ref_to_value(bucket[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count) 
        : buckets_(bucket_count),
          buckets_mutexes_(bucket_count), 
          buckets_count_(bucket_count) {
    }

    Access operator[](const Key& key) {
        size_t bucket_index = static_cast<size_t>(key) % buckets_count_;
        
        return {buckets_[bucket_index], buckets_mutexes_[bucket_index], key};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> res;
        
        for (size_t i = 0; i < buckets_count_; ++i) {
            buckets_mutexes_[i].lock();
        }
        
        for (size_t i = 0; i < buckets_count_; ++i) {
            res.insert(buckets_[i].begin(), buckets_[i].end());
        }
        
        for (size_t i = 0; i < buckets_count_; ++i) {
            buckets_mutexes_[i].unlock();
        }
        
        return res;
    }

private:
    std::vector<std::map<Key, Value>> buckets_;
    std::vector<std::mutex> buckets_mutexes_;
    size_t buckets_count_;
};
