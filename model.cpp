#include "model.h"
#include <algorithm>
#include <cctype>
namespace desk {
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void validId(const std::string& id) {
    require(!id.empty() && id.size() <= 24, "ID must be 1-24 ASCII letters, digits, hyphens or underscores");
    for (char c : id)
        require((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || c == '-' || c == '_', "invalid character in ID");
}
void validText(const std::string& value) {
    require(value.find_first_not_of(' ') != std::string::npos && value.size() <= 160,
            "text field must contain 1-160 bytes and a non-space character");
    for (unsigned char c : value) require(c >= 32 && c != 127, "control character in text");
}
int number(const std::string& s, int low, int high) {
    require(!s.empty(), "expected a nonnegative integer");
    int result = 0;
    for (char c : s) {
        require(c >= '0' && c <= '9', "expected a nonnegative integer");
        int digit = c - '0';
        require(result < high / 10 || (result == high / 10 && digit <= high % 10),
                "integer out of range");
        result = result * 10 + digit;
    }
    require(result >= low, "integer out of range");
    return result;
}
std::string lowerAscii(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}
Book& bookById(State& s, const std::string& id) {
    auto it = s.books.find(id); require(it != s.books.end(), "unknown book " + id); return it->second;
}
Member& memberById(State& s, const std::string& id) {
    auto it = s.members.find(id); require(it != s.members.end(), "unknown reader " + id); return it->second;
}
int activeCopies(const State& s, const std::string& id) {
    int n = 0; for (const Loan& l : s.loans) if (!l.closed && l.book == id) ++n;
    return n;
}
int readyCount(const Book& b) {
    return static_cast<int>(std::count_if(b.holds.begin(), b.holds.end(), [](const Hold& h){return h.ready;}));
}
int available(const State& s, const Book& b) { return b.copies - activeCopies(s,b.id) - readyCount(b); }
std::string bookLine(const State& s, const Book& b) {
    return b.id + " | " + b.title + " | " + b.author + " | copies=" + std::to_string(b.copies)
        + " available=" + std::to_string(available(s,b)) + " ready=" + std::to_string(readyCount(b));
}
std::string memberLine(const Member& m) {
    return m.id + " | " + m.name + " | " + tierName(m.tier) + " | " + m.contact;
}
namespace { bool space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; } }
Words split(const std::string& line) {
    Words fields;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && space(line[i])) ++i;
        if (i == line.size()) break;
        std::string word;
        if (line[i] == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\') {
                    ++i; require(i < line.size() && (line[i] == '\\' || line[i] == '"'), "invalid quoted escape");
                }
                word += line[i++];
            }
            require(i < line.size(), "unterminated quoted field"); ++i;
            require(i == line.size() || space(line[i]), "expected space after quote");
        } else {
            while (i < line.size() && !space(line[i])) {
                require(line[i] != '"', "quote the entire field"); word += line[i++];
            }
        }
        fields.push_back(word);
    }
    return fields;
}
std::string quote(const std::string& value) {
    std::string out(1,'"');
    for (char c : value) { if (c == '"' || c == '\\') out += '\\'; out += c; }
    return out + '"';
}
}
