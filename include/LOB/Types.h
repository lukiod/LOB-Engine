#pragma once

#include <cstdint>
#include <limits>

namespace LOB {

using Price = int64_t;
using Quantity = uint64_t;
using OrderID = uint64_t;

// Constants for invalid/max values
constexpr Price INVALID_PRICE = std::numeric_limits<Price>::min();
constexpr Quantity INVALID_QUANTITY = 0;
constexpr OrderID INVALID_ORDER_ID = 0;

enum class Side : int8_t {
    Buy = 1,
    Sell = -1
};

}
