#pragma once
#include <string>
#include <vector>

namespace chpromo {
struct Style { int color = 0; bool bold = false, italic = false, underline = false; };
inline bool operator==(const Style& a, const Style& b) { return a.color == b.color && a.bold == b.bold && a.italic == b.italic && a.underline == b.underline; }
inline bool operator!=(const Style& a, const Style& b) { return !(a == b); }
struct Character { wchar_t value = 0; Style style; };
struct Document { std::vector<Character> characters; };
Document Parse(const std::string& encoded, unsigned* unknownTags = nullptr);
std::string Serialize(const Document& document);
std::wstring PlainText(const Document& document);
}
