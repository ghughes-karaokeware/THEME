#include "../dll/CHPromoMarkup.h"
#include <cstdio>
#include <string>
#include <vector>

int main()
{
    const std::vector<std::string> cases = {
        "Plain text",
        "<B>Bold</B>",
        "<I>Italic</I>",
        "<U>Underline</U>",
        "<3>Red<0>",
        "<3><B>Bold red</B><0>",
        "<B><I><U>All three</U></I></B>",
        "<2>Yellow <B>bold <I>italic</I></B><0>",
        "<3>Red</B> malformed surrounding text",
        "Unknown <X>tag</X> remains",
        "  leading and trailing  ",
        "One\r\nTwo\r\nThree",
        "<3><B>Formatted</B><0>\r\nPlain",
        "<3>Red\r\nPlain"
    };
    for (const auto& input : cases) {
        unsigned unknown{}; const auto first = chpromo::Parse(input, &unknown);
        const auto encoded = chpromo::Serialize(first); const auto second = chpromo::Parse(encoded);
        if (chpromo::PlainText(first) != chpromo::PlainText(second)) {
            std::printf("FAIL plain roundtrip: %s -> %s\n", input.c_str(), encoded.c_str()); return 1;
        }
        const auto stable = chpromo::Serialize(second);
        if (encoded != stable) { std::printf("FAIL canonical stability: %s -> %s -> %s\n", input.c_str(), encoded.c_str(), stable.c_str()); return 2; }
    }
    const auto blank = chpromo::Serialize(chpromo::Parse("\r\nOne\r\n\r\nTwo\r\n"));
    if (blank != "One\r\nTwo") { std::printf("FAIL blank semantics: %s\n", blank.c_str()); return 3; }
    const auto reset = chpromo::Serialize(chpromo::Parse("<3><B>Red bold\r\nPlain"));
    if (reset.find("</B><0>\r\nPlain") == std::string::npos) { std::printf("FAIL line reset: %s\n", reset.c_str()); return 4; }
    std::puts("Promo markup parser/serializer: PASS"); return 0;
}
