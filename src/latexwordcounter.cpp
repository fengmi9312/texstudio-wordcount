// SPDX-License-Identifier: GPL-2.0-or-later

#include "latexwordcounter.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

namespace texstudio_wordcount {
namespace {

bool contains(std::string_view value, std::initializer_list<std::string_view> choices)
{
    return std::find(choices.begin(), choices.end(), value) != choices.end();
}

bool asciiLetter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

bool latinLetter(std::uint32_t c)
{
    return (c <= 0x7f && asciiLetter(static_cast<char>(c)))
        || (c >= 0x00c0 && c <= 0x00d6)
        || (c >= 0x00d8 && c <= 0x00f6)
        || (c >= 0x00f8 && c <= 0x024f)
        || (c >= 0x1e00 && c <= 0x1eff)
        || (c >= 0xfb00 && c <= 0xfb06);
}

bool hanIdeograph(std::uint32_t c)
{
    // Assigned unified and compatibility ideographs, plus ideographic zero.
    // Unicode 17.0: https://www.unicode.org/Public/17.0.0/ucd/PropList.txt
    if (c < 0x3007) return false;
    return c == 0x3007
        || (c >= 0x3400 && c <= 0x4dbf)
        || (c >= 0x4e00 && c <= 0x9fff)
        || (c >= 0xf900 && c <= 0xfa6d)
        || (c >= 0xfa70 && c <= 0xfad9)
        || (c >= 0x20000 && c <= 0x2a6df)
        || (c >= 0x2a700 && c <= 0x2b81d)
        || (c >= 0x2b820 && c <= 0x2cead)
        || (c >= 0x2ceb0 && c <= 0x2ebe0)
        || (c >= 0x2ebf0 && c <= 0x2ee5d)
        || (c >= 0x2f800 && c <= 0x2fa1d)
        || (c >= 0x30000 && c <= 0x3134a)
        || (c >= 0x31350 && c <= 0x33479);
}

template <typename Span>
auto overlappingSpans(const std::vector<Span> &spans, std::size_t begin, std::size_t end)
{
    const auto first = std::upper_bound(
        spans.begin(), spans.end(), begin,
        [](std::size_t pos, const Span &span) { return pos < span.end; });
    const auto after = std::lower_bound(
        first, spans.end(), end,
        [](const Span &span, std::size_t pos) { return span.begin < pos; });
    return std::pair{first, after};
}

std::uint32_t readCharacter(std::string_view text, std::size_t &pos)
{
    const auto first = static_cast<unsigned char>(text[pos++]);
    if (first < 0x80) return first;

    std::uint32_t value;
    unsigned remaining;
    std::uint32_t minimum;
    if ((first & 0xe0) == 0xc0) {
        value = first & 0x1f;
        remaining = 1;
        minimum = 0x80;
    } else if ((first & 0xf0) == 0xe0) {
        value = first & 0x0f;
        remaining = 2;
        minimum = 0x800;
    } else if ((first & 0xf8) == 0xf0) {
        value = first & 0x07;
        remaining = 3;
        minimum = 0x10000;
    } else {
        return 0xfffd;
    }

    if (remaining > text.size() - pos) return 0xfffd;
    for (unsigned i = 0; i < remaining; ++i) {
        const auto next = static_cast<unsigned char>(text[pos + i]);
        if ((next & 0xc0) != 0x80) return 0xfffd;
        value = (value << 6) | (next & 0x3f);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        return 0xfffd;
    }
    pos += remaining;
    return value;
}

std::string_view withoutStar(std::string_view name)
{
    if (!name.empty() && name.back() == '*') name.remove_suffix(1);
    return name;
}

bool mathEnvironment(std::string_view name)
{
    return contains(withoutStar(name), {
        "align", "alignat", "aligned", "alignedat", "array", "Bmatrix", "bmatrix",
        "cases", "CD", "dcases", "dmath", "displaymath", "dseries",
        "empheq", "eqnarray", "equation", "flalign", "gather", "gathered",
        "IEEEeqnarray", "IEEEeqnarraybox", "math", "matrix", "multline", "multlined", "numcases",
        "pmatrix", "rcases", "smallmatrix", "split", "subarray", "subnumcases",
        "Vmatrix", "vmatrix", "xalignat", "xxalignat"
    });
}

bool rawEnvironment(std::string_view name)
{
    return contains(withoutStar(name), {
        "BVerbatim", "LVerbatim", "Verbatim", "alltt", "comment", "filecontents",
        "listingcode", "lstlisting", "minted", "semiverbatim", "verbatim",
        "verbatimtab"
    });
}

bool citationCommand(std::string_view name)
{
    constexpr std::string_view names[] = {
        "autocite", "autocites", "bibentry", "cite", "citealp", "citealt", "citeauthor",
        "citedate", "citefield", "citefullauthor", "citelist", "citename", "citenum", "citep",
        "cites", "citet", "citetext", "citetitle", "citeurl", "citeyear", "citeyearpar",
        "footcite", "footcites", "footcitetext", "fullcite", "nocite", "parencite",
        "parencites", "pnotecite", "smartcite", "smartcites", "supercite", "supercites",
        "textcite", "textcites", "volcite", "volcites", "pvolcite", "fvolcite",
        "ftvolcite", "svolcite", "tvolcite", "avolcite"
    };
    for (const auto candidate : names) {
        if (std::equal(name.begin(), name.end(), candidate.begin(), candidate.end(),
                       [](char actual, char lower) {
            return actual == lower || (actual >= 'A' && actual <= 'Z' && actual - 'A' + 'a' == lower);
        })) return true;
    }
    return false;
}

bool formattingCommand(std::string_view name)
{
    return contains(name, {
        "emph", "text", "textbf", "textit", "textmd", "textnormal", "textrm",
        "textsc", "textsf", "textsl", "texttt", "textup", "underline", "uline",
        "uuline", "uwave", "sout", "xout", "mbox", "hbox", "fbox",
        "MakeUppercase", "MakeLowercase", "uppercase", "lowercase",
        "protect", "relax", "begingroup", "endgroup",
        "bf", "bfseries", "em", "it", "itshape", "mdseries", "normalfont", "rm",
        "rmfamily", "sc", "scshape", "sf", "sffamily", "sl", "slshape", "tt",
        "ttfamily", "upshape", "tiny", "scriptsize", "footnotesize", "small",
        "normalsize", "large", "Large", "LARGE", "huge", "Huge"
    });
}

// The csquotes variants put language and citation arguments before the text.
int quoteMetadataArguments(std::string_view name)
{
    if (contains(name, {"enquote", "textquote", "blockquote"})) return 0;
    if (contains(name, {
            "foreignquote", "hyphenquote", "foreigntextquote", "hyphentextquote",
            "foreignblockquote", "hyphenblockquote", "hybridblockquote",
            "textcquote", "blockcquote"
        })) return 1;
    if (contains(name, {
            "foreigntextcquote", "hyphentextcquote", "foreignblockcquote",
            "hyphenblockcquote", "hybridblockcquote"
        })) return 2;
    return -1;
}

bool accentCommand(std::string_view name)
{
    return contains(name, {
        "'", "`", "\"", "^", "~", "=", ".", "b", "c", "d", "H",
        "k", "r", "t", "u", "v"
    });
}

bool textLetterCommand(std::string_view name)
{
    return contains(name, {
        "AA", "aa", "AE", "ae", "DH", "dh", "DJ", "dj", "i", "IJ", "ij",
        "j", "L", "l", "NG", "ng", "O", "o", "OE", "oe", "SS", "ss", "TH", "th"
    });
}

} // namespace

class WordIndex::Scanner {
public:
    Scanner(std::string_view text, const WordCountRules &rules,
            std::vector<LetterRun> &letters, std::vector<HanSpan> &han)
        : text_(text), rules_(rules), letters_(letters), han_(han) {}

