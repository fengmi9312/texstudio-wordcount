// SPDX-License-Identifier: GPL-2.0-or-later

#include "latexwordcounter.h"

#include <QApplication>
#include <QByteArray>
#include <QEvent>
#include <QFontMetrics>
#include <QFutureWatcher>
#include <QGenericPlugin>
#include <QLabel>
#include <QMainWindow>
#include <QMetaMethod>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QtConcurrentRun>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using texstudio_wordcount::WordCounts;
using texstudio_wordcount::WordIndex;

constexpr auto labelName = "selectionWordCountLabel";

QObject *invokeObjectGetter(QObject *object, const char *signature)
{
    const auto *meta = object->metaObject();
    const int index = meta->indexOfMethod(signature);
    if (index < 0) return nullptr;

    const QMetaMethod method = meta->method(index);
    const QMetaType resultType = method.returnMetaType();
    if (!resultType.flags().testFlag(QMetaType::PointerToQObject)) return nullptr;

    // Keep the actual stock return type in QVariant storage, then convert
    // through Qt's QObject pointer conversion without linking to that type.
    QVariant result(resultType);
    if (!method.invoke(object, Qt::DirectConnection,
                       QGenericReturnArgument(method.typeName(), result.data()))) {
        return nullptr;
    }
    return result.value<QObject *>();
}

QObject *findDescendant(QObject *object, const char *className)
{
    const auto descendants = object->findChildren<QObject *>();
    const auto match = std::find_if(descendants.begin(), descendants.end(), [className](QObject *child) {
        return child->inherits(className);
    });
    return match == descendants.end() ? nullptr : *match;
}

struct Position {
    int line = -1;
    int column = -1;
};

struct Selection {
    Position start;
    Position end;
    int caretLine = 0;
    int visualColumn = 0;
    bool hasText = false;
};

bool readSelection(QObject *editor, Selection &selection)
{
    QVariant value = editor->property("cursor");
    if (!value.isValid() || qstrcmp(value.metaType().name(), "QDocumentCursor") != 0) {
        return false;
    }

    // In TeXstudio 4.9.8 QDocumentCursor is a copyable class whose only
    // base is QObject. QVariant keeps the reflected cursor copy alive.
    auto *cursor = static_cast<QObject *>(value.data());
    if (!cursor || !cursor->inherits("QDocumentCursor")
        || !QMetaObject::invokeMethod(cursor, "hasSelection", Qt::DirectConnection,
                                      Q_RETURN_ARG(bool, selection.hasText))) {
        return false;
    }
    QMetaObject::invokeMethod(cursor, "lineNumber", Qt::DirectConnection,
                              Q_RETURN_ARG(int, selection.caretLine));
    QMetaObject::invokeMethod(cursor, "visualColumnNumber", Qt::DirectConnection,
                              Q_RETURN_ARG(int, selection.visualColumn));
    if (!selection.hasText) return true;

    return QMetaObject::invokeMethod(cursor, "startLineNumber", Qt::DirectConnection,
                                    Q_RETURN_ARG(int, selection.start.line))
        && QMetaObject::invokeMethod(cursor, "startColumnNumber", Qt::DirectConnection,
                                     Q_RETURN_ARG(int, selection.start.column))
        && QMetaObject::invokeMethod(cursor, "endLineNumber", Qt::DirectConnection,
                                     Q_RETURN_ARG(int, selection.end.line))
        && QMetaObject::invokeMethod(cursor, "endColumnNumber", Qt::DirectConnection,
                                     Q_RETURN_ARG(int, selection.end.column));
}

struct DocumentIndex {
    std::vector<std::size_t> lineStarts;
    std::vector<std::size_t> utf8Offsets;
    WordIndex words;

    std::size_t utf16Offset(Position position) const
    {
        if (position.line < 0 || position.column < 0) return 0;
        const auto line = static_cast<std::size_t>(position.line);
        if (line >= lineStarts.size()) return utf8Offsets.size() - 1;

        const auto start = lineStarts[line];
        const auto end = line + 1 < lineStarts.size()
            ? lineStarts[line + 1] - 1 : utf8Offsets.size() - 1;
        return start + std::min(static_cast<std::size_t>(position.column), end - start);
    }

