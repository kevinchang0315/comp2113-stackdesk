#include "desk.h"
#include "storage.h"
#include "web_view.h"
#include <algorithm>
#include <iostream>
#include <utility>
using namespace desk;

namespace {
enum class Group { Books, Readers, Loans, Holds, Reports, Clock, Web };
struct Command {
    std::string name, label;
    Group group;
    Words fields;
    bool writesDatabase;
};
const std::vector<Command> commands{
    {"add-book", "Add a book", Group::Books, {"Book ID","Title","Author","Copies"}, true},
    {"book", "View a book by ID", Group::Books, {"Book ID"}, false},
    {"find-book", "Search titles, authors or IDs", Group::Books, {"Search text"}, false},
    {"edit-book", "Edit book details and stock", Group::Books, {"Book ID","New title","New author","Total copies"}, true},
    {"books", "List books", Group::Books, {}, false},
    {"set-topics", "Edit topics on a book", Group::Books, {"Book ID","Topics separated by commas"}, true},
    {"add-reader", "Register a reader", Group::Readers, {"Reader ID","Name","Contact","Tier (STUDENT/FACULTY)"}, true},
    {"reader", "View a reader by ID", Group::Readers, {"Reader ID"}, false},
    {"find-reader", "Search readers by name or ID", Group::Readers, {"Search text"}, false},
    {"edit-reader", "Edit a reader", Group::Readers, {"Reader ID","New name","New contact","Tier (STUDENT/FACULTY)"}, true},
    {"readers", "List readers", Group::Readers, {}, false},
    {"borrow", "Check out a copy or collect a hold", Group::Loans, {"Book ID","Reader ID"}, true},
    {"return", "Return a copy", Group::Loans, {"Book ID","Reader ID"}, true},
    {"renew", "Renew a loan", Group::Loans, {"Book ID","Reader ID"}, true},
    {"history", "View a reader's loan history", Group::Loans, {"Reader ID"}, false},
    {"policy", "Show limits and loan period", Group::Loans, {"Tier (STUDENT/FACULTY)"}, false},
    {"reserve", "Join a title's queue", Group::Holds, {"Book ID","Reader ID"}, true},
    {"queue", "View a reservation queue", Group::Holds, {"Book ID"}, false},
    {"cancel-hold", "Cancel a reservation", Group::Holds, {"Book ID","Reader ID"}, true},
    {"stats", "Show stock and popular titles", Group::Reports, {}, false},
    {"overdue", "List overdue loans", Group::Reports, {}, false},
    {"export-overdue", "Export overdue loans to a new CSV", Group::Reports, {"New CSV path"}, false},
    {"date", "Show the business date", Group::Clock, {}, false},
    {"advance", "Advance the business date in demo mode", Group::Clock, {"YYYY-MM-DD"}, true},
    {"snapshot", "Print the browser snapshot", Group::Web, {}, false},
    {"catalog-search", "Rank catalog matches", Group::Web, {"Search text"}, false},
    {"recommendations-book", "Suggest available books with shared topics", Group::Web, {"Book ID"}, false},
    {"recommendations-reader", "Suggest books for a reader", Group::Web, {"Reader ID"}, false}
};
const std::pair<Group,const char*> groups[]{
    {Group::Books,"Book catalog"}, {Group::Readers,"Reader registry"},
    {Group::Loans,"Loans and returns"}, {Group::Holds,"Reservations"}, {Group::Reports,"Reports"}
};
struct Options {
    std::string database = "stackdesk.db";
    bool batch = false, fixedDate = false, help = false;
    Date day;
};
Options options(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--batch") opt.batch = true;
        else if (a == "--help" || a == "-h") opt.help = true;
        else if (a == "--db" || a == "--date") {
            require(i + 1 < argc, "missing value for " + a);
            if (a == "--db") opt.database = argv[++i];
            else { opt.day = Date::parse(argv[++i]); opt.fixedDate = true; }
        } else throw std::runtime_error("unknown option " + a + "; use --help");
    }
    if (!opt.fixedDate && !opt.help) opt.day = Date::today();
    return opt;
}

