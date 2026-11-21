#include "desk.h"
#include <algorithm>
namespace desk {
void offerCopies(Book& b, int onLoan, Date day) {
    int free = b.copies - onLoan - readyCount(b);
    for (Hold& h : b.holds) {
        if (free == 0) break;
        if (h.ready) continue;
        day.plus(3); // Ensure the exclusive expiry date can be represented.
        h.offered = day; h.ready = true; --free;
    }
}
int advanceDay(State& s, Date target) {
    require(!(target < s.day), "business date cannot move backwards");
    std::map<std::string,int> active;
    for (const Loan& l : s.loans) if (!l.closed) ++active[l.book];
    int expired = 0;
    for (auto& item : s.books) {
        Book& b = item.second;
        // Ready holds form the queue's prefix in chronological offer order.
        while (!b.holds.empty() && b.holds.front().ready) {
            Date expiry = b.holds.front().offered.plus(3);
            if (target < expiry) break;
            do {
                b.holds.pop_front(); ++expired;
            } while (!b.holds.empty() && b.holds.front().ready && b.holds.front().offered.plus(3) == expiry);
            offerCopies(b,active[b.id],expiry);
        }
    }
    s.day = target;
    return expired;
}
std::string reservations(State& s, const std::string& action, const Words& a) {
    Book& b = bookById(s,a[0]);
    if (action == "queue") {
        if (b.holds.empty()) return "Queue empty.";
        std::string out;
        int position = 0;
        for (const Hold& h : b.holds) {
            if (!out.empty()) out += '\n';
            out += std::to_string(++position) + " " + h.reader + " ";
            out += h.ready ? "READY through " + h.offered.plus(2).str() : "WAITING";
        }
        return out;
    }
    const Member& m = memberById(s,a[1]);
    auto here = std::find_if(b.holds.begin(),b.holds.end(),[&](const Hold& h){return h.reader == m.id;});
    if (action == "cancel-hold") {
        require(here != b.holds.end(), "no reservation for this reader");
        b.holds.erase(here);
        offerCopies(b,activeCopies(s,b.id),s.day);
        return "Cancelled reservation " + b.id + " " + m.id;
    }
    require(here == b.holds.end(), "reader already in queue");
    for (const Loan& l : s.loans)
        require(l.closed || l.book != b.id || l.reader != m.id, "reader already has this title");
    require(available(s,b) == 0, "copy available; borrow it at the desk");
    b.holds.push_back(Hold{m.id,false,Date()});
    return "Reserved " + b.id + " for " + m.id + " position=" + std::to_string(b.holds.size());
}
}
