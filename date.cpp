#include "date.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace desk {
namespace {
bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
int beforeYear(int y) {
    int n = y - 1;
    return 365 * n + n / 4 - n / 100 + n / 400;
}
int monthDays(int y, int m) {
    static const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return lengths[m-1] + (m == 2 && leap(y) ? 1 : 0);
}
}
Date::Date() : ordinal_(beforeYear(1900)) {}
Date::Date(int ordinal) : ordinal_(ordinal) {}
Date Date::parse(const std::string& iso) {
    if (iso.size() != 10 || iso[4] != '-' || iso[7] != '-')
        throw std::runtime_error("expected date YYYY-MM-DD");
    for (std::size_t i = 0; i < iso.size(); ++i)
        if (i != 4 && i != 7 && (iso[i] < '0' || iso[i] > '9'))
            throw std::runtime_error("expected date YYYY-MM-DD");
    int y = std::stoi(iso.substr(0,4)), m = std::stoi(iso.substr(5,2));
    int d = std::stoi(iso.substr(8,2));
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > monthDays(y,m))
        throw std::runtime_error("invalid calendar date (years 1900-9999)");
    int ordinal = beforeYear(y) + d - 1;
    for (int month = 1; month < m; ++month) ordinal += monthDays(y, month);
    return Date(ordinal);
}
Date Date::today() {
    std::time_t t = std::time(nullptr);
    std::tm* local = std::localtime(&t);
    char text[16];
    if (!local || !std::strftime(text, sizeof(text), "%Y-%m-%d", local))
        throw std::runtime_error("cannot read local date; supply --date");
    return parse(text);
}
std::string Date::str() const {
    int lo = 1900, hi = 10000;
    while (lo + 1 < hi) {
        int mid = (lo + hi) / 2;
        if (beforeYear(mid) <= ordinal_) lo = mid; else hi = mid;
    }
    int m = 1, d = ordinal_ - beforeYear(lo) + 1;
    while (d > monthDays(lo,m)) { d -= monthDays(lo,m); ++m; }
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << lo << '-' << std::setw(2) << m
        << '-' << std::setw(2) << d;
    return out.str();
}
Date Date::plus(int days) const {
    long long next = static_cast<long long>(ordinal_) + days;
    if (next < beforeYear(1900) || next >= beforeYear(10000))
        throw std::runtime_error("date arithmetic exceeds supported calendar");
    return Date(static_cast<int>(next));
}
int Date::operator-(Date other) const { return ordinal_ - other.ordinal_; }
bool Date::operator<(Date other) const { return ordinal_ < other.ordinal_; }
bool Date::operator==(Date other) const { return ordinal_ == other.ordinal_; }
}
