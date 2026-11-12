// SPDX-License-Identifier: GPL-2.0-or-later
#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QDeadlineTimer>
#include <QDebug>
#include <QEventLoop>
#include <QFileInfo>
#include <QGenericPlugin>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QTimer>

namespace {

QString filename(QObject *object, const char *method)
{
    QString name;
    QMetaObject::invokeMethod(object, method, Qt::DirectConnection, Q_RETURN_ARG(QString, name));
    return name.isEmpty() ? QString() : QFileInfo(name).canonicalFilePath();
}

template<class Predicate>
bool waitFor(Predicate ready)
{
    if (ready())
        return true;
    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (ready())
            loop.quit();
    });
    poll.start(15);
    QTimer::singleShot(4000, &loop, &QEventLoop::quit);
    loop.exec();
    return ready();
}

bool check(bool condition, const char *step)
{
    if (!condition)
        qCritical().noquote() << "native word-count smoke:" << step;
    return condition;
}

QLabel *countLabel(QMainWindow *window)
{
    return window->findChild<QLabel *>(QStringLiteral("selectionWordCountLabel"));
}

bool expectCount(QMainWindow *window, int english, int chinese, const char *step)
{
    const auto expected = QStringLiteral("English %1 \u00b7 \u6c49\u5b57 %2 \u00b7 Total %3")
                              .arg(english).arg(chinese).arg(english + chinese);
    if (!waitFor([&] {
            const auto *label = countLabel(window);
            return label && label->isVisible() && label->text() == expected;
        })) {
        const auto *label = countLabel(window);
        qCritical().noquote() << "native word-count smoke:" << step
                              << "expected" << expected << "got" << (label ? label->text() : QString())
                              << "panel width" << (label && label->parentWidget() ? label->parentWidget()->width() : 0)
                              << "window width" << window->width();
        return false;
    }
    qInfo().noquote() << "native word-count smoke:" << step << "passed";
    return true;
}

void key(QWidget *widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent press(QEvent::KeyPress, code, modifiers);
    QCoreApplication::sendEvent(widget, &press);
    QKeyEvent release(QEvent::KeyRelease, code, modifiers);
    QCoreApplication::sendEvent(widget, &release);
}

void goToLine(QWidget *editor, int line)
{
    key(editor, Qt::Key_Home, Qt::ControlModifier);
    for (int i = 0; i < line; ++i)
        key(editor, Qt::Key_Down);
    key(editor, Qt::Key_Home);
}

bool activate(QMainWindow *window, const QString &file)
{
    if (filename(window, "getCurrentFileName") == file)
        return true;
    if (!QMetaObject::invokeMethod(window, "gotoNextDocument", Qt::DirectConnection))
        return false;
    return waitFor([&] { return filename(window, "getCurrentFileName") == file; });
}

bool suppressAccessibleText(QWidget *editor)
{
    if (auto *previous = QAccessible::queryAccessibleInterface(editor))
        QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(previous));
    // Reproduce the live Windows editor exposing a generic widget instead
    // of an interface with accessible text.
    QAccessible::registerAccessibleInterface(new QAccessibleWidget(editor));
    auto *cached = QAccessible::queryAccessibleInterface(editor);
    return cached && !cached->textInterface();
}