    void scan()
    {
        while (pos_ < text_.size()) {
            switch (text_[pos_]) {
            case '%':
                endWord();
                skipComment(pos_);
                break;
            case '$': {
                endWord();
                const std::string_view delimiter = startsAt(pos_, "$$") ? "$$" : "$";
                pos_ += delimiter.size();
                skipMath(delimiter);
                break;
            }
            case '\\':
                scanCommand();
                break;
            case '{':
                ++depth_;
                ++pos_;
                break;
            case '}':
                ++pos_;
                if (!textGroups_.empty() && textGroups_.back().depth == depth_) {
                    const auto after = textGroups_.back().after;
                    textGroups_.pop_back();
                    if (after == AfterTextGroup::SkipArgument) {
                        skipArguments(1);
                    } else {
                        endWord();
                    }
                }
                if (depth_ > 0) --depth_;
                break;
            default:
                scanCharacter();
                break;
            }
        }
    }

private:
    enum class AfterTextGroup { EndWord, SkipArgument };

    struct TextGroup {
        std::size_t depth;
        AfterTextGroup after;
    };

    bool startsAt(std::size_t pos, std::string_view token) const
    {
        return text_.substr(pos, token.size()) == token;
    }

    std::string_view readCommand(std::size_t &pos) const
    {
        const auto start = ++pos;
        if (pos == text_.size()) return {};
        if (!asciiLetter(text_[pos]) && text_[pos] != '@') {
            readCharacter(text_, pos);
        } else {
            do {
                ++pos;
            } while (pos < text_.size() && (asciiLetter(text_[pos]) || text_[pos] == '@'));
        }
        return text_.substr(start, pos - start);
    }

