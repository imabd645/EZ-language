#ifndef EZ_UTF8_H
#define EZ_UTF8_H

#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#include <cstddef>

// ============================================================================
// Minimal UTF-8 helpers.
//
// EZ strings are byte strings that hold UTF-8. Most operations (concat, split,
// replace, indexOf, contains, trim) are byte-safe by construction: they only
// ever move whole sequences around. Three were not, and turned valid text into
// invalid text:
//
//   reverse("café")  ->  bytes 169 195 102 97 99   (sequence reversed)
//   chr(233)         ->  byte 233                  (needs 195 169)
//   ord("é")         ->  195                       (lead byte, not U+00E9)
//
// These helpers exist for those. Decoding is deliberately LENIENT: a byte that
// is not part of a well-formed sequence is treated as a single character rather
// than raising. Strings can arrive from files, sockets and FFI, and a decoder
// that threw would turn "this text has one bad byte" into "this program dies".
// ============================================================================

namespace ez_utf8 {

// Bytes in the sequence introduced by lead byte `c`, or 1 if it is not a valid
// lead (so callers always make progress).
inline size_t seqLen(unsigned char c) {
    if (c < 0x80) return 1;           // 0xxxxxxx
    if ((c & 0xE0) == 0xC0) return 2; // 110xxxxx
    if ((c & 0xF0) == 0xE0) return 3; // 1110xxxx
    if ((c & 0xF8) == 0xF0) return 4; // 11110xxx
    return 1;                         // continuation or invalid: consume one
}

// True if the whole sequence starting at `i` is present and well-formed.
inline bool validAt(const std::string& s, size_t i, size_t n) {
    if (n == 1) return (unsigned char)s[i] < 0x80 || seqLen((unsigned char)s[i]) == 1;
    if (i + n > s.size()) return false;
    for (size_t k = 1; k < n; ++k) {
        if (((unsigned char)s[i + k] & 0xC0) != 0x80) return false; // not 10xxxxxx
    }
    return true;
}

// Decode the character starting at `i`, advancing `i` past it.
// An ill-formed byte decodes to itself and advances by one.
inline uint32_t decode(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i];
    size_t n = seqLen(c);
    if (n == 1 || !validAt(s, i, n)) { i += 1; return c; }

    uint32_t cp = 0;
    switch (n) {
        case 2: cp = c & 0x1F; break;
        case 3: cp = c & 0x0F; break;
        default: cp = c & 0x07; break;
    }
    for (size_t k = 1; k < n; ++k) {
        cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
    }
    i += n;
    return cp;
}

// Append `cp` to `out` as UTF-8. Returns false for a value outside Unicode or
// in the surrogate range, leaving `out` untouched.
inline bool encode(uint32_t cp, std::string& out) {
    if (cp > 0x10FFFF) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false; // lone surrogate
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
    return true;
}

// Reverse by character rather than by byte, so multi-byte sequences survive.
inline std::string reverseChars(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    // Walk forward recording each character's extent, then emit them backwards:
    // the sequences keep their internal byte order, only their order changes.
    std::vector<std::pair<size_t, size_t>> spans; // offset, length
    size_t i = 0;
    while (i < s.size()) {
        size_t start = i;
        size_t n = seqLen((unsigned char)s[i]);
        if (n == 1 || !validAt(s, i, n)) n = 1;
        i = start + n;
        spans.push_back({start, n});
    }
    for (size_t k = spans.size(); k-- > 0; ) {
        out.append(s, spans[k].first, spans[k].second);
    }
    return out;
}

} // namespace ez_utf8

#endif // EZ_UTF8_H