    std::size_t offset(Position position) const
    {
        return utf8Offsets[utf16Offset(position)];
    }
};

using Index = std::shared_ptr<const DocumentIndex>;

Index indexText(const QStringList &lines)
{
    std::vector<std::size_t> lineStarts;
    lineStarts.reserve(static_cast<std::size_t>(lines.size()));
    std::size_t lineStart = 0;
    for (const auto &line : lines) {
        lineStarts.push_back(lineStart);
        lineStart += static_cast<std::size_t>(line.size()) + 1;
    }
    const QString source = lines.join(QLatin1Char('\n'));
    const QByteArray utf8 = source.toUtf8();
    std::vector<std::size_t> offsets(static_cast<std::size_t>(source.size()) + 1);

    std::size_t bytes = 0;
    for (qsizetype i = 0; i < source.size(); ++i) {
        offsets[static_cast<std::size_t>(i)] = bytes;
        const auto unit = source.at(i);
        const auto value = unit.unicode();
        if (unit.isHighSurrogate() && i + 1 < source.size() && source.at(i + 1).isLowSurrogate()) {
            // A selection through either half overlaps the same four-byte code point.
            offsets[static_cast<std::size_t>(++i)] = bytes + 2;
            bytes += 4;
        } else if (unit.isSurrogate()) {
            bytes += static_cast<std::size_t>(QString(unit).toUtf8().size());
        } else {
            bytes += value < 0x80 ? 1 : value < 0x800 ? 2 : 3;
        }
    }
    offsets.back() = bytes;

    return std::make_shared<const DocumentIndex>(DocumentIndex{
        std::move(lineStarts),
        std::move(offsets),
        WordIndex(std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size())))
    });
}

class SelectionWordCount final : public QObject {
    Q_OBJECT

public:
    explicit SelectionWordCount(QMainWindow *window)
        : QObject(window), window_(window)
    {
        refreshTimer_.setSingleShot(true);
        refreshTimer_.setInterval(40);
        connect(&refreshTimer_, &QTimer::timeout, this, &SelectionWordCount::refresh);
        connect(&worker_, &QFutureWatcher<Index>::finished, this, [this] {
            building_ = false;
            if (pendingRevision_ == revision_) index_ = worker_.result();
            if (!refreshTimer_.isActive()) refresh();
        });

        const auto editorsSignal = connectSignal(findDescendant(window, "Editors"),
                                                 "currentEditorChanged()", "bindEditor()",
                                                 Qt::QueuedConnection);
        if (!editorsSignal) {
            connect(window, &QWidget::windowTitleChanged, this, &SelectionWordCount::bindEditor,
                    Qt::QueuedConnection);
        }
        bindEditor();
    }

    ~SelectionWordCount() override
    {
        if (building_) worker_.waitForFinished();
        delete label_.data();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == panel_.data()) {
            switch (event->type()) {
            case QEvent::Show:
            case QEvent::Resize:
            case QEvent::FontChange:
            case QEvent::LanguageChange:
                placeLabel();
                break;
            case QEvent::ChildAdded:
            case QEvent::ChildRemoved:
                // The stock panel adds a timer when modification age is shown.
                scheduleRefresh();
                break;
            default:
                break;
            }
        }
        return false;
    }

