#include <gtest/gtest.h>
#include "LOB/OrderBook.h"

// Test Basic Order Addition
TEST(OrderBookTest, AddOrder) {
    LOB::OrderBook book;
    book.addOrder(1, 100, 10, LOB::Side::Buy, 0);
    
    EXPECT_EQ(book.getBestBid(), 100);
    EXPECT_EQ(book.getVolumeAtPrice(100), 10);
    EXPECT_EQ(book.getOrderCount(), 1);
}

// Test Price Priority
TEST(OrderBookTest, PricePriority) {
    LOB::OrderBook book;
    book.addOrder(1, 100, 10, LOB::Side::Buy, 0);
    book.addOrder(2, 101, 10, LOB::Side::Buy, 0); // Higher bid should be best
    
    EXPECT_EQ(book.getBestBid(), 101);
}

// Test Cancellation
TEST(OrderBookTest, CancelOrder) {
    LOB::OrderBook book;
    book.addOrder(1, 100, 10, LOB::Side::Buy, 0);
    bool result = book.cancelOrder(1);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(book.getVolumeAtPrice(100), 0);
    EXPECT_EQ(book.getOrderCount(), 0);
}

// Test Execution
TEST(OrderBookTest, ExecuteOrder) {
    LOB::OrderBook book;
    book.addOrder(1, 100, 10, LOB::Side::Buy, 0);
    
    // Partial execution
    book.executeOrder(1, 4, 100, LOB::Side::Buy);
    EXPECT_EQ(book.getVolumeAtPrice(100), 6);
    
    // Full execution
    book.executeOrder(1, 6, 100, LOB::Side::Buy);
    EXPECT_EQ(book.getVolumeAtPrice(100), 0);
    // Should be removed
    EXPECT_EQ(book.getOrderCount(), 0);
}

// Test OBI Calculation
TEST(OrderBookTest, OBI) {
    LOB::OrderBook book;
    book.addOrder(1, 100, 100, LOB::Side::Buy, 0);
    book.addOrder(2, 105, 50, LOB::Side::Sell, 0);
    
    // OBI = (100 - 50) / (100 + 50) = 50 / 150 = 0.333...
    double obi = book.getOBI();
    EXPECT_NEAR(obi, 0.333333, 1e-5);
}

// Test Self-Healing / Dummy Orders
TEST(OrderBookTest, HealingDummy) {
    LOB::OrderBook book;
    // Simulate a missing level appearing logic
    LOB::Limit* limit = book.getOrCreateLimit(200, LOB::Side::Sell);
    EXPECT_EQ(limit->totalVolume, 0);
    
    // Manually inject volume via "Healing" logic simulation
    // (In reality, main.cpp does this, but we can test the OrderBook methods used)
    
    // If we use the public addOrder, it works
    book.addOrder(999, 200, 500, LOB::Side::Sell, 0);
    
    EXPECT_EQ(book.getVolumeAtPrice(200), 500);
    EXPECT_EQ(book.getBestAsk(), 200);
}
