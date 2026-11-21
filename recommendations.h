#ifndef DESK_RECOMMENDATIONS_H
#define DESK_RECOMMENDATIONS_H

#include "model.h"
#include <cstddef>

namespace desk {
struct Suggestion {
    std::string book;
    Words topics;
    bool peers;
    std::size_t score;
};

// Return up to three available titles ordered by shared subjects.
std::vector<Suggestion> availableAlternatives(const State& state, const std::string& bookId);
// Recommend up to three available titles the reader has not borrowed, using topics and peer borrowing.
std::vector<Suggestion> readerRecommendations(const State& state, const std::string& readerId);
}
#endif
