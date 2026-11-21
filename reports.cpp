#include "desk.h"
#include <algorithm>
#include <sstream>
#include <tuple>
namespace desk {
namespace {
// Both views use this order so staff see the same priority on screen and in the export.
std::vector<const Loan*> lateLoans(const State& s) {
    std::vector<const Loan*> rows;
    for (const Loan& l : s.loans)
        if (!l.closed && l.due < s.day) rows.push_back(&l);
    std::sort(rows.begin(),rows.end(),[](const Loan* a, const Loan* b) {
        return std::tie(a->due,a->book,a->reader) < std::tie(b->due,b->book,b->reader);
    });
    return rows;
}
std::string csvCell(const std::string& value) {
    std::string out(1,'"');
    std::size_t first = value.find_first_not_of(' ');
    if (first != std::string::npos && std::string("=+-@").find(value[first]) != std::string::npos)
        out += '\'';
    for (char c : value) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + '"';
}
}
std::string reports(const State& s, const std::string& action) {
    std::ostringstream out;
    if (action == "overdue") {
        const auto rows = lateLoans(s);
        if (rows.empty()) return "No overdue loans.";
        out << "Overdue on " << s.day.str();
        for (const Loan* l : rows) {
            const int days = s.day - l->due;
            out << '\n' << days << (days == 1 ? " day | " : " days | ") << l->book
                << ' ' << s.books.at(l->book).title << " | " << l->reader << ' '
                << s.members.at(l->reader).name << " | due=" << l->due.str();
        }
        return out.str();
    }
    long long copies = 0, ready = 0, waiting = 0, open = 0, overdue = 0, completed = 0;
    std::map<std::string, int> checkouts;
    for (const auto& item : s.books) {
        int readyForBook = readyCount(item.second);
        copies += item.second.copies;
        ready += readyForBook;
        waiting += static_cast<long long>(item.second.holds.size()) - readyForBook;
    }
    for (const Loan& l : s.loans) {
        ++checkouts[l.book];
        if (l.closed) ++completed;
        else {
            ++open;
            if (l.due < s.day) ++overdue;
        }
    }
    std::vector<std::pair<std::string,int>> top(checkouts.begin(),checkouts.end());
    std::sort(top.begin(),top.end(),[](const std::pair<std::string,int>& a, const std::pair<std::string,int>& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    out << "Stock on " << s.day.str() << "\nTitles=" << s.books.size() << " Readers=" << s.members.size()
        << "\nCopies=" << copies << " Available=" << (copies-open-ready) << " On loan=" << open
        << " Ready=" << ready << " Waiting=" << waiting
        << "\nOverdue=" << overdue << " Completed loans=" << completed << "\nTop titles";
    if (top.empty()) out << ": none";
    for (std::size_t i = 0; i < top.size() && i < 3; ++i)
        out << " | " << top[i].first << '=' << top[i].second;
    return out.str();
}
std::string overdueCsv(const State& s) {
    std::ostringstream out;
    out << "days_late,book_id,title,reader_id,reader_name,contact,due_date\r\n";
    for (const Loan* l : lateLoans(s)) {
        const Member& m = s.members.at(l->reader);
        out << (s.day-l->due) << ',' << csvCell(l->book) << ',' << csvCell(s.books.at(l->book).title)
            << ',' << csvCell(l->reader) << ',' << csvCell(m.name) << ',' << csvCell(m.contact)
            << ',' << l->due.str() << "\r\n";
    }
    return out.str();
}
}
