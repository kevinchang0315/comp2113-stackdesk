#include "recommendations.h"
#include <algorithm>
#include <set>

namespace desk {
namespace {
using TitleSet = std::set<std::string>;
using Counts = std::map<std::string,std::size_t>;

std::vector<Suggestion> topThree(std::vector<Suggestion> choices) {
    std::sort(choices.begin(),choices.end(),[](const Suggestion& left, const Suggestion& right) {
        if (left.score != right.score) return left.score > right.score;
        return left.book < right.book;
    });
    if (choices.size() > 3) choices.resize(3);
    return choices;
}

Counts onLoan(const State& state) {
    Counts open;
    for (const Loan& loan : state.loans)
        if (!loan.closed) ++open[loan.book];
    return open;
}

bool canBorrow(const Book& book, const Counts& open) {
    const auto found = open.find(book.id);
    const std::size_t borrowed = found == open.end() ? 0 : found->second;
    return static_cast<std::size_t>(book.copies) >
           borrowed + static_cast<std::size_t>(readyCount(book));
}
}

std::vector<Suggestion> availableAlternatives(const State& state, const std::string& bookId) {
    const auto source = state.books.find(bookId);
    require(source != state.books.end(),"unknown book " + bookId);
    const Counts open = onLoan(state);
    std::vector<Suggestion> choices;
    for (const auto& entry : state.books) {
        const Book& book = entry.second;
        if (book.id == bookId || !canBorrow(book,open)) continue;

        Words common;
        for (const std::string& topic : book.topics) {
            const Words& sourceTopics = source->second.topics;
            if (std::find(sourceTopics.begin(),sourceTopics.end(),topic) != sourceTopics.end())
                common.push_back(topic);
        }
        if (!common.empty()) choices.push_back({book.id,common,false,common.size()});
    }
    return topThree(choices);
}

std::vector<Suggestion> readerRecommendations(const State& state, const std::string& readerId) {
    require(state.members.count(readerId) != 0,"unknown reader " + readerId);
    std::map<std::string,TitleSet> history;
    for (const Loan& loan : state.loans) history[loan.reader].insert(loan.book);
    const TitleSet& own = history[readerId];
    if (own.empty()) return {};

    Counts interests;
    for (const std::string& title : own)
        for (const std::string& topic : state.books.at(title).topics) ++interests[topic];

    Counts votes;
    for (const auto& entry : history) {
        if (entry.first == readerId) continue;
        const TitleSet& titles = entry.second;
        const bool overlap = std::any_of(titles.begin(),titles.end(),
                                         [&](const std::string& title){ return own.count(title) != 0; });
        if (!overlap) continue;
        for (const std::string& title : titles)
            if (!own.count(title)) ++votes[title];
    }

    const Counts open = onLoan(state);
    std::vector<Suggestion> choices;
    for (const auto& entry : state.books) {
        const Book& book = entry.second;
        if (own.count(book.id) || !canBorrow(book,open)) continue;

        const std::size_t support = votes[book.id];
        const bool peers = support >= 2;
        std::size_t score = peers ? 3*support : 0;
        Words common;
        for (const std::string& topic : book.topics) {
            const auto interest = interests.find(topic);
            if (interest != interests.end()) {
                score += interest->second;
                common.push_back(topic);
            }
        }
        if (score > 0) choices.push_back({book.id,common,peers,score});
    }
    return topThree(choices);
}
}
