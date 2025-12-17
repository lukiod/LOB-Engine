#include <benchmark/benchmark.h>
#include "LOB/OrderBook.h"
#include <random>

// Fixture for setting up a book with some depth
class OrderBookFixture : public benchmark::Fixture {
public:
    LOB::OrderBook book;
    std::mt19937 rng{42};
    std::uniform_int_distribution<uint64_t> priceDist{100, 10000};
    
    void SetUp(const benchmark::State&) {
        // Pre-fill book to simulate real state
        for (uint64_t i = 0; i < 1000; ++i) {
            book.addOrder(i, priceDist(rng), 100, LOB::Side::Buy, 0);
            book.addOrder(i + 1000, priceDist(rng), 100, LOB::Side::Sell, 0);
        }
    }
};

// Benchmark adding orders
static void BM_AddOrder(benchmark::State& state) {
    LOB::OrderBook book;
    uint64_t id = 0;
    for (auto _ : state) {
        book.addOrder(++id, 5000, 100, LOB::Side::Buy, 0);
    }
}
BENCHMARK(BM_AddOrder);

// Benchmark matching (execution)
// We set up a scenario where we aggressively cross the spread
static void BM_ExecuteOrder(benchmark::State& state) {
    LOB::OrderBook book;
    // Fill the book with resting orders
    for (uint64_t i = 1; i <= 10000; ++i) {
        book.addOrder(i, 5000, 100, LOB::Side::Sell, 0);
    }
    
    uint64_t execID = 20000;
    for (auto _ : state) {
        // Execute against the resting orders
        // Note: In a real bench we might reset state, but LOB performance often degrades with fragmentation, 
        // so checking 'steady state' execution is valuable.
        // However, once empty, it does nothing. 
        // Let's refill if empty? Or just measure cheap checks?
        // Better: Partial execution against a huge order
        state.PauseTiming();
        book.addOrder(++execID, 5000, 10, LOB::Side::Buy, 0); // Add small crossing order
        state.ResumeTiming();
        
        // This effectively just reduces the volume of the resting order at the head
        // Depending on logic, it might traverse list.
        // Our 'executeOrder' takes an ID. 
        // Actually, we usually execute by matching an Incoming Aggressive Order.
        // But our API `executeOrder(id, size...)` simulates a market execution report.
        book.executeOrder(1, 1, 5000, LOB::Side::Sell);
    }
}
BENCHMARK(BM_ExecuteOrder);

// Benchmark OBI Calculation (should be very fast)
static void BM_GetOBI(benchmark::State& state) {
    LOB::OrderBook book;
    book.addOrder(1, 100, 100, LOB::Side::Buy, 0);
    book.addOrder(2, 101, 100, LOB::Side::Sell, 0);
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.getOBI());
    }
}
BENCHMARK(BM_GetOBI);

BENCHMARK_MAIN();
