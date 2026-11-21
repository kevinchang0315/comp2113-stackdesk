#include "storage.h"
#include <fstream>
#include <set>
#include <utility>
namespace desk {
namespace {
fs::path normalPath(const fs::path& p) {
    require(p.has_filename() && p.filename() != "." && p.filename() != "..", "expected a file path");
    return fs::canonical(fs::absolute(p).parent_path()) / p.filename();
}
fs::path databasePath(const fs::path& p) {
    require(p.extension() == ".db","database filename must end in .db");
    return normalPath(p);
}
bool regularOrMissing(const fs::path& p) {
    fs::file_status status = fs::symlink_status(p);
    require(!fs::exists(status) || fs::is_regular_file(status), "expected a regular file at " + p.string());
    return fs::is_regular_file(status);
}
void writeFile(const fs::path& p, const std::string& contents) {
    std::ofstream out(p,std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out),"cannot open " + p.string());
    out << contents;
    out.close();
    require(static_cast<bool>(out),"could not finish writing " + p.string());
}
void validate(const State& s) {
    Policies rules;
    using Key = std::pair<std::string,std::string>;
    std::map<Key,const Loan*> last;
    std::map<std::string,int> active;
    Date previousIssue;
    for (const Loan& l : s.loans) {
        require(s.books.count(l.book) && s.members.count(l.reader),"loan references unknown record");
        require(!(s.day < l.issued) && !(l.issued < previousIssue),"invalid loan issue date or order");
        previousIssue = l.issued;
        if (l.closed)
            require(!(l.returned < l.issued) && !(s.day < l.returned),"invalid loan return date");
        const LoanPolicy& p = rules.get(l.tier);
        require(l.renewals <= p.maxRenewals() && l.issued.plus(p.days()*(1+l.renewals)) == l.due,
                "loan date or renewal count contradicts its policy");
        Key k(l.book,l.reader);
        auto previous = last.find(k);
        if (previous != last.end()) {
            require(previous->second->closed && !(l.issued < previous->second->returned),
                    "overlapping loans for one reader and title");
        }
        last[k] = &l;
        if (!l.closed) ++active[l.book];
    }

    for (const auto& entry : s.books) {
        const Book& b = entry.second;
        std::set<std::string> readers;
        bool waitingSeen = false;
        Date previousOffer;
        int ready = 0;
        for (const Hold& h : b.holds) {
            require(s.members.count(h.reader) && readers.insert(h.reader).second,
                    "unknown or duplicate reservation reader");
            auto l = last.find(Key(b.id,h.reader));
            require(l == last.end() || l->second->closed,"reader has a loan and reservation for one title");
            if (h.ready) {
                require(!waitingSeen && !(h.offered < previousOffer) && !(s.day < h.offered)
                        && s.day < h.offered.plus(3),"invalid pickup queue or offer date");
                previousOffer = h.offered;
                ++ready;
            } else waitingSeen = true;
        }
        int free = b.copies - active[b.id] - ready;
        require(free >= 0 && (!waitingSeen || free == 0),"stock contradicts loans or reservations");
    }
}
void readRow(State& s, const Words& w) {
    require(!w.empty(),"empty record");
    if (w[0] == "BOOK") {
        require(w.size() == 5,"bad book record length");
        validId(w[1]); validText(w[2]); validText(w[3]);
        Book b{w[1],w[2],w[3],number(w[4],1,9999),{}};
        require(s.books.emplace(w[1],std::move(b)).second,"duplicate book");
    } else if (w[0] == "READER") {
        require(w.size() == 5,"bad reader record length");
        validId(w[1]); validText(w[2]); validText(w[3]);
        Member m{w[1],w[2],w[3],parseTier(w[4])};
        require(s.members.emplace(w[1],std::move(m)).second,"duplicate reader");
    } else if (w[0] == "LOAN") {
        require(w.size() == 8,"bad loan record length");
        bool closed = w[7] != "-";
        s.loans.push_back(Loan{w[1],w[2],parseTier(w[3]),Date::parse(w[4]),Date::parse(w[5]),
                          number(w[6],0,2),closed,closed ? Date::parse(w[7]) : Date()});
    } else if (w[0] == "HOLD") {
        require(w.size() == 5,"bad reservation record length");
        require(w[3] == "READY" || (w[3] == "WAITING" && w[4] == "-"),"invalid reservation status");
        bool ready = w[3] == "READY";
        bookById(s,w[1]).holds.push_back(Hold{w[2],ready,ready ? Date::parse(w[4]) : Date()});
    } else throw std::runtime_error("unknown record type " + w[0]);
}
std::string serialize(const State& s) {
    std::string out = "STACKDESK 1\nDATE " + s.day.str() + '\n';
    for (const auto& item : s.books) {
        const Book& b = item.second;
        out += "BOOK " + quote(b.id) + ' ' + quote(b.title) + ' ' + quote(b.author)
            + ' ' + std::to_string(b.copies) + '\n';
    }
    for (const auto& item : s.members) {
        const Member& m = item.second;
        out += "READER " + quote(m.id) + ' ' + quote(m.name) + ' ' + quote(m.contact)
            + ' ' + tierName(m.tier) + '\n';
    }
    for (const Loan& l : s.loans) {
        out += "LOAN " + quote(l.book) + ' ' + quote(l.reader) + ' ' + tierName(l.tier)
            + ' ' + l.issued.str() + ' ' + l.due.str() + ' ' + std::to_string(l.renewals)
            + ' ' + (l.closed ? l.returned.str() : "-") + '\n';
    }
    for (const auto& item : s.books) {
        const Book& b = item.second;
        for (const Hold& h : b.holds) {
            out += "HOLD " + quote(b.id) + ' ' + quote(h.reader)
                + (h.ready ? " READY " + h.offered.str() : " WAITING -") + '\n';
        }
    }
    return out + "END\n";
}
}
PathLock::PathLock(fs::path directory) : directory_(std::move(directory)) {
    require(fs::create_directory(directory_),"path is locked: " + directory_.string());
}
PathLock::~PathLock() {
    std::error_code ignored;
    fs::remove_all(directory_,ignored);
}
Storage::Storage(const fs::path& path)
    : file_(databasePath(path)), backup_(file_.string()+".bak"), lock_(file_.string()+".lock") {}
