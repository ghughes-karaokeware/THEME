#include "CHPromoMarkup.h"
#include <windows.h>
#include <algorithm>
#include <cctype>

namespace chpromo {
namespace {
std::string Upper(std::string value) { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); }); return value; }
void CloseStyles(std::string& out, const Style& state) { if (state.underline) out += "</U>"; if (state.italic) out += "</I>"; if (state.bold) out += "</B>"; }
void OpenStyles(std::string& out, const Style& state) { if (state.bold) out += "<B>"; if (state.italic) out += "<I>"; if (state.underline) out += "<U>"; }
}

Document Parse(const std::string& encoded, unsigned* unknownTags)
{
    Document result; Style state{}; unsigned unknown = 0;
    for (size_t i = 0; i < encoded.size();) {
        if (encoded[i] == '<') {
            const size_t end = encoded.find('>', i + 1);
            if (end != std::string::npos) {
                const std::string raw = encoded.substr(i, end - i + 1);
                const std::string tag = Upper(raw);
                bool known = true;
                if (tag.size() == 3 && tag[1] >= '0' && tag[1] <= '9') state.color = tag[1] - '0';
                else if (tag == "<B>") state.bold = true; else if (tag == "</B>") state.bold = false;
                else if (tag == "<I>") state.italic = true; else if (tag == "</I>") state.italic = false;
                else if (tag == "<U>") state.underline = true; else if (tag == "</U>") state.underline = false;
                else known = false;
                if (known) { i = end + 1; continue; }
                ++unknown;
            }
        }
        unsigned char byte = static_cast<unsigned char>(encoded[i]);
        if (byte == '\r') { if (i + 1 < encoded.size() && encoded[i + 1] == '\n') ++i; result.characters.push_back({L'\n', {}}); state = {}; }
        else if (byte == '\n' || byte == '|') { result.characters.push_back({L'\n', {}}); state = {}; }
        else { wchar_t converted{}; char source=static_cast<char>(byte); if(!MultiByteToWideChar(1252,0,&source,1,&converted,1))converted=L'?'; result.characters.push_back({converted, state}); }
        ++i;
    }
    if (unknownTags) *unknownTags = unknown;
    return result;
}

std::wstring PlainText(const Document& document) { std::wstring out; out.reserve(document.characters.size()); for (const auto& c : document.characters) out.push_back(c.value); return out; }

std::string Serialize(const Document& document)
{
    std::string out, line; Style active{}; bool visible = false;
    auto finishLine = [&]() { CloseStyles(line, active); if (active.color != 0) line += "<0>"; if (visible) { if (!out.empty()) out += "\r\n"; out += line; } line.clear(); active = {}; visible = false; };
    for (const auto& ch : document.characters) {
        if (ch.value == L'\r') continue;
        if (ch.value == L'\n') { finishLine(); continue; }
        if (ch.style != active) {
            CloseStyles(line, active);
            if (ch.style.color != active.color) { line += '<'; line += static_cast<char>('0' + std::clamp(ch.style.color, 0, 9)); line += '>'; }
            OpenStyles(line, ch.style); active = ch.style;
        }
        char encoded='?'; BOOL usedDefault=FALSE; WideCharToMultiByte(1252,WC_NO_BEST_FIT_CHARS,&ch.value,1,&encoded,1,"?",&usedDefault); line += encoded; visible = true;
    }
    finishLine(); return out;
}
}