    void skipComment(std::size_t &pos) const
    {
        while (pos < text_.size() && text_[pos] != '\n' && text_[pos] != '\r') ++pos;
    }

    std::size_t triviaEnd(std::size_t pos) const
    {
        while (pos < text_.size()) {
            if (space(text_[pos])) {
                ++pos;
            } else if (text_[pos] == '%') {
                skipComment(pos);
            } else {
                break;
            }
        }
        return pos;
    }

    bool skipGroup(std::size_t &pos, char opening) const
    {
        const auto start = triviaEnd(pos);
        if (start == text_.size() || text_[start] != opening) return false;

        const bool optional = opening != '{';
        const char closing = opening == '[' ? ']' : ')';
        std::size_t braces = optional ? 0 : 1;
        std::size_t delimiters = optional ? 1 : 0;
        pos = start + 1;
        while (pos < text_.size()) {
            const char c = text_[pos];
            if (c == '\\') {
                readCommand(pos);
                continue;
            }
            if (c == '%') {
                skipComment(pos);
                continue;
            }

            ++pos;
            if (c == '{') {
                ++braces;
            } else if (c == '}' && braces > 0) {
                --braces;
                if (!optional && braces == 0) return true;
            } else if (optional && braces == 0 && c == opening) {
                ++delimiters;
            } else if (optional && braces == 0 && c == closing && --delimiters == 0) {
                return true;
            }
        }
        return true;
    }

    void skipOptions()
    {
        while (skipGroup(pos_, '[')) {}
    }

    void skipArguments(unsigned count)
    {
        for (unsigned i = 0; i < count; ++i) {
            skipOptions();
            if (!skipGroup(pos_, '{')) break;
        }
    }

    void positionAtTextGroup()
    {
        const auto start = triviaEnd(pos_);
        if (start < text_.size() && text_[start] == '{') pos_ = start;
    }

    void afterTextGroup(AfterTextGroup action)
    {
        positionAtTextGroup();
        if (pos_ < text_.size() && text_[pos_] == '{') {
            textGroups_.push_back({depth_ + 1, action});
        }
    }

    void skipMath(std::string_view close)
    {
        while (pos_ < text_.size()) {
            if (startsAt(pos_, close)) {
                pos_ += close.size();
                return;
            }
            if (text_[pos_] == '%') {
                skipComment(pos_);
            } else if (text_[pos_] == '\\') {
                readCommand(pos_);
            } else {
                ++pos_;
            }
        }
    }

    std::string_view readEnvironment(std::size_t &pos) const
    {
        const auto open = triviaEnd(pos);
        if (open == text_.size() || text_[open] != '{') return {};
        auto close = open + 1;
        while (close < text_.size()) {
            const auto c = text_[close];
            if (c == '}') {
                pos = close + 1;
                return text_.substr(open + 1, close - open - 1);
            }
            if (!(asciiLetter(c) || (c >= '0' && c <= '9')
                  || c == '*' || c == '@' || c == '-' || c == '_')) break;
            ++close;
        }
        pos = close;
        return {};
    }

