#pragma once

#include "LOB/MemoryMappedFile.h"
#include "LOB/Types.h"
#include <vector>
#include <charconv>
#include <iostream>

namespace LOB {

struct RAWMessage {
    double timestamp;
    int type;
    uint64_t orderId;
    uint64_t size;
    int64_t price;
    int direction;
};

class LobsterMessageParser {
public:
    LobsterMessageParser(const std::string& filePath) : file_(filePath), current_(file_.data()), end_(file_.data() + file_.size()) {}

    bool hasNext() const {
        return current_ < end_;
    }

    // Hand-rolled parsing for speed (skipping charconv for now to ensure compatibility if <charconv> for double is missing in older MSYS2)
    // LOBSTER Format: Time, Type, OrderID, Size, Price, Direction
    // Example: 34200.004241176,1,16113575,18,5853300,1
    bool next(RAWMessage& msg) {
        if (current_ >= end_) return false;

        // Skip newlines if any
        while (current_ < end_ && (*current_ == '\n' || *current_ == '\r')) {
            current_++;
        }
        if (current_ >= end_) return false;

        // 1. Time (double)
        char* nextToken;
        msg.timestamp = std::strtod(current_, &nextToken);
        current_ = nextToken + 1; // Skip comma

        // 2. Type (int)
        msg.type = std::strtol(current_, &nextToken, 10);
        current_ = nextToken + 1;

        // 3. OrderID (u64)
        msg.orderId = std::strtoull(current_, &nextToken, 10);
        current_ = nextToken + 1;

        // 4. Size (u64)
        msg.size = std::strtoull(current_, &nextToken, 10);
        current_ = nextToken + 1;

        // 5. Price (i64) - LOBSTER prices are integers (shifted)
        msg.price = std::strtoll(current_, &nextToken, 10);
        current_ = nextToken + 1;

        // 6. Direction (int)
        msg.direction = std::strtol(current_, &nextToken, 10);
        current_ = nextToken; // Points to newline or end

        return true;
    }

private:
    MemoryMappedFile file_;
    const char*current_;
    const char* end_;
};

}
