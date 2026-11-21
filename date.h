#ifndef DESK_DATE_H
#define DESK_DATE_H
#include <string>
namespace desk {
// A Gregorian calendar day. Arithmetic counts midnight boundaries, without time zones.
class Date {
    int ordinal_;
    explicit Date(int ordinal);
public:
    Date();
    static Date parse(const std::string& iso);
    static Date today();
    std::string str() const;
    Date plus(int days) const;
    int operator-(Date other) const;
    bool operator<(Date other) const;
    bool operator==(Date other) const;
};
}
#endif
