#include "web_view.h"
#include "recommendations.h"
#include <algorithm>
#include <initializer_list>
#include <sstream>

namespace desk {
namespace {
std::string json(const std::string& value) {
    const char hex[] = "0123456789abcdef";
    std::string out(1,'"');
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 32) {
            out += "\\u00";
            out += hex[c / 16];
            out += hex[c % 16];
        } else {
            out += static_cast<char>(c);
        }
    }
    out += '"';
    return out;
}

template<class Items, class Write>
void array(std::ostream& out, const Items& items, Write write) {
    out << '[';
    bool first = true;
    for (const auto& item : items) {
        if (!first) out << ',';
        first = false;
        write(item);
    }
    out << ']';
}

bool wordByte(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '#' || c >= 128;
}

Words tokens(const std::string& text) {
    Words words;
    std::string word;
    for (unsigned char c : lowerAscii(text)) {
        if (wordByte(c)) word += static_cast<char>(c);
        else if (!word.empty()) {
            words.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) words.push_back(word);
    return words;
}

// Check a single insertion, deletion, substitution, or adjacent swap, with no Unicode case folding.
// Two-letter terms and IDs still use exact/prefix matching to avoid spurious corrections.
bool oneEdit(const std::string& a, const std::string& b) {
    if (a.size() > b.size()) return oneEdit(b,a);
    if (b.size() - a.size() > 1) return false;
    std::size_t at = 0;
    while (at < a.size() && a[at] == b[at]) ++at;
    if (a.size() + 1 == b.size()) return a.compare(at,std::string::npos,b,at+1,std::string::npos) == 0;
    if (at == a.size()) return false;
    if (a.compare(at+1,std::string::npos,b,at+1,std::string::npos) == 0) return true;
    return at + 1 < a.size() && a[at] == b[at+1] && a[at+1] == b[at] &&
        a.compare(at+2,std::string::npos,b,at+2,std::string::npos) == 0;
}

bool letters(const std::string& word) {
    return std::all_of(word.begin(),word.end(),[](char c){return c >= 'a' && c <= 'z';});
}

struct Score { int points = 0; bool fuzzy = false; };
void consider(Score& best, const std::string& query, const Words& field, int weight, bool fuzzy) {
    for (const std::string& word : field) {
        Score match;
        if (word == query) match.points = 100 + weight;
        else if (query.size() >= 2 && word.compare(0,query.size(),query) == 0) match.points = 65 + weight;
        else if (query.size() >= 3 && word.find(query) != std::string::npos) match.points = 45 + weight;
        else if (fuzzy && query.size() >= 4 && word.size() >= 4 && letters(query) && letters(word) && oneEdit(query,word))
            match = Score{20 + weight,true};
        if (match.points > best.points) best = match;
    }
}
}