    void skipEnvironment(std::string_view name, bool raw)
    {
        if (raw) {
            std::string close = "\\end{";
            close.append(name);
            close += '}';
            const auto end = text_.find(close, pos_);
            pos_ = end == std::string_view::npos ? text_.size() : end + close.size();
            return;
        }

        std::size_t depth = 1;
        while (pos_ < text_.size()) {
            if (text_[pos_] == '%') {
                skipComment(pos_);
            } else if (text_[pos_] != '\\') {
                ++pos_;
            } else {
                const auto command = readCommand(pos_);
                if (command == "begin" && readEnvironment(pos_) == name) {
                    ++depth;
                } else if (command == "end" && readEnvironment(pos_) == name && --depth == 0) {
                    return;
                }
            }
        }
    }

    void scanEnvironment(std::string_view command)
    {
        endWord();
        const auto name = readEnvironment(pos_);
        if (name == "document") {
            if (command == "end") {
                pos_ = text_.size();
            } else if (!inDocument_) {
                inDocument_ = true;
                letters_.clear();
                han_.clear();
                word_ = 0;
            }
            return;
        }
        if (command != "begin" || name.empty()) return;

        if (rawEnvironment(name)) {
            skipEnvironment(name, true);
        } else if (mathEnvironment(name) || contains(name, {
                       "thebibliography", "tikzpicture", "asy", "pspicture"
                   }) || std::any_of(rules_.ignoredEnvironments.begin(), rules_.ignoredEnvironments.end(),
                                    [name](const auto &rule) { return std::string_view(rule) == name; })) {
            skipEnvironment(name, false);
        } else {
            skipOptions();
            if (contains(withoutStar(name), {"tabularx", "tabulary"}) || name == "tabular*") {
                skipArguments(2);
            } else if (contains(name, {"tabular", "longtable", "minipage", "wrapfigure"})) {
                skipArguments(name == "wrapfigure" ? 2 : 1);
            }
        }
    }

    void skipInlineVerbatim(std::string_view command)
    {
        if (command == "mintinline") {
            skipArguments(1);
            if (skipGroup(pos_, '{')) return;
        } else if (command == "lstinline" || command == "Verb") {
            skipOptions();
            if (command == "lstinline" && skipGroup(pos_, '{')) return;
        }
        if (pos_ == text_.size() || space(text_[pos_])) return;
        const char delimiter = text_[pos_++];
        while (pos_ < text_.size() && text_[pos_] != '\n' && text_[pos_] != '\r') {
            if (text_[pos_++] == delimiter) return;
        }
    }

    void skipDefinition(std::string_view command)
    {
        if (contains(command, {"def", "edef", "gdef", "xdef"})) {
            while (pos_ < text_.size() && text_[pos_] != '{'
                   && text_[pos_] != '\n' && text_[pos_] != '\r') {
                if (text_[pos_] == '\\') {
                    readCommand(pos_);
                } else if (text_[pos_] == '%') {
                    skipComment(pos_);
                } else {
                    ++pos_;
                }
            }
            skipGroup(pos_, '{');
            return;
        }

        skipOptions();
        const auto name = triviaEnd(pos_);
        if (name < text_.size() && text_[name] == '\\') {
            pos_ = name;
            readCommand(pos_);
        } else {
            skipGroup(pos_, '{');
        }
        skipArguments(contains(command, {"newenvironment", "renewenvironment"}) ? 2 : 1);
    }

    void skipFalseBranch()
    {
        std::size_t depth = 1;
        while (pos_ < text_.size()) {
            if (text_[pos_] == '%') {
                skipComment(pos_);
            } else if (text_[pos_] != '\\') {
                ++pos_;
            } else {
                const auto command = readCommand(pos_);
                if (command.size() >= 2 && command.substr(0, 2) == "if") {
                    ++depth;
                } else if ((command == "fi" && --depth == 0)
                           || (command == "else" && depth == 1)) {
                    return;
                }
            }
        }
    }

