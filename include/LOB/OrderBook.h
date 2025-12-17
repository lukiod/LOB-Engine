#pragma once

#include <map>
#include <unordered_map>
#include <functional>
#include "LOB/Types.h"
#include "LOB/Order.h"
#include "LOB/Limit.h"
#include "LOB/SlabAllocator.h"

namespace LOB {

class OrderBook {
public:
    OrderBook() : orderAllocator_(1000000) {}
    
    ~OrderBook() {
        for (auto& pair : bids_) {
            delete pair.second;
        }
        for (auto& pair : asks_) {
            delete pair.second;
        }
    }
    
    // Helper to find limit (Public for Main healing)
    Limit* getOrCreateLimit(Price price, Side side) {
        if (side == Side::Buy) {
            auto it = bids_.find(price);
            if (it != bids_.end()) return it->second;
            
            Limit* limit = new Limit(price); 
            bids_[price] = limit;
            return limit;
        } else {
            auto it = asks_.find(price);
            if (it != asks_.end()) return it->second;
            
            Limit* limit = new Limit(price);
            asks_[price] = limit;
            return limit;
        }
    }

    // Add a new order
    // For LOBSTER, 'Add' means a new limit order submission
    // We assume the parser provides valid inputs.
    void addOrder(OrderID id, Price price, Quantity size, Side side, uint64_t timestamp) {
        if (orderLookup_.find(id) != orderLookup_.end()) {
            return; // Duplicate ID, ignore or handle error
        }

        // Allocate Order from Slab
        Order* order = orderAllocator_.allocate();
        // Placement new or manual init
        order->id = id;
        order->price = price;
        order->size = size;
        order->side = side;
        order->timestamp = timestamp;
        order->prev = nullptr;
        order->next = nullptr;
        order->parentLimit = nullptr;

        // Find or create Limit level
        Limit* limit = getOrCreateLimit(price, side);
        limit->addOrder(order);
        
        // Add to O(1) lookup
        orderLookup_[id] = order;
    }

    // Initialize level (for starting from a snapshot)
    void addLevel(Price price, Quantity size, Side side) {
        Limit* limit = getOrCreateLimit(price, side);
        limit->totalVolume += size;
        // checking orderCount is 0, so head/tail are nullptr.
        // This effectively creates "Dark Matter" volume that we track but can't name.
    }

    // Cancel/Delete Order
    // Updated to handle cases where we don't have the OrderID (pre-snapshot orders)
    // Cancel an order by ID
    // Returns true if found and canceled
    bool cancelOrder(OrderID id) {
        auto it = orderLookup_.find(id);
        if (it != orderLookup_.end()) {
            Order* order = it->second;
            Limit* limit = order->parentLimit;
            limit->removeOrder(order);
            if (limit->isEmpty() && limit->totalVolume == 0) {
                removeLimit(limit);
            }
            orderAllocator_.deallocate(order);
            orderLookup_.erase(it);
            return true;
        }
        return false;
    }
    
    // Overloaded for convenience/backward compat if needed, but we should change the main interface
    // LOBSTER Type 3 (Delete) has: Timestamp, Type, ID, Size, Price, Direction.
    void deleteOrder(OrderID id, Price price, Quantity size, Side side) {
        auto it = orderLookup_.find(id);
        if (it != orderLookup_.end()) {
            // We found the order, just remove it standard way. 
            // We assume the size matches what we have or we just trust the ID removal.
            Order* order = it->second;
            Limit* limit = order->parentLimit;
            limit->removeOrder(order);
             if (limit->isEmpty() && limit->totalVolume == 0) {
                 removeLimit(limit);
             }
             orderAllocator_.deallocate(order);
             orderLookup_.erase(it);
        } else {
            // Fallback
             Limit* limit = getLimit(price, side);
             if (limit) {
                 if (size > limit->totalVolume) limit->totalVolume = 0; // Safety clamp
                 else limit->totalVolume -= size;
                 
                 // If volume hits 0 (and no orders), remove limit
                 if (limit->totalVolume == 0 && limit->orderCount == 0) {
                     removeLimit(limit);
                 }
             }
        }
    }

