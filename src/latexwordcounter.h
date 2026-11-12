// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TEXSTUDIO_WORDCOUNT_LATEXWORDCOUNTER_H
#define TEXSTUDIO_WORDCOUNT_LATEXWORDCOUNTER_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace texstudio_wordcount {

// Additional source syntax for projects that use their own LaTeX commands.
struct WordCountRules {
    struct Command {
        // The control word, without the backslash or optional star.
        std::string name;
        unsigned metadataArguments = 0;
    };

    // A command rule skips leading braced metadata and adjacent optional
    // arguments. Any following text arguments are still scanned for prose.
    std::vector<Command> commands;

    // Exact environment names, including any star. The entire environment
    // body is skipped using ordinary LaTeX comment and command syntax.
    std::vector<std::string> ignoredEnvironments;
};

struct WordCounts {
    std::size_t englishWords = 0;
    std::size_t chineseCharacters = 0;

    constexpr std::size_t total() const noexcept { return englishWords + chineseCharacters; }
};

// An index for one immutable UTF-8 source snapshot. The input is only needed
// during construction. Querying another selection does not rescan the source.
class WordIndex {
public:
    explicit WordIndex(std::string_view text, const WordCountRules &rules = {});

    // Offsets are UTF-8 byte positions in a half-open range. A clipped word
    // counts when the selected range overlaps at least one of its letters.
    std::size_t count(std::size_t selectionBegin = 0,
                      std::size_t selectionEnd = std::string_view::npos) const;

    // A visible Han ideograph counts once if any of its UTF-8 bytes overlap.
    WordCounts counts(std::size_t selectionBegin = 0,
                      std::size_t selectionEnd = std::string_view::npos) const;

private:
    struct LetterRun {
        std::size_t begin;
        std::size_t end;
        std::size_t word;
    };
    struct HanSpan {
        std::size_t begin;
        std::size_t end;
    };
    class Scanner;

    std::vector<LetterRun> letters_;
    std::vector<HanSpan> han_;
    std::size_t textSize_;
};

std::size_t countWords(std::string_view text,
                       std::size_t selectionBegin = 0,
                       std::size_t selectionEnd = std::string_view::npos);

} // namespace texstudio_wordcount

#endif