    bool scanTextCommand(std::string_view name)
    {
        if (accentCommand(name) || textLetterCommand(name) || formattingCommand(name)
            || contains(name, {"/", "-", "@"})) {
            // A control word consumes following spaces. A control symbol does
            // so only when it is an accent command looking for its argument.
            if (accentCommand(name) || (name.size() > 1 || (name.size() == 1 && asciiLetter(name[0])))) {
                while (pos_ < text_.size() && space(text_[pos_])) ++pos_;
            }
            return true;
        }

        const auto quoteMetadata = quoteMetadataArguments(name);
        if (quoteMetadata >= 0) {
            endWord();
            skipArguments(static_cast<unsigned>(quoteMetadata));
            skipOptions();
            afterTextGroup(AfterTextGroup::EndWord);
            return true;
        }

        unsigned ignored = 0;
        if (contains(name, {"href", "hyperlink", "hypertarget", "foreignlanguage", "textcolor",
                            "colorbox", "rotatebox", "scalebox", "raisebox"})) {
            ignored = 1;
        } else if (contains(name, {"fcolorbox", "resizebox", "multicolumn", "multirow"})) {
            ignored = 2;
        }

        if (ignored > 0 || contains(name, {"hyperref", "texorpdfstring", "textls"})) {
            skipArguments(ignored);
            skipOptions();
            if (name == "texorpdfstring") {
                afterTextGroup(AfterTextGroup::SkipArgument);
            } else {
                positionAtTextGroup();
            }
            return true;
        }

        if (contains(name, {"part", "chapter", "section", "subsection", "subsubsection",
                            "paragraph", "subparagraph", "caption", "captionof", "footnote",
                            "footnotetext", "thanks", "marginpar", "item", "abstract",
                            "subcaption", "addchap", "addsec", "frametitle", "framesubtitle"})) {
            endWord();
            if (name == "captionof") skipArguments(1);
            skipOptions();
            if (name != "item") afterTextGroup(AfterTextGroup::EndWord);
            return true;
        }
        return false;
    }

    void scanCommand()
    {
        const auto name = readCommand(pos_);
        const bool controlWord = !name.empty() && asciiLetter(name.front());
        if (controlWord && pos_ < text_.size() && text_[pos_] == '*') ++pos_;
        if (name == "begin" || name == "end") {
            scanEnvironment(name);
            return;
        }

        const auto rule = std::find_if(rules_.commands.begin(), rules_.commands.end(),
                                       [name](const auto &entry) { return std::string_view(entry.name) == name; });
        if (rule != rules_.commands.end()) {
            endWord();
            skipArguments(rule->metadataArguments);
            skipOptions();
            return;
        }
        if (scanTextCommand(name)) return;
        endWord();

        if (name == "(" || name == "[") {
            skipMath(name == "(" ? "\\)" : "\\]");
        } else if (name == "iffalse") {
            skipFalseBranch();
        } else if (contains(name, {"verb", "Verb", "lstinline", "mintinline"})) {
            skipInlineVerbatim(name);
        } else if (contains(name, {"def", "edef", "gdef", "xdef", "newcommand", "renewcommand",
                                   "providecommand", "DeclareRobustCommand", "newenvironment",
                                   "renewenvironment"})) {
            skipDefinition(name);
        } else if (citationCommand(name)) {
            // BibLaTeX multicites may have two global notes before [notes]{keys} pairs.
            if (name.back() == 's' || name.back() == 'S') {
                if (skipGroup(pos_, '(')) skipGroup(pos_, '(');
                do {
                    skipOptions();
                } while (skipGroup(pos_, '{'));
            } else {
                skipArguments(contains(name, {"citefield", "citelist", "citename", "volcite",
                                               "pvolcite", "fvolcite", "ftvolcite", "svolcite",
                                               "tvolcite", "avolcite"}) ? 2 : 1);
            }
        } else if (contains(name, {
                       "documentclass", "documentstyle", "usepackage", "RequirePackage",
                       "author", "date", "title", "subtitle", "institute", "affiliation",
                       "address", "email", "orcid", "label", "ref", "pageref", "eqref",
                       "autoref", "autopageref", "vref", "vpageref", "cref", "Cref", "cpageref",
                       "Cpageref", "labelcref", "labelcpageref", "nameref", "subref",
                       "url", "nolinkurl", "path", "index", "gls", "Gls", "glspl", "Glspl",
                       "acrshort", "acrlong", "acrfull", "bibliography", "bibliographystyle",
                       "addbibresource", "include", "includeonly", "input", "subfile", "subfileinclude",
                       "includegraphics", "includepdf", "lstinputlisting", "ensuremath", "boxed",
                       "hspace", "vspace", "phantom", "hphantom", "vphantom", "color",
                       "geometry", "hypersetup", "captionsetup", "lstset", "setlist",
                       "hyphenation", "pagestyle", "thispagestyle", "bibliographyunit",
                       "usebibresource", "setstretch", "symbol", "verbinput", "stepcounter",
                       "refstepcounter", "newcounter", "fontfamily", "fontencoding", "fontseries",
                       "fontshape", "fontspec", "setmainfont", "setsansfont", "setmonofont",
                       "selectlanguage", "graphicspath", "DeclareGraphicsExtensions",
                       "num", "si", "unit", "ang", "numlist", "numproduct", "sisetup"
                   })) {
            skipArguments(1);
        } else if (contains(name, {
                       "setlength", "addtolength", "settowidth", "settoheight", "settodepth",
                       "setcounter", "addtocounter", "counterwithin",
                       "counterwithout", "DeclareMathOperator", "crefrange", "Crefrange",
                       "cpagerefrange", "Cpagerefrange", "newtheorem", "setkeys",
                       "setkomafont", "addtokomafont", "AtBeginEnvironment", "AtEndEnvironment",
                       "SI", "qty", "numrange", "SIlist", "qtylist", "SIproduct", "qtyproduct"
                   })) {
            skipArguments(2);
        } else if (contains(name, {"crefname", "Crefname", "crefformat", "Crefformat", "definecolor",
                                   "DeclareDocumentCommand", "NewDocumentCommand",
                                   "RenewDocumentCommand", "ProvideDocumentCommand", "SIrange", "qtyrange"})) {
            skipArguments(3);
        } else if (contains(name, {"printbibliography", "printglossary", "printindex",
                                   "linebreak", "pagebreak", "nopagebreak", "footnotemark"})
                   || name == "\\") {
            skipOptions();
        }
    }

