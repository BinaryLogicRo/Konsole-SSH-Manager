#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;

namespace KParts
{
class ReadOnlyPart;
}

// One tab: one konsolepart instance running `ssh <alias>`.
//
// The part deletes itself when ssh exits; sessionFinished() is emitted then and
// the owner should remove and delete the tab. If ssh fails, the terminal stays
// open and the owner calls showFailure(), which offers to retry or close.
class TerminalTab : public QWidget
{
    Q_OBJECT

public:
    // Loads a new Konsole part and starts ssh. Returns nullptr (and a
    // user-readable `errorMessage`) if Konsole can't be loaded. `alias` must
    // already be validated.
    static TerminalTab *create(const QString &alias, QWidget *parent, QString *errorMessage);
    ~TerminalTab() override;

    QString alias() const;
    // Process id of the session helper, 0 if there is none.
    qint64 sessionProcessId() const;
    void focusTerminal();
    // Shows the Retry and Close Tab buttons above the terminal.
    void showFailure();

Q_SIGNALS:
    void sessionFinished(TerminalTab *tab);
    void closeRequested(TerminalTab *tab);

private:
    TerminalTab(const QString &alias, QWidget *parent);
    // Starts a new session. On retry, it replaces the old terminal only once the new one is running.
    bool startSession(QString *errorMessage);
    void retry();
    void onPartDestroyed();

    QString m_alias;
    QPointer<KParts::ReadOnlyPart> m_part;
    QWidget *m_failureBar = nullptr;
    QLabel *m_failureLabel = nullptr;
};
