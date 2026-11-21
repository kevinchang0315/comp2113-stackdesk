#ifndef DESK_POLICY_H
#define DESK_POLICY_H
#include <memory>
#include <string>
namespace desk {
enum class Tier { Student, Faculty };
Tier parseTier(const std::string& text);
std::string tierName(Tier tier);
// Both tiers implement the same contract. Checkout and renewal consult it at run time.
class LoanPolicy {
public:
    virtual ~LoanPolicy() = default;
    virtual int days() const = 0;
    virtual int maxLoans() const = 0;
    virtual int maxRenewals() const = 0;
};
class Policies {
    std::unique_ptr<LoanPolicy> student_;
    std::unique_ptr<LoanPolicy> faculty_;
public:
    Policies();
    const LoanPolicy& get(Tier tier) const;
};
}
#endif