    // Partial Cancel (Type 2)
    void reduceOrder(OrderID id, Quantity reductionSize, Price price, Side side) {
        auto it = orderLookup_.find(id);
        if (it != orderLookup_.end()) {
            Order* order = it->second;
            if (reductionSize >= order->size) {
                 // Convert to delete
                 // Re-find to avoid iterator issues or just call logic directly
                 // We call internal remove
                 Limit* limit = order->parentLimit;
                 limit->removeOrder(order);
                 if (limit->isEmpty() && limit->totalVolume == 0) removeLimit(limit);
                 orderAllocator_.deallocate(order);
                 orderLookup_.erase(it);
            } else {
                order->size -= reductionSize;
                order->parentLimit->totalVolume -= reductionSize;
            }
        } else {
            // Fallback
            Limit* limit = getLimit(price, side);
            if (limit) {
                 if (reductionSize > limit->totalVolume) limit->totalVolume = 0;
                 else limit->totalVolume -= reductionSize;
                  if (limit->totalVolume == 0 && limit->orderCount == 0) removeLimit(limit);
            }
        }
    }

    // Execution (Partial or Full)
    void executeOrder(OrderID id, Quantity executedSize, Price price, Side side) {
        reduceOrder(id, executedSize, price, side);
    }

    // Get Best Bid/Ask
    Price getBestBid() const {
        if (bids_.empty()) return INVALID_PRICE;
        return bids_.begin()->first;
    }
    
    // Helper to find limit without creating
    Limit* getLimit(Price price, Side side) {
        if (side == Side::Buy) {
            auto it = bids_.find(price);
            if (it != bids_.end()) return it->second;
        } else {
            auto it = asks_.find(price);
            if (it != asks_.end()) return it->second;
        }
        return nullptr;
    }

    Price getBestAsk() const {
        if (asks_.empty()) return INVALID_PRICE;
        return asks_.begin()->first;
    }

    Quantity getVolumeAtPrice(Price price) const {
        // Check bids
        auto bit = bids_.find(price);
        if (bit != bids_.end()) {
            return bit->second->totalVolume;
        }
        // Check asks
        auto ait = asks_.find(price);
        if (ait != asks_.end()) {
            return ait->second->totalVolume;
        }
        return 0;
    }

    // Quant Features
    
    // Order Book Imbalance (OBI) = (BestBidSize - BestAskSize) / (BestBidSize + BestAskSize)
    // Returns value between -1 (Selling Pressure) and 1 (Buying Pressure)
    double getOBI() const {
        Price bid = getBestBid();
        Price ask = getBestAsk();
        
        if (bid == INVALID_PRICE || ask == INVALID_PRICE) return 0.0;
        
        Quantity bidSize = getVolumeAtPrice(bid);
        Quantity askSize = getVolumeAtPrice(ask);
        
        if (bidSize + askSize == 0) return 0.0;
        
        return static_cast<double>(static_cast<int64_t>(bidSize) - static_cast<int64_t>(askSize)) / static_cast<double>(bidSize + askSize);
    }

    // Microprice = (BestBid * BestAskSize + BestAsk * BestBidSize) / (BestBidSize + BestAskSize)
    double getMicroprice() const {
        Price bid = getBestBid();
        Price ask = getBestAsk();
        
        if (bid == INVALID_PRICE || ask == INVALID_PRICE) return 0.0;
        
        Quantity bidSize = getVolumeAtPrice(bid);
        Quantity askSize = getVolumeAtPrice(ask);
        
        if (bidSize + askSize == 0) return 0.0;
        
        return (static_cast<double>(bid * askSize) + static_cast<double>(ask * bidSize)) / static_cast<double>(bidSize + askSize);
    }

    // Diagnostics/Verification helper
    size_t getOrderCount() const { return orderLookup_.size(); }

private:
    // Buy side: High prices first (descending)
    std::map<Price, Limit*, std::greater<Price>> bids_;
    // Sell side: Low prices first (ascending)
    std::map<Price, Limit*, std::less<Price>> asks_;
    
    // O(1) Order Lookup
    std::unordered_map<OrderID, Order*> orderLookup_;

    // Memory Pool
    SlabAllocator<Order> orderAllocator_;

    void removeLimit(Limit* limit) {
        if (limit->totalVolume > 0) return; // Safety check
        auto bidIt = bids_.find(limit->limitPrice);
        if (bidIt != bids_.end() && bidIt->second == limit) {
            bids_.erase(bidIt);
            delete limit;
            return;
        }

        auto askIt = asks_.find(limit->limitPrice);
        if (askIt != asks_.end() && askIt->second == limit) {
            asks_.erase(askIt);
            delete limit;
            return;
        }
    }
};

}
