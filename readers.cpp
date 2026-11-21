#include "desk.h"
namespace desk {
std::string readers(State& s, const std::string& action, const Words& a) {
    if (action == "reader") {
        const Member& m = memberById(s,a[0]);
        int open = 0, late = 0, holds = 0;
        for (const Loan& l : s.loans) {
            if (l.reader != m.id || l.closed) continue;
            ++open;
            if (l.due < s.day) ++late;
        }
        for (const auto& entry : s.books)
            for (const Hold& h : entry.second.holds) if (h.reader == m.id) ++holds;
        return memberLine(m) + "\nopen=" + std::to_string(open) + " overdue=" + std::to_string(late)
            + " reservations=" + std::to_string(holds);
    }
    if (action == "readers" || action == "find-reader") {
        std::string key = action == "readers" ? "" : lowerAscii(a[0]);
        if (action == "find-reader") validText(key);
        std::string out;
        for (const auto& entry : s.members) {
            const Member& m = entry.second;
            if (lowerAscii(m.id).find(key) == std::string::npos && lowerAscii(m.name).find(key) == std::string::npos)
                continue;
            if (!out.empty()) out += '\n';
            out += memberLine(m);
        }
        return out.empty() ? "No matching readers." : out;
    }
    validId(a[0]); validText(a[1]); validText(a[2]); Tier tier = parseTier(a[3]);
    if (action == "add-reader") {
        require(!s.members.count(a[0]), "reader ID already exists");
        s.members.emplace(a[0], Member{a[0],a[1],a[2],tier});
        return "Registered reader " + a[0];
    }
    Member& m = memberById(s,a[0]);
    m.name = a[1]; m.contact = a[2]; m.tier = tier;
    return "Updated reader " + m.id;
}
}
