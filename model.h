#ifndef DESK_MODEL_H
#define DESK_MODEL_H
#include "date.h"
#include "policy.h"
#include <deque>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace desk {
using Words = std::vector<std::string>;
// Ready offers form a prefix of each FIFO queue. Waiting entries have no offer date.
struct Hold { std::string reader; bool ready; Date offered; };
struct Book { std::string id, title, author; int copies; std::deque<Hold> holds; Words topics; };
struct Member { std::string id, name, contact; Tier tier; };
// Keep the issue tier with the loan so reader edits do not change existing terms.
struct Loan {
    std::string book, reader; Tier tier;
    Date issued, due; int renewals; bool closed; Date returned;
};
struct State {
    Date day;
    std::map<std::string, Book> books;
    std::map<std::string, Member> members;
    std::vector<Loan> loans;
};
// Throw a human-readable error on failure. The caller discards the draft transaction.
void require(bool condition, const std::string& message);
void validId(const std::string& id);
void validText(const std::string& field);
int number(const std::string& digits, int low, int high);
std::string lowerAscii(std::string s);
// Normalize a comma-separated topic list. An empty list removes the book's topics.
Words parseTopics(const std::string& text);
std::string topicList(const Words& topics);
Book& bookById(State& s, const std::string& id);
Member& memberById(State& s, const std::string& id);
int activeCopies(const State& s, const std::string& book);
int readyCount(const Book& book);
int available(const State& s, const Book& book);
std::string bookLine(const State& s, const Book& book);
std::string memberLine(const Member& member);
// Batch commands and on-disk records share one quoted-field grammar.
Words split(const std::string& line);
std::string quote(const std::string& field);
}
#endif
