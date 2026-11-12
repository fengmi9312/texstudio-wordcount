#include "latexwordcounter.h"

#include <cstddef>
#include <iostream>
#include <string_view>

namespace {

using texstudio_wordcount::countWords;
using texstudio_wordcount::WordCountRules;
using texstudio_wordcount::WordCounts;
using texstudio_wordcount::WordIndex;

int failures = 0;

void expect(std::string_view name, std::size_t actual, std::size_t expected)
{
    if (actual == expected) {
        return;
    }
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    ++failures;
}

void expectSelection(const WordIndex &index, std::string_view text,
                     std::string_view selection, std::size_t expected)
{
    const auto begin = text.find(selection);
    if (begin == std::string_view::npos) {
        std::cerr << "Selection fixture is missing: " << selection << '\n';
        ++failures;
        return;
    }
    expect(selection, index.count(begin, begin + selection.size()), expected);
    expect(selection, countWords(text, begin, begin + selection.size()), expected);
}

void countEnglishProse()
{
    expect("empty input", countWords(""), 0);
    expect("numbers and punctuation", countWords("123 3.14 ... ! ?"), 0);
    expect("English word boundaries", countWords("One, two. Three! four?"), 4);
    expect("contractions, hyphens and alphanumeric words",
           countWords("don't co-operate model2 42 3.14"), 3);
    expect("Unicode apostrophe and hyphen",
           countWords(u8"paper\u2019s peer\u2011review"), 2);
    expect("Latin accents", countWords(u8"na\u00efve caf\u00e9"), 2);
    expect("non-English scripts", countWords(u8"\u4e2d\u6587 English \u03b1\u03b2"), 1);
}

void countLaTeXProse()
{
    expect("nested formatting and footnote prose",
           countWords(R"(A \emph{careful \textbf{review}} helps.\footnote{Further details.})"), 6);
    expect("document body and section text",
           countWords(R"(
\documentclass{article}
\newcommand{\metadata}{Text from the preamble}
\begin{document}
\section{An introduction}
Body text.
\end{document}
Trailing material.
)"), 4);
    expect("citations, references and labels",
           countWords(R"(
See \cite[compare][page ten]{smith2025,jones2024} and \ref{sec:related}.
\label{fig:sample} Figure caption.
)"), 4);
    expect("short caption excluded",
           countWords(R"(\caption[Short version]{Actual caption.})"), 2);
    expect("hyperlink target excluded",
           countWords(R"(\href{https://example.test/data/file.pdf}{Online data})"), 2);
    expect("foreign quotation language excluded",
           countWords(R"(\foreignquote{english}{Quoted text.})"), 2);
    expect("headings and adjoining prose",
           countWords(R"(\chapter[Short title]{Visible heading}Following text)"), 4);
    expect("PDF bookmark fallback excluded",
           countWords(R"(\texorpdfstring{Visible title}{Bookmark fallback} text)"), 3);
    expect("multi-citation notes and keys excluded",
           countWords(R"(As shown \textcites(global note)(another note)[page four]{key one}{key two}.)"), 2);
    expect("csquotes preserves prose and excludes language and citation arguments",
           countWords(R"(
prefix\enquote{quoted words}suffix
\hyphenquote*{english}{Another quotation}
\foreigntextquote{english}[Citation metadata]{Quoted study}
\textcquote[See also][chapter two]{source:key}[.]{Source text}
)"), 10);
    expect("reference formatting definitions",
           countWords(R"(\crefname{figure}{diagram}{diagrams} Body text.)"), 2);
    expect("incomplete editing before unclosed markup",
           countWords(R"(A \emph{working draft $hidden formula)"), 3);
    expect("comments and escaped comment markers",
           countWords("rate\\% estimate % hidden words with $ or {\r\n"
                      "after comment"), 4);
    expect("inline and displayed math",
           countWords(R"(
Before $hidden + \text{inside}$ \(more words\)
$$math text$$ \[display contents\] after.
)"), 2);
    expect("math environment",
           countWords(R"(
Before
\begin{align*}
  x &= \text{not prose} \\
  y &= z
\end{align*}
after
)"), 2);
    expect("verbatim command and environment",
           countWords(R"(
Before \verb|not prose or math $| after
\begin{verbatim}
ignore words with $ and unmatched {
\end{verbatim}
done
)"), 3);
}

void countWithProjectRules()
{
    constexpr std::string_view text = R"(
Before \projecttext[layout hint]{metadata label}[another hint]{Visible text}.
\begin{projectannotation}hidden words
\begin{projectannotation}nested hidden words\end{projectannotation}
\end{projectannotation} After.
)";
    const WordIndex index(text, WordCountRules{{{"projecttext", 1}}, {"projectannotation"}});
    expect("project metadata and nested environments", index.count(), 4);
    const auto start = text.find("Visible text");
    expect("project rules preserve visible selection", index.count(start, start + 12), 2);
}

void countSelectionsInDocumentContext()
{
    constexpr std::string_view text = R"(
Prose before \textbf{peer-review results} $formula words$ % comment words
\label{section identifier} final words.
)";
    const WordIndex index(text);

    expect("whole input through index", index.count(), 6);
    expect("empty selection", index.count(3, 3), 0);
    expect("out of range selection", index.count(text.size() + 1, text.size() + 2), 0);
    expectSelection(index, text, "review results", 2);
    expectSelection(index, text, "peer-", 1);
    expectSelection(index, text, "-", 0);
    expectSelection(index, text, "textbf", 0);
    expectSelection(index, text, "formula words", 0);
    expectSelection(index, text, "comment words", 0);
    expectSelection(index, text, "section identifier", 0);
    expectSelection(index, text, "results} $formula", 1);
    expectSelection(index, text, "final words", 2);

    constexpr std::string_view nonAsciiPrefix = u8"\u4e2d\u6587 alpha beta";
    const WordIndex unicodeIndex(nonAsciiPrefix);
    expectSelection(unicodeIndex, nonAsciiPrefix, "beta", 1);
    expect("selection past end is clipped",
           unicodeIndex.count(nonAsciiPrefix.size() - 2, nonAsciiPrefix.size() + 10), 1);

    constexpr std::string_view number = "model2025";
    const WordIndex numberIndex(number);
    expectSelection(numberIndex, number, "2025", 0);
}

void expectCounts(std::string_view name, WordCounts actual, std::size_t english,
                  std::size_t chinese)
{
    if (actual.englishWords == english && actual.chineseCharacters == chinese
        && actual.total() == english + chinese) {
        return;
    }
    std::cerr << name << ": expected EN " << english << ", Chinese " << chinese
              << ", total " << english + chinese << "; got EN " << actual.englishWords
              << ", Chinese " << actual.chineseCharacters << ", total " << actual.total() << '\n';
    ++failures;
}

void countMixedLanguageSelections()
{
    constexpr std::string_view text =
        u8"Caf\u00e9\u524d\u7f00\U00020000 \u8bba\u6587 \\textbf{mixed\u6b63\u6587} words.\n"
        u8"Alpha $x + \\text{\u516c\u5f0f}$ \\cite{\u5f15\u6587} Omega. % \u6ce8\u91ca\n"
        u8"\u3007 \u3042 \ud55c \uff0c \\emph{\u7e41\u9ad4\u6f22\u5b57}";
    const WordIndex index(text);
    expectCounts("mixed-language document", index.counts(), 5, 12);
    expect("legacy English count stays English-only", index.count(), 5);

    const auto supplementary = text.find(u8"\U00020000");
    const auto mixedEnd = text.find("words.") + 6;
    expectCounts("mixed selection after Unicode prefix", index.counts(supplementary, mixedEnd), 2, 5);
    expectCounts("one supplementary Han character",
                 index.counts(supplementary, supplementary + 4), 0, 1);
    expectCounts("range cutting through a Han character",
                 index.counts(supplementary + 1, supplementary + 3), 0, 1);
    expectCounts("empty mixed-language selection", index.counts(supplementary, supplementary), 0, 0);

    const auto expectMixedFragment = [&](std::string_view name, std::string_view fragment,
                                         std::size_t english, std::size_t chinese) {
        const auto begin = text.find(fragment);
        expectCounts(name, index.counts(begin, begin + fragment.size()), english, chinese);
    };
    expectMixedFragment("adjacent Latin and Han text in formatting", u8"mixed\u6b63\u6587", 1, 2);
    expectMixedFragment("Chinese formula text excluded", u8"\u516c\u5f0f", 0, 0);
    expectMixedFragment("Chinese citation key excluded", u8"\u5f15\u6587", 0, 0);
    expectMixedFragment("Chinese comment excluded", u8"\u6ce8\u91ca", 0, 0);
    expectMixedFragment("ideographic zero is Han", u8"\u3007", 0, 1);
    expectMixedFragment("traditional Han in formatting", u8"\u7e41\u9ad4\u6f22\u5b57", 0, 4);
    expectMixedFragment("Kana, Hangul and fullwidth punctuation excluded", u8"\u3042 \ud55c \uff0c", 0, 0);

    const WordIndex body(u8"Hidden \u524d\\begin{document}Real \u8bba\\end{document}Extra \u7f00");
    expectCounts("preamble and trailing Han excluded", body.counts(), 1, 1);
}
} // namespace

int main()
{
    countEnglishProse();
    countLaTeXProse();
    countWithProjectRules();
    countSelectionsInDocumentContext();
    countMixedLanguageSelections();
    return failures == 0 ? 0 : 1;
}
