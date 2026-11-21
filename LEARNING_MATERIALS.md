# Runtime Polymorphism and Smart Pointer Ownership

StackDesk needs two lending policies. Both provide a loan duration, a maximum number of open loans and a renewal limit. Selecting the policy by reader tier at checkout keeps these decisions in one place and gives the loan workflow a common interface.

## The C++ ideas

**Abstract interface.** In the excerpt from `policy.h`, `= 0` makes each of the three policy methods pure virtual. A concrete class must implement the methods before you can create an object from it.

~~~cpp
class LoanPolicy {
public:
    virtual ~LoanPolicy() = default;
    virtual int days() const = 0;
    virtual int maxLoans() const = 0;
    virtual int maxRenewals() const = 0;
};
~~~

**Runtime dispatch.** The implementation in `policy.cpp` supplies two concrete classes. The keyword `override` asks the compiler to check each overriding declaration against the base interface. Changing the concrete object that a base reference refers to changes the behaviour of calls on that reference.

~~~cpp
class StudentPolicy : public LoanPolicy {
public:
    int days() const override { return 14; }
    int maxLoans() const override { return 3; }
    int maxRenewals() const override { return 1; }
};
~~~

The faculty implementation uses 28 days, five open loans and two renewals. At checkout, the person at the desk provides a book ID and reader ID. In `circulation.cpp`, the reader's stored tier selects the object.

~~~cpp
const LoanPolicy& p = rules.get(m.tier);
// After checking the open-loan limit and the copy's status:
Date due = s.day.plus(p.days());
~~~

The expression `p.days()` calls the concrete policy at run time. A renewal uses the tier stored with the loan and adds that policy's duration to the existing due date. This keeps the original terms even if a reader's profile moves from student to faculty. A new loan follows the reader's current profile.

**RAII and ownership.** In `policy.h`, each policy object has one owner. In `policy.cpp` these pointers take ownership at construction.

~~~cpp
// Members in Policies:
std::unique_ptr<LoanPolicy> student_;
std::unique_ptr<LoanPolicy> faculty_;

// Constructor in policy.cpp:
Policies::Policies() : student_(new StudentPolicy), faculty_(new FacultyPolicy) {}
~~~

When `Policies` leaves scope, its members release both policy objects automatically. If constructing the second object fails, C++ unwinds already-constructed members and releases the first. The destructor of the base is virtual so deletion through `LoanPolicy*` invokes the derived destructor. A returned `const LoanPolicy&` borrows access and is valid while its owning `Policies` object lives. Copying a `unique_ptr` is disallowed because there can only be one owner.

## Verify the feature in the application

Run `make demo` or replay test T04 in [the transcripts](tests/TRANSCRIPTS.md). On January 1, 2026, the student checkout is due January 15 and the faculty checkout is due January 29. Then change the student to faculty, renew their first loan and issue a new loan. The first loan extends by 14 days from its original due date. The new one starts with a 28-day term.

T05 checks the exact open-loan limits for both concrete policies. T06 checks the last permitted renewal and the first rejected renewal for each. These checks exercise the feature through the same borrowing operations used by library staff.

## Practice

1. Add a third tier in a local branch. Identify the concrete policy class, the tier parser, the selection function and the tests that need to change.
2. Temporarily change `StudentPolicy::days()` to 10. Predict which due dates and renewal dates change in T04 before running `make test`.
3. Trace the ownership of the two policy objects from `Policies::Policies()` to the end of `main`. Locate the line that borrows a policy reference and identify which object must outlive it.