std::string webSnapshot(const State& state, const Policies& policies) {
    // Count open copies in one pass, independent of the size of the catalog.
    std::map<std::string,int> loanCounts;
    for (const Loan& loan : state.loans) if (!loan.closed) ++loanCounts[loan.book];

    std::ostringstream out;
    out << "{\"day\":" << json(state.day.str()) << ",\"books\":";
    array(out,state.books,[&](const auto& row) {
        const Book& book = row.second;
        const int borrowed = loanCounts[book.id];
        const int ready = readyCount(book);
        out << "{\"id\":" << json(book.id) << ",\"title\":" << json(book.title) << ",\"author\":" << json(book.author)
            << ",\"copies\":" << book.copies << ",\"available\":" << (book.copies-borrowed-ready)
            << ",\"borrowed\":" << borrowed << ",\"ready\":" << ready << ",\"topics\":";
        array(out,book.topics,[&](const std::string& topic){ out << json(topic); });
        out << ",\"holds\":";
        array(out,book.holds,[&](const Hold& hold) {
            out << "{\"reader\":" << json(hold.reader) << ",\"readyThrough\":"
                << (hold.ready ? json(hold.offered.plus(2).str()) : "null") << '}';
        });
        out << '}';
    });
    out << ",\"readers\":";
    array(out,state.members,[&](const auto& row) {
        const Member& reader = row.second;
        out << "{\"id\":" << json(reader.id) << ",\"name\":" << json(reader.name)
            << ",\"contact\":" << json(reader.contact) << ",\"tier\":" << json(tierName(reader.tier)) << '}';
    });
    out << ",\"loans\":";
    array(out,state.loans,[&](const Loan& loan) {
        const int daysLate = loan.closed ? 0 : std::max(0,state.day-loan.due);
        out << "{\"book\":" << json(loan.book) << ",\"reader\":" << json(loan.reader)
            << ",\"tier\":" << json(tierName(loan.tier)) << ",\"issued\":" << json(loan.issued.str())
            << ",\"due\":" << json(loan.due.str()) << ",\"renewals\":" << loan.renewals
            << ",\"returned\":" << (loan.closed ? json(loan.returned.str()) : "null")
            << ",\"daysLate\":" << daysLate << '}';
    });
    out << ",\"policies\":";
    array(out,std::initializer_list<Tier>{Tier::Student,Tier::Faculty},[&](Tier tier) {
        const LoanPolicy& p = policies.get(tier);
        out << "{\"tier\":" << json(tierName(tier)) << ",\"days\":" << p.days()
            << ",\"maxLoans\":" << p.maxLoans() << ",\"maxRenewals\":" << p.maxRenewals() << '}';
    });
    out << '}';
    return out.str();
}

std::string searchCatalog(const State& state, const std::string& query) {
    if (!query.empty()) validText(query);
    const Words words = tokens(query);
    const std::string key = lowerAscii(query);
    struct Match { const Book* book; int score; bool fuzzy; };
    std::vector<Match> matches;
    for (const auto& row : state.books) {
        const Book& book = row.second;
        if (lowerAscii(book.id) == key || lowerAscii(book.title) == key) {
            matches.push_back(Match{&book,20000,false});
            continue;
        }
        if (words.empty() && !query.empty()) continue;

        const Words title = tokens(book.title);
        const Words author = tokens(book.author);
        const Words id = tokens(book.id);
        const Words topics = tokens(topicList(book.topics));
        int points = 0;
        bool fuzzy = false;
        bool matched = true;
        for (const std::string& word : words) {
            Score best;
            consider(best,word,title,12,true);
            consider(best,word,author,4,true);
            consider(best,word,topics,7,false);
            consider(best,word,id,0,false);
            if (!best.points) { matched = false; break; }
            points += best.points;
            fuzzy = fuzzy || best.fuzzy;
        }
        if (matched) matches.push_back(Match{&book,points,fuzzy});
    }
    std::sort(matches.begin(),matches.end(),[](const Match& a, const Match& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.book->id < b.book->id;
    });
    std::ostringstream out;
    array(out,matches,[&](const Match& match) {
        out << "{\"id\":" << json(match.book->id) << ",\"fuzzy\":" << (match.fuzzy ? "true" : "false") << '}';
    });
    return out.str();
}

std::string webRecommendations(const State& state, const std::string& id, bool byReader) {
    const auto suggestions = byReader ? readerRecommendations(state,id) : availableAlternatives(state,id);
    std::ostringstream out;
    array(out,suggestions,[&](const Suggestion& suggestion) {
        const Book& book = state.books.at(suggestion.book);
        out << "{\"id\":" << json(book.id) << ",\"title\":" << json(book.title)
            << ",\"author\":" << json(book.author) << ",\"available\":" << available(state,book)
            << ",\"similarReaders\":" << (suggestion.peers ? "true" : "false") << ",\"topics\":";
        array(out,suggestion.topics,[&](const std::string& topic){ out << json(topic); });
        out << '}';
    });
    return out.str();
}
}