private Q_SLOTS:
    void bindEditor()
    {
        if (!window_) return;
        QObject *next = invokeObjectGetter(window_, "currentEditor()");
        if (!next) {
            // A fallback for builds that omit the concrete return metatype.
            auto *focus = window_->focusWidget();
            while (focus && !focus->inherits("QEditor")) focus = focus->parentWidget();
            next = focus;
        }
        if (next == editor_.data()) return;

        clearEditor();
        editor_ = next;
        if (!editor_) return;

        panel_ = editor_->findChild<QWidget *>(QStringLiteral("statusPanel"));
        if (panel_ && panel_->inherits("QStatusPanel")) {
            label_ = new QLabel(panel_);
            label_->setObjectName(QLatin1String(labelName));
            label_->setTextFormat(Qt::PlainText);
            label_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            label_->setAttribute(Qt::WA_TransparentForMouseEvents);
            panel_->installEventFilter(this);
            showCount({});
        }

        cursorConnection_ = connectSignal(editor_, "cursorPositionChanged()", "scheduleRefresh()");
        document_ = invokeObjectGetter(editor_, "document()");
        if (!document_) document_ = findDescendant(editor_, "QDocument");
        contentsConnection_ = connectSignal(document_, "contentsChanged()", "documentEdited()");
        destroyConnection_ = connect(editor_.data(), &QObject::destroyed, this, [this] {
            clearEditor();
            QTimer::singleShot(0, this, &SelectionWordCount::bindEditor);
        });
        scheduleRefresh();
    }

    void scheduleRefresh()
    {
        if (!refreshTimer_.isActive()) refreshTimer_.start();
    }

    void documentEdited()
    {
        ++revision_;
        index_.reset();
        // Let bursts of edits settle. Cursor movement is throttled separately.
        refreshTimer_.start();
    }

private:
    QMetaObject::Connection connectSignal(QObject *sender, const char *signal, const char *slot,
                                          Qt::ConnectionType type = Qt::AutoConnection)
    {
        if (!sender) return {};
        const auto *source = sender->metaObject();
        const auto *destination = metaObject();
        const int signalIndex = source->indexOfSignal(signal);
        const int slotIndex = destination->indexOfSlot(slot);
        if (signalIndex < 0 || slotIndex < 0) return {};
        return QObject::connect(sender, source->method(signalIndex), this,
                                destination->method(slotIndex), type);
    }

    void clearEditor()
    {
        disconnect(cursorConnection_);
        disconnect(contentsConnection_);
        disconnect(destroyConnection_);
        editor_.clear();
        document_.clear();
        if (panel_) panel_->removeEventFilter(this);
        delete label_.data();
        panel_.clear();
        selection_ = {};
        refreshTimer_.stop();
        ++revision_;
        index_.reset();
    }

    void refresh()
    {
        if (!editor_ || !label_) return;
        Selection selection;
        if (!readSelection(editor_, selection) || !document_) {
            setLabelText(QStringLiteral("Count unavailable"));
            return;
        }
        selection_ = selection;
        if (!selection.hasText) {
            showCount({});
            return;
        }
        if (!index_) {
            if (!building_) {
                QStringList lines;
                if (!QMetaObject::invokeMethod(document_, "textLines", Qt::DirectConnection,
                                               Q_RETURN_ARG(QStringList, lines))) {
                    setLabelText(QStringLiteral("Count unavailable"));
                    return;
                }

                // Only copy the editor's line strings on the UI thread.
                // Joining, UTF-8 conversion, and parsing run in the worker.
                building_ = true;
                pendingRevision_ = revision_;
                worker_.setFuture(QtConcurrent::run([lines = std::move(lines)] {
                    return indexText(lines);
                }));
            }
            setLabelText(QStringLiteral("Counting\u2026"));
            return;
        }

        showCount(index_->words.counts(index_->offset(selection.start),
                                       index_->offset(selection.end)));
    }

    void placeLabel()
    {
        if (!label_ || !panel_ || !editor_) return;
        const auto translate = [](const char *text, const char *comment = nullptr) {
            return QCoreApplication::translate("QStatusPanel", text, comment);
        };
        const auto &selection = selection_;
        const QString gap = QStringLiteral("    ");
        QString left = translate("Line: %1").arg(selection.caretLine + 1)
            + gap + translate("Column: %1").arg(selection.visualColumn);
        if (selection.hasText) {
            const int lineBreaks = std::max(0, selection.end.line - selection.start.line);
            std::size_t characters = 999999999;
            if (lineBreaks == 0) {
                characters = static_cast<std::size_t>(std::max(0, selection.end.column - selection.start.column));
            } else if (index_) {
                const auto start = index_->utf16Offset(selection.start);
                const auto end = index_->utf16Offset(selection.end);
                if (start <= end) {
                    const auto units = end - start;
                    characters = units - std::min(units, static_cast<std::size_t>(lineBreaks));
                }
            }
            left += gap + translate("Selected: %1").arg(static_cast<qulonglong>(characters));
            if (lineBreaks > 0) left += gap + translate("Lines: %1").arg(lineBreaks + 1);
        }
        // Reserve space for the optional RTL flag and the larger edit mode.
        left += gap + translate("RTL", "Abbreviation for Right-To-Left used in status bar.");
        const QFontMetrics metrics(panel_->font());
        int reserved = metrics.horizontalAdvance(left)
            + std::max(metrics.horizontalAdvance(translate("INSERT")),
                       metrics.horizontalAdvance(translate("OVERWRITE")))
            + 80; // Stock gaps, 16-pixel icon space, and a 4-pixel gap after the edit mode.
        if (panel_->findChild<QTimer *>(QString(), Qt::FindDirectChildrenOnly)) {
            reserved += metrics.horizontalAdvance(translate("(%1 min %2 s ago)").arg(99999999).arg(59));
        }

        bool conflict = false;
        QMetaObject::invokeMethod(editor_, "isInConflict", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, conflict));
        const int rightPadding = 4 + (conflict ? 46 + metrics.horizontalAdvance(translate("Conflict")) : 0);
        if (label_->font() != panel_->font()) label_->setFont(panel_->font());

        const int width = label_->sizeHint().width();
        if (width > panel_->width() - reserved - rightPadding) {
            label_->hide();
            return;
        }
        label_->setGeometry(reserved, 0, width, panel_->height());
        label_->show();
    }

    void setLabelText(const QString &text)
    {
        if (!label_) return;
        label_->setText(text);
        placeLabel();
    }

    void showCount(WordCounts counts)
    {
        const auto english = static_cast<qulonglong>(counts.englishWords);
        const auto chinese = static_cast<qulonglong>(counts.chineseCharacters);
        const auto total = static_cast<qulonglong>(counts.total());
        setLabelText(QStringLiteral("English %1 \u00B7 \u6C49\u5B57 %2 \u00B7 Total %3")
                         .arg(english).arg(chinese).arg(total));
    }

    QPointer<QMainWindow> window_;
    QPointer<QWidget> panel_;
    QPointer<QLabel> label_;
    Selection selection_;
    QPointer<QObject> editor_;
    QPointer<QObject> document_;
    QTimer refreshTimer_;
    QMetaObject::Connection cursorConnection_;
    QMetaObject::Connection contentsConnection_;
    QMetaObject::Connection destroyConnection_;
    QFutureWatcher<Index> worker_;
    Index index_;
    std::uint64_t revision_ = 0;
    std::uint64_t pendingRevision_ = 0;
    bool building_ = false;
};