bool verify(QMainWindow *window, QStatusBar *status,
            QWidget *first, QWidget *second, const QString &firstFile, const QString &secondFile)
{
    // The application may restore a small default geometry on a fresh profile.
    window->resize(1400, 760);
    if (!check(activate(window, firstFile), "cannot activate primary tab")
        || !expectCount(window, 0, 0, "startup without selection"))
        return false;
    const auto *label = countLabel(window);
    if (!check(label->parentWidget()
                   && label->parentWidget()->objectName() == QStringLiteral("statusPanel")
                   && label->parentWidget()->inherits("QStatusPanel"),
               "label is not inside the editor Line/Column/Selected/INSERT panel"))
        return false;

    const auto message = QStringLiteral("native smoke message");
    status->showMessage(message);
    goToLine(first, 4);
    key(first, Qt::Key_End, Qt::ShiftModifier);
    if (!expectCount(window, 3, 0, "keyboard selection")
        || !check(label->isVisible() && status->currentMessage() == message,
                  "existing status bar message changed or obscured the label"))
        return false;
    status->clearMessage();

    goToLine(first, 9);
    for (int i = 0; i < 6; ++i)
        key(first, Qt::Key_Right);
    key(first, Qt::Key_End, Qt::ShiftModifier);
    if (!expectCount(window, 2, 5, "mixed English, simplified and traditional Han after Unicode prefix"))
        return false;
    goToLine(first, 9);
    for (int i = 0; i < 6; ++i)
        key(first, Qt::Key_Right);
    key(first, Qt::Key_Right, Qt::ShiftModifier);
    if (!expectCount(window, 0, 1, "supplementary Han selected with keyboard"))
        return false;

    if (!check(QMetaObject::invokeMethod(first, "selectNothing"), "clear selection failed")
        || !expectCount(window, 0, 0, "clear selection")
        || !check(QMetaObject::invokeMethod(first, "selectAll"), "primary selectAll failed")
        || !expectCount(window, 22, 7, "full bilingual LaTeX fixture"))
        return false;

    if (!check(activate(window, secondFile), "cannot activate secondary tab")
        || !expectCount(window, 0, 0, "switch to unselected tab")
        || !check(QMetaObject::invokeMethod(second, "selectAll"), "secondary selectAll failed")
        || !expectCount(window, 1, 2, "secondary tab selection"))
        return false;
    if (!check(activate(window, firstFile), "cannot return to primary tab")
        || !expectCount(window, 22, 7, "restore primary tab selection"))
        return false;

    QString edited;
    const auto original = QStringLiteral("A careful result.");
    const auto replacement = QStringLiteral("Alpha \u65b0\u8bba\u6587 here.  ");
    if (!check(QMetaObject::invokeMethod(first, "text", Q_RETURN_ARG(QString, edited))
               && edited.contains(original) && replacement.size() == original.size(),
               "cannot obtain the expected edit fixture"))
        return false;
    // Keep UTF-16 length unchanged so reusing the previous index cannot pass.
    edited.replace(original, replacement);
    if (!check(first->metaObject()->indexOfSlot("setText(QString)") >= 0
               && QMetaObject::invokeMethod(first, "setText", Q_ARG(QString, edited)),
               "stock QEditor setText(QString) slot failed")
        || !check(QMetaObject::invokeMethod(first, "selectAll"), "selectAll after editing failed"))
        return false;
    if (!expectCount(window, 21, 10, "selection reindexed after in-memory edit")
        || !check(suppressAccessibleText(first), "could not simulate missing accessibility text")
        || !check(QMetaObject::invokeMethod(first, "selectNothing"), "clear selection failed")
        || !expectCount(window, 0, 0, "clear selection without accessibility text")
        || !check(QMetaObject::invokeMethod(first, "selectAll"), "selectAll without accessibility failed"))
        return false;
    return expectCount(window, 21, 10, "selection counted without accessibility text");
}

class Observer final : public QObject {
public:
    Observer()
        : firstFile(QFileInfo(qEnvironmentVariable("TEXSTUDIO_WORDCOUNT_GUI_SMOKE")).canonicalFilePath()),
          secondFile(QFileInfo(qEnvironmentVariable("TEXSTUDIO_WORDCOUNT_GUI_SMOKE_SECOND")).canonicalFilePath())
    {
        connect(&starter, &QTimer::timeout, this, [this] { startWhenReady(); });
        starter.start(35);
    }

private:
    void startWhenReady()
    {
        for (auto *top : QApplication::topLevelWidgets()) {
            auto *window = qobject_cast<QMainWindow *>(top);
            if (!window || !window->inherits("Texstudio") || !window->isVisible())
                continue;
            auto *status = window->findChild<QStatusBar *>();
            if (!status)
                continue;
            auto *label = window->findChild<QLabel *>(QStringLiteral("selectionWordCountLabel"));
            if (!label)
                continue;

            QWidget *first = nullptr;
            QWidget *second = nullptr;
            for (auto *child : window->findChildren<QWidget *>()) {
                if (!child->inherits("QEditor"))
                    continue;
                const auto file = filename(child, "fileName");
                if (!file.isEmpty() && file == firstFile)
                    first = child;
                else if (!file.isEmpty() && file == secondFile)
                    second = child;
            }
            if (!first || !second)
                continue;

            starter.stop();
            const bool passed = verify(window, status, first, second, firstFile, secondFile);
            if (passed)
                qInfo() << "native word-count GUI smoke passed";
            QCoreApplication::exit(passed ? 0 : 1);
            return;
        }
        if (deadline.hasExpired()) {
            starter.stop();
            qCritical() << "native word-count smoke: could not find two fixture tabs and a label";
            QCoreApplication::exit(1);
        }
    }

    const QString firstFile;
    const QString secondFile;
    QTimer starter;
    QDeadlineTimer deadline{15000};
};

} // namespace

class SelectionWordCountSmokePlugin final : public QGenericPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QGenericPluginFactoryInterface_iid FILE "native_selection_smoke.json")
public:
    QObject *create(const QString &key, const QString &) override
    {
        if (key.compare(QStringLiteral("texstudio-wordcount-smoke"), Qt::CaseInsensitive) != 0
            || !qEnvironmentVariableIsSet("TEXSTUDIO_WORDCOUNT_GUI_SMOKE"))
            return nullptr;
        return new Observer;
    }
};

#include "native_selection_smoke.moc"
