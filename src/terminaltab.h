#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

namespace KParts
{
class ReadOnlyPart;
}

// One tab: one konsolepart instance running `ssh <alias>`.
//
// The part deletes itself when ssh exits; sessionFinished() is emitted then and
// the owner should remove and delete the tab.
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
    void focusTerminal();

Q_SIGNALS:
    void sessionFinished(TerminalTab *tab);

private:
    TerminalTab(const QString &alias, QWidget *parent);
    bool loadPart(QString *errorMessage);
    void onPartDestroyed();

    QString m_alias;
    QPointer<KParts::ReadOnlyPart> m_part;
};