class Desk {
    const Storage& storage_;
    State state_;
    Policies policies_;
    bool fixedDate_;
public:
    Desk(const Storage& storage, const Options& opt)
        : storage_(storage), state_(storage.load(opt.day)), fixedDate_(opt.fixedDate) {
        if (fixedDate_) require(!(opt.day < state_.day),"--date is earlier than the saved business date");
        updateClock(opt.day);
    }
    Date day() const { return state_.day; }
    void updateClock(Date requested) {
        if (!(state_.day < requested)) return;
        State draft = state_;
        advanceDay(draft,requested);
        storage_.save(draft);
        state_ = std::move(draft);
    }
    // UI modes call the same dispatch. Save the entire draft before publishing any success.
    std::string execute(const Command& c, const Words& a) {
        require(a.size() == c.fields.size(),
                c.name + " expects " + std::to_string(c.fields.size()) + " argument(s)");
        if (!fixedDate_) updateClock(Date::today());
        if (!c.writesDatabase) return run(state_,c,a);
        State draft = state_;
        std::string result = run(draft,c,a);
        storage_.save(draft);
        state_ = std::move(draft);
        return result;
    }
private:
    std::string run(State& s, const Command& c, const Words& a) {
        switch (c.group) {
        case Group::Books: return catalog(s,c.name,a);
        case Group::Readers: return readers(s,c.name,a);
        case Group::Loans: return circulation(s,policies_,c.name,a);
        case Group::Holds: return reservations(s,c.name,a);
        case Group::Reports:
            if (c.name == "export-overdue") {
                writeCsv(a[0],overdueCsv(s));
                return "Exported overdue report: " + a[0];
            }
            return reports(s,c.name);
        case Group::Web:
            if (c.name == "snapshot") return webSnapshot(s,policies_);
            if (c.name == "catalog-search") return searchCatalog(s,a[0]);
            return webRecommendations(s,a[0],c.name == "recommendations-reader");
        case Group::Clock:
            if (c.name == "date") return s.day.str();
            require(fixedDate_,"advance requires --date at startup");
            Date target = Date::parse(a[0]);
            int expired = advanceDay(s,target);
            return "Date " + s.day.str() + " | expired pickups=" + std::to_string(expired);
        }
        throw std::logic_error("unknown command group");
    }
};
void usage() {
    std::cout << "Usage: ./main [--db FILE.db] [--date YYYY-MM-DD] [--batch]\n"
              << "Without --batch, use the numbered menus. Batch mode reads one command per line.\n"
              << "In batch, quote fields containing spaces, use help for commands and exit to finish.\n";
}
void batchHelp() {
    for (const Command& c : commands) {
        std::cout << c.name;
        for (const auto& field : c.fields) std::cout << " <" << field << '>';
        std::cout << '\n';
    }
    std::cout << "help\nexit\n";
}
int batch(Desk& desk) {
    int status = 0;
    std::string line;
    while (std::getline(std::cin,line)) {
        auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;
        try {
            Words fields = split(line);
            if (fields.empty()) continue;
            std::string name = lowerAscii(fields.front());
            if (name == "exit" || name == "help") {
                require(fields.size() == 1,name + " expects no arguments");
                if (name == "exit") break;
                batchHelp();
                continue;
            }
            auto c = std::find_if(commands.begin(),commands.end(),[&](const Command& row){return row.name == name;});
            require(c != commands.end(),"unknown command " + name + "; use help");
            std::cout << desk.execute(*c,Words(fields.begin()+1,fields.end())) << '\n';
        } catch (const std::exception& e) {
            std::cout << "ERROR: " << e.what() << '\n';
            status = 1;
        }
    }
    if (std::cin.bad()) throw std::runtime_error("cannot read input");
    return status;
}
bool ask(const std::string& prompt, std::string& value) {
    std::cout << prompt << std::flush;
    if (std::getline(std::cin,value)) {
        if (!value.empty() && value.back() == '\r') value.pop_back();
        return true;
    }
    if (std::cin.bad()) throw std::runtime_error("cannot read input");
    return false;
}
bool menuChoice(int count, int& v) {
    std::string line;
    if (!ask("Select: ",line)) return false;
    try {
        auto fields = split(line);
        require(fields.size() == 1,"choose one option");
        v = number(fields[0],0,count);
    } catch (const std::exception&) {
        std::cout << "Choose a number from 0 to " << count << ".\n";
        v = -1;
    }
    return true;
}
bool submenu(Desk& desk, Group group, const std::string& title) {
    std::vector<const Command*> items;
    for (const Command& c : commands) if (c.group == group) items.push_back(&c);
    while (true) {
        std::cout << "\n" << title << "\n";
        for (std::size_t i = 0; i < items.size(); ++i)
            std::cout << "  " << i + 1 << "  " << items[i]->label << '\n';
        std::cout << "  0  Back\n";
        int choice;
        if (!menuChoice(static_cast<int>(items.size()),choice)) return false;
        if (choice == -1) continue;
        if (choice == 0) return true;
        const Command& c = *items[choice-1];
        Words args;
        if (!c.fields.empty()) std::cout << "Type :back to cancel this action.\n";
        for (const std::string& label : c.fields) {
            std::string value;
            if (!ask(label + ": ",value)) return false;
            if (value == ":back") break;
            args.push_back(value);
        }
        if (args.size() != c.fields.size()) continue;
        try {
            std::cout << "\n" << desk.execute(c,args) << '\n';
        } catch (const std::exception& e) {
            std::cout << "\nERROR: " << e.what() << '\n';
        }
    }
}
int interactive(Desk& desk) {
    constexpr int count = static_cast<int>(sizeof(groups)/sizeof(groups[0]));
    while (true) {
        std::cout << "\nStackDesk  |  " << desk.day().str() << "\n";
        for (int i = 0; i < count; ++i) std::cout << "  " << i+1 << "  " << groups[i].second << '\n';
        std::cout << "  0  Exit\n";
        int choice;
        if (!menuChoice(count,choice) || choice == 0) break;
        if (choice > 0 && !submenu(desk,groups[choice-1].first,groups[choice-1].second)) break;
    }
    std::cout << "Desk closed.\n";
    return 0;
}
}
int main(int argc, char** argv) {
    try {
        const Options opt = options(argc,argv);
        if (opt.help) { usage(); return 0; }
        Storage storage(opt.database);
        Desk desk(storage,opt);
        return opt.batch ? batch(desk) : interactive(desk);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << '\n';
        return 2;
    }
}