class WindowDiscovery final : public QObject {
public:
    WindowDiscovery()
    {
        // Qt requests generic plugins while constructing QApplication.
        // Inspect widgets after construction rather than creating UI here.
        QCoreApplication::instance()->installEventFilter(this);
        QTimer::singleShot(0, this, [this] {
            for (auto *widget : QApplication::topLevelWidgets()) {
                attach(qobject_cast<QMainWindow *>(widget));
            }
        });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            auto *window = qobject_cast<QMainWindow *>(watched);
            if (window && window->inherits("Texstudio")) {
                const QPointer<QMainWindow> target(window);
                QTimer::singleShot(0, this, [this, target] { attach(target); });
            }
        }
        return false;
    }

private:
    void attach(QMainWindow *window)
    {
        if (window && window->inherits("Texstudio")
            && !window->findChild<SelectionWordCount *>()) {
            new SelectionWordCount(window);
        }
    }
};

class WordCountPlugin final : public QGenericPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QGenericPluginFactoryInterface_iid FILE "wordcountplugin.json")

public:
    QObject *create(const QString &key, const QString &specification) override
    {
        if (key.compare(QLatin1String("texstudio-wordcount"), Qt::CaseInsensitive) != 0
            || !specification.isEmpty()) {
            return nullptr;
        }
        return new WindowDiscovery;
    }
};

} // namespace

#include "wordcountplugin.moc"
