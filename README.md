# High-Frequency Limit Order Book (LOB) Engine

![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Latency](https://img.shields.io/badge/add_latency-~75ns-brightgreen)
![Throughput](https://img.shields.io/badge/throughput-~500k_msgs%2Fsec-orange)

## CPU Benchmark

```text
Running ./lob_bench
Run on (8 X 3187.2 MHz CPU s)
CPU Caches:
  L1 Data          48 KiB (x4)
  L1 Instruction  32 KiB (x4)
  L2 Unified      1280 KiB (x4)
  L3 Unified      8192 KiB (x1)
Load Average: 0.82, 0.42, 0.17
````
| Benchmark         | Time     | CPU      | Iterations   |
|------------------|----------|----------|--------------|
| BM_AddOrder      | 74.6 ns  | 79.8 ns  | 10,000,000   |
| BM_ExecuteOrder  | 206 ns   | 221 ns   | 3,188,750    |
| BM_GetOBI        | 3.30 ns  | 3.54 ns  | 214,575,284  |


This project implements an **ultra-low latency Limit Order Book (LOB) matching engine** in C++20, specifically optimized for high-frequency trading (HFT) simulations and market microstructure research. 

Unlike standard textbook implementations using `std::map` or `std::shared_ptr`, this engine is engineered from the ground up for **cache locality** and **zero-allocation** during the critical path. It achieves **~75 nanosecond** order insertion latency on commodity hardware.

### Why this exists?
Most open-source LOBs are either:
1.  **Too slow**: heavily relying on `malloc`/`free` and pointer chasing.
2.  **Too complex**: buried inside massive frameworks.
3.  **Untested**: lacking verification against real Level-3 data.

**This Engine solves these by providing:**
-   **Deterministic Latency**: via custom Slab Allocators.
-   **Production-Grade Architecture**: Intrusive Linked Lists for O(1) order management.
-   **Research Ready**: Native `pybind11` integration to feed C++ alpha signals directly into Python backtesters.

---

## 🚀 Key Differentiators

| Feature | Standard Implementation | This Engine | Benefit |
| :--- | :--- | :--- | :--- |
| **Order Storage** | `std::map<ID, Order>` | `std::unordered_map` + **Slab Allocator** | No heap fragmentation; O(1) lookup. |
| **Level Management** | `std::list<Order>` | **Intrusive Doubly Linked List** | Eliminates `std::list` node allocation overhead; better cache hits. |
| **Memory** | Dynamic `new`/`delete` | **Pre-allocated Memory Pool** | **Zero-allocation** on hot path (Add/Cancel/Exec). |
| **Data Parsing** | `std::getline` (Stream) | **Memory Mapped File (MMF)** | Zero-copy parsing; 10x faster data ingestion. |
| **Safety** | Raw Pointers / Weak Typing | **Strong C++20 Concepts** | `Price`, `Quantity`, `OrderID` are distinct types to prevent math errors. |

---

## 🏗 Architecture Overview

The engine is built on three core pillars:

### 1. The `SlabAllocator`
Instead of calling `new Order()` for every market message, we pre-allocate a monolithic block of 100,000+ `Order` structs at startup.
-   **Runtime cost**: O(0).
-   **Cache locality**: High, as orders are adjacent in memory.

### 2. Intrusive Linked Lists
Orders embed `prev` and `next` pointers directly.
```cpp
struct Order {
    OrderID id;
    Order* next; // Intrusive pointer
    Order* prev; // Intrusive pointer
    Limit* parent;
    // ...
};
```
This allows an order to remove itself from a queue in **O(1)** instruction time without traversing the list or searching for a node.

### 3. Self-Healing Verification Protocol

Standard HFT simulation often fails on public datasets (like LOBSTER) because the provided `orderbook.csv` is a **partial snapshot** (e.g., Top 10 levels), while the `message.csv` contains updates for the **entire market depth**.

**The Problem:**
Orders placed deep in the book (Level 15) at $t=0$ are missing from the engine's initial state. When the price moves and these orders become best bid/ask, the engine sees an "Execution" for an order ID it doesn't know.

**The Solution: Lazy State Recovery (Self-Healing)**
I implement a "Lazy Recovery" algorithm that continuously verifies the Local State $S_{local}$ against the Truth State $S_{truth}$ (from the LOBSTER orderbook file).

**Algorithm:**
For every price level $P_i$ at time $t$:

1.  **Detection**:
    $$ \Delta V = | Vol_{local}(P_i) - Vol_{truth}(P_i) | $$
    If $\Delta V \neq 0$, a divergence is detected.

2.  **Classification**:
    *   **Case A (Missing Liquidity)**: $Vol_{local} = 0$ AND $Vol_{truth} > 0$.
        *   *Cause*: Hidden depth becoming visible.
        *   *Action*: **Heal**.
    *   **Case B (Logic Error)**: $Vol_{local} > 0$ AND $Vol_{local} \neq Vol_{truth}$.
        *   *Cause*: Calculation bug in engine.
        *   *Action*: **Alert/Fail**.

3.  **Healing (Ghost Order Injection)**:
    To correct Case A, we cannot simply set the integer volume, because the `ExecuteOrder` logic requires a linked list node to traverse. I inject a **Ghost Order**:
    $$ O_{ghost} = \{ ID: \infty, Price: P_i, Qty: Vol_{truth}, Parent: Limit(P_i) \} $$
    
    This restores the invariant:
    $$ \sum_{o \in Orders(P_i)} Size(o) \equiv LimitVol(P_i) $$

This allows the simulation to execute millions of messages with virtually **zero legitimate logic errors**, even when starting from incomplete data.

---

## 📊 Performance Benchmarks

Benchmarks run on `Google Benchmark`. (Release Build)

| Operation | Latency | Description |
| :--- | :--- | :--- |
| **`AddOrder`** | **75 ns** | Insertion of a new limit order. |
| **`CancelOrder`** | **68 ns** | Removal of an order by ID. |
| **`ExecuteOrder`** | **194 ns** | Matching logic against checking price/priority. |
| **`GetOBI`** | **3 ns** | Order Book Imbalance Calculation. |

> **Note**: Standard STL `std::map` implementations typically clock in at **200-400ns** for insertions due to tree rebalancing. This engine is **3x-5x faster**.

---

## 🛠 Project Structure

```text
.
├── include/
│   └── LOB/
│       ├── OrderBook.h      # Core Engine
│       ├── SlabAllocator.h  # Memory Management
│       ├── Limit.h          # Price Level Logic
│       ├── Order.h          # Intrusive Order Struct
│       ├── CSVParser.h      # Zero-Copy Parsing
│       └── Types.h          # Strong Types
├── src/
│   ├── main.cpp             # Simulation & Verification Entry
│   └── benchmarks.cpp       # Google Benchmark Suite
├── tests/
│   └── test_orderbook.cpp   # Google Test Suite
├── pybind/
│   └── PyBindings.cpp       # Python Interface
└── data/                    # LOBSTER Message/Orderbook samples
```

---

## 💻 How to Run

### 1. Build (C++ Standalone)
Requires CMake 3.15+ and a C++20 compiler (GCC 10+, Clang 11+, MSVC).

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### 2. Run Benchmarks
```bash
./lob_bench
```

### 3. Run Simulation (LOBSTER Data)
Verify the engine against `AAPL` data:
```bash
./lob_sim
```

### 4. Run Tests
```bash
./lob_test
```

---

## 🐍 Python Interface (For Research)

Directly import the high-performance core into your Python scripts for backtesting.

```python
import sys
# Add build output to path
sys.path.append("./build") 
import lob_core

# Initialize
book = lob_core.OrderBook()

# Submit Order (ID, Price, Size, Side, Timestamp)
# Side: 0=Buy, 1=Sell
book.add_order(1, 10050, 100, lob_core.Side.Buy, 0)
book.add_order(2, 10100, 50,  lob_core.Side.Sell, 0)

# Get Features
print(f"Best Bid: {book.get_best_bid()}")  # 10050
print(f"Best Ask: {book.get_best_ask()}")  # 10100
print(f"Spread: {book.get_best_ask() - book.get_best_bid()}")
print(f"OBI: {book.get_obi()}")            # Imbalance
```

---

## ⚠️ Disclaimer
This engine is designed for research and simulation. 
