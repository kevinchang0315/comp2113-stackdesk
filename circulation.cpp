#include "desk.h"
#include <algorithm>
namespace desk {
namespace {
bool hasOverdue(const State& s, const std::string& reader) {
    for (const Loan& l : s.loans) if (!l.closed && l.reader == reader && l.due < s.day) return true;
    return false;
}
std::string history(State& s, const std::string& reader) {
    memberById(s,reader);
    std::string out;
    for (const Loan& l : s.loans) {
        if (l.reader != reader) continue;
        if (!out.empty()) out += '\n';
        out += l.book + " | issued=" + l.issued.str() + " due=" + l.due.str() +
            " renewals=" + std::to_string(l.renewals) + " | ";
        out += l.closed ? "RETURNED " + l.returned.str() : (l.due < s.day ? "OVERDUE" : "OPEN");
    }
    return out.empty() ? "No loan history." : out;
}
}
std::string circulation(State& s, const Policies& rules, const std::string& action, const Words& a) {
    if (action == "policy") {
        Tier tier = parseTier(a[0]);
        const LoanPolicy& p = rules.get(tier);
        return tierName(tier) + " | days=" + std::to_string(p.days()) + " limit=" + std::to_string(p.maxLoans())
            + " renewals=" + std::to_string(p.maxRenewals());
    }
    if (action == "history") return history(s,a[0]);
    Book& b = bookById(s,a[0]);
    const Member& m = memberById(s,a[1]);
    auto current = std::find_if(s.loans.begin(),s.loans.end(),[&](const Loan& l){
        return !l.closed && l.book == b.id && l.reader == m.id;
    });
    if (action == "borrow") {
        require(current == s.loans.end(), "reader already has this title");
        require(!hasOverdue(s,m.id), "return overdue loans first");
        const LoanPolicy& p = rules.get(m.tier);
        auto count = std::count_if(s.loans.begin(),s.loans.end(),[&](const Loan& l){return !l.closed && l.reader == m.id;});
        require(count < p.maxLoans(), "reader loan limit reached");
        auto hold = std::find_if(b.holds.begin(),b.holds.end(),[&](const Hold& h){return h.reader == m.id;});
        if (hold == b.holds.end()) require(available(s,b) > 0, "no unreserved copy available");
        else {
            require(hold->ready, "reader is still waiting");
            b.holds.erase(hold);
        }
        Date due = s.day.plus(p.days());
        s.loans.push_back(Loan{b.id,m.id,m.tier,s.day,due,0,false,Date()});
        return "Borrowed " + b.id + " by " + m.id + " due=" + due.str();
    }
    require(current != s.loans.end(), "no open loan for this book and reader");
    if (action == "return") {
        current->closed = true; current->returned = s.day;
        offerCopies(b,activeCopies(s,b.id),s.day);
        return "Returned " + b.id + " by " + m.id;
    }
    require(!hasOverdue(s,m.id), "return overdue loans first");
    require(b.holds.empty(), "another reader has reserved this title");
    const LoanPolicy& p = rules.get(current->tier);
    require(current->renewals < p.maxRenewals(), "renewal limit reached");
    current->due = current->due.plus(p.days()); ++current->renewals;
    return "Renewed " + b.id + " for " + m.id + " due=" + current->due.str();
}
}