State Storage::load(Date day) const {
    State s;
    s.day = day;
    if (!regularOrMissing(file_)) {
        require(!regularOrMissing(backup_),"database missing but a backup exists; restore it before continuing");
        return s;
    }

    std::ifstream in(file_);
    require(static_cast<bool>(in),"cannot open database " + file_.string());
    std::string line;
    int lineNo = 1;
    try {
        require(static_cast<bool>(std::getline(in,line)) && split(line) == Words{"STACKDESK","1"},
                "unrecognised database header");
        ++lineNo;
        require(static_cast<bool>(std::getline(in,line)),"missing database date");
        Words date = split(line);
        require(date.size() == 2 && date[0] == "DATE","invalid database date record");
        s.day = Date::parse(date[1]);

        bool ended = false;
        while (std::getline(in,line)) {
            ++lineNo;
            Words w = split(line);
            if (w.empty()) continue;
            require(!ended,"unexpected record after END");
            if (w == Words{"END"}) ended = true;
            else readRow(s,w);
        }
        require(ended && in.eof() && !in.bad(),"snapshot is incomplete or unreadable");
        validate(s);
    } catch (const std::exception& e) {
        throw std::runtime_error("database " + file_.string() + " line " + std::to_string(lineNo) + ": " + e.what());
    }
    return s;
}
void Storage::save(const State& s) const {
    const fs::path draft = lock_.path() / "draft";
    const fs::path backupDraft = lock_.path() / "backup";
    validate(s);
    writeFile(draft,serialize(s));
    if (regularOrMissing(file_)) {
        regularOrMissing(backup_);
        fs::copy_file(file_,backupDraft,fs::copy_options::overwrite_existing);
        fs::rename(backupDraft,backup_);
    }
    fs::rename(draft,file_);
}
void writeCsv(const fs::path& destination, const std::string& contents) {
    const fs::path file = normalPath(destination);
    require(file.extension() == ".csv","export filename must end in .csv");
    require(!regularOrMissing(file),"export file already exists");
    PathLock staging(file.string()+".export");
    const fs::path staged = staging.path() / "report";
    writeFile(staged,contents);
    // A hard link publishes an already-complete CSV and fails if another file took the name.
    fs::create_hard_link(staged,file);
}
}
