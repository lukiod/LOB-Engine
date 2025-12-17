#pragma once

#include "LOB/Types.h"
#include "LOB/Order.h"

namespace LOB {

struct Limit {
    Price limitPrice;
    Quantity totalVolume;
    uint32_t orderCount;

    Order* head;
    Order* tail;

    Limit(Price price) 
        : limitPrice(price), totalVolume(0), orderCount(0), head(nullptr), tail(nullptr) {}

    void addOrder(Order* order) {
        order->parentLimit = this;
        // Append to tail (time priority)
        if (tail == nullptr) {
            head = order;
            tail = order;
            order->prev = nullptr;
            order->next = nullptr;
        } else {
            tail->next = order;
            order->prev = tail;
            order->next = nullptr;
            tail = order;
        }
        totalVolume += order->size;
        orderCount++;
    }

    void removeOrder(Order* order) {
        if (order->prev != nullptr) {
            order->prev->next = order->next;
        } else {
            // Removing head
            head = order->next;
        }

        if (order->next != nullptr) {
            order->next->prev = order->prev;
        } else {
            // Removing tail
            tail = order->prev;
        }

        totalVolume -= order->size;
        orderCount--;
    }
    
    bool isEmpty() const {
        return orderCount == 0;
    }
};

}