    void scanCharacter()
    {
        const auto start = pos_;
        const auto c = readCharacter(text_, pos_);
        if (latinLetter(c)) {
            if (!hasLetters_) {
                ++word_;
                hasLetters_ = true;
            }
            inToken_ = true;
            pendingJoiner_ = false;
            if (!letters_.empty() && letters_.back().word == word_
                && letters_.back().end == start) {
                letters_.back().end = pos_;
            } else {
                letters_.push_back({start, pos_, word_});
            }
        } else if (hanIdeograph(c)) {
            endWord();
            han_.push_back({start, pos_});
        } else if (c >= '0' && c <= '9') {
            inToken_ = true;
            pendingJoiner_ = false;
        } else if (c >= 0x0300 && c <= 0x036f && inToken_ && !pendingJoiner_) {
            // A combining accent keeps the word together without creating a
            // countable letter on its own.
        } else if ((c == '\'' || c == 0x2019 || c == 0x02bc || c == '-'
                    || c == 0x2010 || c == 0x2011)
                   && inToken_ && !pendingJoiner_) {
            pendingJoiner_ = true;
        } else {
            endWord();
        }
    }

    void endWord()
    {
        hasLetters_ = false;
        inToken_ = false;
        pendingJoiner_ = false;
    }

    std::string_view text_;
    const WordCountRules &rules_;
    std::vector<LetterRun> &letters_;
    std::vector<HanSpan> &han_;
    std::vector<TextGroup> textGroups_;
    std::size_t pos_ = 0;
    std::size_t depth_ = 0;
    std::size_t word_ = 0;
    bool hasLetters_ = false;
    bool inToken_ = false;
    bool pendingJoiner_ = false;
    bool inDocument_ = false;
};

WordIndex::WordIndex(std::string_view text, const WordCountRules &rules) : textSize_(text.size())
{
    Scanner(text, rules, letters_, han_).scan();
}

std::size_t WordIndex::count(std::size_t selectionBegin, std::size_t selectionEnd) const
{
    selectionEnd = std::min(selectionEnd, textSize_);
    if (selectionBegin >= selectionEnd) return 0;

    const auto [first, after] = overlappingSpans(letters_, selectionBegin, selectionEnd);
    if (first == after) return 0;
    return (after - 1)->word - first->word + 1;
}

WordCounts WordIndex::counts(std::size_t selectionBegin, std::size_t selectionEnd) const
{
    selectionEnd = std::min(selectionEnd, textSize_);
    if (selectionBegin >= selectionEnd) return {};

    const auto [first, after] = overlappingSpans(han_, selectionBegin, selectionEnd);
    return {count(selectionBegin, selectionEnd), static_cast<std::size_t>(after - first)};
}

std::size_t countWords(std::string_view text, std::size_t selectionBegin, std::size_t selectionEnd)
{
    return WordIndex(text).count(selectionBegin, selectionEnd);
}

} // namespace texstudio_wordcount
