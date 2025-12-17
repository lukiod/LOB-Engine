#pragma once

#include "LOB/Types.h"

namespace LOB {

struct Limit;

struct Order {
    OrderID id;
    Price price;
    Quantity size;
    Side side;
    uint64_t timestamp;

    // Intrusive Doubly Linked List Pointers
    Order* prev;
    Order* next;
    
    // Pointer to parent Limit level (optional, but useful for O(1) cancel)
    Limit* parentLimit;

    Order() 
        : id(INVALID_ORDER_ID), price(INVALID_PRICE), size(INVALID_QUANTITY), 
          side(Side::Buy), timestamp(0), prev(nullptr), next(nullptr), parentLimit(nullptr) {}

    // Reset for SlabAllocator reuse
    void reset() {
        id = INVALID_ORDER_ID;
        price = INVALID_PRICE;
        size = INVALID_QUANTITY;
        timestamp = 0;
        prev = nullptr;
        next = nullptr;
        parentLimit = nullptr;
    }
};

}
