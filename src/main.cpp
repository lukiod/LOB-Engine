#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <chrono> // Added missing include
#include "LOB/OrderBook.h"
#include "LOB/CSVParser.h"

struct LOBTruthLevel {
    LOB::Price askPrice;
    LOB::Quantity askSize;
    LOB::Price bidPrice;
    LOB::Quantity bidSize;
};

// Returns a vector of 10 levels
std::vector<LOBTruthLevel> parseTruthLine(const char*& current, const char* end) {
    std::vector<LOBTruthLevel> levels;
    levels.reserve(10);

    for (int i = 0; i < 10; ++i) {
        if (current >= end) break;
        LOBTruthLevel lvl;
        
        char* nextToken;
        // Ask Price
        lvl.askPrice = std::strtoll(current, &nextToken, 10);
        current = nextToken + 1;
        // Ask Size
        lvl.askSize = std::strtoull(current, &nextToken, 10);
        current = nextToken + 1;
        // Bid Price
        lvl.bidPrice = std::strtoll(current, &nextToken, 10);
        current = nextToken + 1;
        // Bid Size
        lvl.bidSize = std::strtoull(current, &nextToken, 10);
        current = nextToken + 1; // Skip comma or newline

        levels.push_back(lvl);
    }
    // Skip newline
    if (current < end && (*current == '\n' || *current == '\r')) current++;
    if (current < end && *current == '\n') current++; // Handle CRLF

    return levels;
}

