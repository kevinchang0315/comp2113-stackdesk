#include "desk.h"
namespace desk {
std::string catalog(State& s, const std::string& action, const Words& a) {
    if (action == "book") return bookLine(s,bookById(s,a[0]));
    if (action == "books" || action == "find-book") {
        std::string key = action == "books" ? "" : lowerAscii(a[0]);
        if (action == "find-book") validText(key);
        std::string out;
        for (const auto& item : s.books) {
            const Book& b = item.second;
            if (lowerAscii(b.id).find(key) == std::string::npos &&
                lowerAscii(b.title).find(key) == std::string::npos &&
                lowerAscii(b.author).find(key) == std::string::npos) continue;
            if (!out.empty()) out += '\n';
            out += bookLine(s,b);
        }
        return out.empty() ? "No matching books." : out;
    }
    validId(a[0]); validText(a[1]); validText(a[2]);
    int copies = number(a[3],1,9999);
    if (action == "add-book") {
        require(!s.books.count(a[0]), "book ID already exists");
        s.books.emplace(a[0], Book{a[0],a[1],a[2],copies,{}});
        return "Added book " + a[0];
    }
    Book& b = bookById(s,a[0]);
    int out = activeCopies(s,b.id);
    require(copies >= out + readyCount(b), "copy count would break an active loan or pickup promise");
    b.title = a[1]; b.author = a[2]; b.copies = copies;
    offerCopies(b,out,s.day);
    return "Updated book " + b.id;
}
}
