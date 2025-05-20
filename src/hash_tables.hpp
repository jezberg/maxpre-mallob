
#pragma once

#ifdef MAXPRE_USE_TSL_TABLES


#include "tsl/robin_set.h"
#include "tsl/robin_map.h"

template <typename K>
using hashset = tsl::robin_set<K>;

template <typename K, typename V>
using hashmap = tsl::robin_map<K, V>;


#else


#include <unordered_set>
#include <unordered_map>

template <typename K>
using hashset = std::unordered_set<K>;

template <typename K, typename V>
using hashmap = std::unordered_map<K, V>;


#endif