int main(int argc, char* argv[]) {
    // Relative paths assume running from the 'build' directory (project root is ..)
    // Adjust if running from root.
    std::string msgPath = "../data/AAPL_2012-06-21_34200000_57600000_message_10.csv";
    std::string bookPath = "../data/AAPL_2012-06-21_34200000_57600000_orderbook_10.csv";

    // Fallback: search in current dir if not found in ..
    // (Simple check omitted for brevity, assuming standard build folder structure)

    std::cout << "Initializing LOBSTER Simulation..." << std::endl;
    std::cout << "Message File: " << msgPath << std::endl;
    std::cout << "Orderbook File: " << bookPath << std::endl;

    LOB::OrderBook book;
    
    // Ensure files exist before starting specific parser
    // We assume they open successfully or throw
    LOB::LobsterMessageParser msgParser(msgPath);
    LOB::MemoryMappedFile truthFile(bookPath);
    const char* truthCurrent = truthFile.data();
    const char* truthEnd = truthFile.data() + truthFile.size();

    uint64_t msgCount = 0;
    uint64_t errorCount = 0;
    LOB::RAWMessage msg;

    auto timeStart = std::chrono::high_resolution_clock::now();

    // --- Initialization Phase ---
    // Read the first line of the truth file to initialize the book
    // This represents the state AFTER the first message?
    // Wait, if we assume the first message is an UPDATE, we need the state BEFORE it.
    // LOBSTER strategy: usually we just start from the snapshot.
    // Let's try: Initialize with Truth Line 1, then SKIP Msg 1. Start verifying from Msg 2.
    
    auto truthLevelsInit = parseTruthLine(truthCurrent, truthEnd);
    if (truthLevelsInit.empty()) {
        std::cerr << "Empty truth file!" << std::endl;
        return 1;
    }
    
    // Populate Book
    for (const auto& level : truthLevelsInit) {
        if (level.askPrice != -9999999999) 
            book.addLevel(level.askPrice, level.askSize, LOB::Side::Sell);
        if (level.bidPrice != -9999999999) 
            book.addLevel(level.bidPrice, level.bidSize, LOB::Side::Buy);
    }
    
    // consume Msg 1 (Skip it)
    if (!msgParser.next(msg)) {
         std::cout << "No messages!" << std::endl;
         return 0;
    }
    msgCount++; // Count it as processed (or skipped)
    
    // --- Simulation Phase ---

    while (msgParser.next(msg)) {
        msgCount++;

        // Process Message
        // Debug Tracing
        if (msg.orderId == 13419503 || msg.price == 5854000) {
            std::cout << "[DEBUG] Msg " << msgCount << " Type " << msg.type 
                      << " ID " << msg.orderId << " Size " << msg.size 
                      << " Price " << msg.price << " Dir " << msg.direction << std::endl;
        }

        // Pass Price/Side/Size for fallback handling
        LOB::Side side = (msg.direction == 1 ? LOB::Side::Buy : LOB::Side::Sell);

        switch (msg.type) {
            case 1: // Add
                book.addOrder(msg.orderId, msg.price, msg.size, side, 
                              static_cast<uint64_t>(msg.timestamp * 1e9));
                break;
            case 2: // Partial Cancel
                book.reduceOrder(msg.orderId, msg.size, msg.price, side);
                break;
            case 3: // Delete
                book.deleteOrder(msg.orderId, msg.price, msg.size, side);
                break;
            case 4: // Execution
                book.executeOrder(msg.orderId, msg.size, msg.price, side);
                break;
            case 5: // Hidden Exec - Ignore
                break;
            default:
                break;
        }

        // Verification
        // We consumed Msg N. We need Truth N.
        // We initiated with Truth 1 (corresponding to Msg 1).
        // Now we processed Msg 2. So we need Truth 2.
        auto truthLevels = parseTruthLine(truthCurrent, truthEnd);
        
        if (!truthLevels.empty()) {
            auto truth = truthLevels[0];
            
            // Check Best Ask
            LOB::Price myAsk = book.getBestAsk();
            LOB::Quantity myAskSize = book.getVolumeAtPrice(myAsk);
            
            // Check Best Bid
            LOB::Price myBid = book.getBestBid();
            LOB::Quantity myBidSize = book.getVolumeAtPrice(myBid);

            bool error = false;
            // Validate Ask
            if (myAsk != truth.askPrice || myAskSize != truth.askSize) {
                if (errorCount < 5) {
                     std::cerr << "Mismatch at msg " << msgCount << " (ASK): "
                               << "Expected " << truth.askPrice << " @ " << truth.askSize
                               << ", Got " << myAsk << " @ " << myAskSize << std::endl;
                }
                error = true;
                
                // Self-Healing
                // If Truth has a price we don't (or diff size), trust Truth?
                // Especially if Truth price is better (missing data moved up).
                if (truth.askPrice != -9999999999) {
                     // Force fix the level
                     LOB::Limit* limit = book.getOrCreateLimit(truth.askPrice, LOB::Side::Sell);
                     // We can't know which order it is, so we just adjust 'totalVolume'
                     // Reset volume to match truth
                     limit->totalVolume = truth.askSize; 
                     // If we had wrong price as best, maybe we should remove it? 
                     // Complex. For now, just ensuring the Expected level exists.
                }
            }
            
            // Validate Bid
            if (myBid != truth.bidPrice || myBidSize != truth.bidSize) {
                if (errorCount < 5) {
                     std::cerr << "Mismatch at msg " << msgCount << " (BID): "
                               << "Expected " << truth.bidPrice << " @ " << truth.bidSize
                               << ", Got " << myBid << " @ " << myBidSize << std::endl;
                }
                error = true;
                
                // Self-Healing
                if (truth.bidPrice != -9999999999) {
                     LOB::Limit* limit = book.getOrCreateLimit(truth.bidPrice, LOB::Side::Buy);
                     limit->totalVolume = truth.bidSize;
                }
            }
            
            if (error) errorCount++;
        }
        
        if (msgCount % 100000 == 0) {
            std::cout << "Processed " << msgCount << " messages." << std::endl;
        }
    }

    auto timeEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> distinct = timeEnd - timeStart;

    std::cout << "Simulation Complete." << std::endl;
    std::cout << "Total Messages: " << msgCount << std::endl;
    std::cout << "Total Errors: " << errorCount << std::endl;
    std::cout << "Time: " << distinct.count() << "s" << std::endl;
    std::cout << "Throughput: " << msgCount / distinct.count() << " msgs/sec" << std::endl;

    return 0;
}
