#include "policy.h"
#include <stdexcept>
namespace desk {
namespace {
class StudentPolicy : public LoanPolicy {
public:
    int days() const override { return 14; }
    int maxLoans() const override { return 3; }
    int maxRenewals() const override { return 1; }
};
class FacultyPolicy : public LoanPolicy {
public:
    int days() const override { return 28; }
    int maxLoans() const override { return 5; }
    int maxRenewals() const override { return 2; }
};
}
Tier parseTier(const std::string& text) {
    if (text == "STUDENT" || text == "student") return Tier::Student;
    if (text == "FACULTY" || text == "faculty") return Tier::Faculty;
    throw std::runtime_error("tier must be STUDENT or FACULTY");
}
std::string tierName(Tier tier) { return tier == Tier::Student ? "STUDENT" : "FACULTY"; }
Policies::Policies() : student_(new StudentPolicy), faculty_(new FacultyPolicy) {}
const LoanPolicy& Policies::get(Tier tier) const {
    if (tier == Tier::Student) return *student_;
    return *faculty_;
}
}
