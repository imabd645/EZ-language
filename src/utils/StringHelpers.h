#ifndef EZ_UTILS_STRING_HELPERS_H
#define EZ_UTILS_STRING_HELPERS_H

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace EZ {
namespace Utils {

// Computes the Levenshtein distance between two strings, ignoring case.
// Bails out early if the distance exceeds `limit`, returning limit + 1.
inline size_t nameDistance(const std::string& a, const std::string& b, size_t limit = 2) {
    if (a == b) return 0;
    if (a.size() > b.size() + limit || b.size() > a.size() + limit) return limit + 1;

    std::vector<size_t> previous(b.size() + 1), current(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) previous[j] = j;
    for (size_t i = 1; i <= a.size(); ++i) {
        current[0] = i;
        size_t best = current[0];
        for (size_t j = 1; j <= b.size(); ++j) {
            size_t cost = (std::tolower((unsigned char)a[i - 1]) ==
                           std::tolower((unsigned char)b[j - 1])) ? 0 : 1;
            current[j] = std::min({ previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost });
            best = std::min(best, current[j]);
        }
        if (best > limit) return limit + 1;   // no cell in this row can recover
        previous = current;
    }
    return previous[b.size()];
}

} // namespace Utils
} // namespace EZ

#endif // EZ_UTILS_STRING_HELPERS_H
