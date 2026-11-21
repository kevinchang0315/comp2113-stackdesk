#ifndef DESK_WEB_VIEW_H
#define DESK_WEB_VIEW_H
#include "model.h"
namespace desk {
// Export a read-only JSON snapshot for the browser, including C++-computed copy counts.
std::string webSnapshot(const State& state, const Policies& policies);
// Return ranked catalog matches as JSON. Exact and prefix matches outrank spelling corrections.
std::string searchCatalog(const State& state, const std::string& query);
std::string webRecommendations(const State& state, const std::string& id, bool byReader);
}
#endif
