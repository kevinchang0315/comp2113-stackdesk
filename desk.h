#ifndef DESK_ACTIONS_H
#define DESK_ACTIONS_H
#include "model.h"
namespace desk {
// Each action receives validated argument count. A successful result is printable text.
// Mutating actions operate on a draft. Nothing reaches the live state until saving succeeds.
// Catalog actions use a book ID or search text and return a record or confirmation.
std::string catalog(State&, const std::string& action, const Words& args);
// Reader actions use a reader ID or search text and return a profile or confirmation.
std::string readers(State&, const std::string& action, const Words& args);
// Circulation updates or inspects loans and applies the appropriate tier policy.
std::string circulation(State&, const Policies&, const std::string& action, const Words& args);
// Reservation actions manage a title's FIFO waitlist and pickup promises.
std::string reservations(State&, const std::string& action, const Words& args);
std::string reports(const State&, const std::string& action);
std::string overdueCsv(const State&);
// Assign newly freed copies to the earliest readers. An offer is valid for three calendar days.
void offerCopies(Book&, int onLoan, Date offerDay);
// Advance in expiry order so skipping ten days yields the same queue as ten daily updates.
int advanceDay(State&, Date target);
}
#endif
