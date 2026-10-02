#include "hosteditdialog.h"
#include "models/hosttreemodel.h"
#include "sshconfig/sshkeywords.h"
#include "sshconfig/sshresolver.h"

#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <KLocalizedString>

namespace
{
enum OptionColumn {
    KeywordColumn = 0,
    ValueColumn = 1,
};

// Suggests OpenSSH 9.2 keywords while typing in the keyword column.
class KeywordDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto *lineEdit = qobject_cast<QLineEdit *>(editor)) {
            auto *completer = new QCompleter(SshKeywords::offered(), lineEdit);
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            lineEdit->setCompleter(completer);
        }
        return editor;
    }
};

bool isStructuralKeyword(const QString &keyword)
{
    return keyword.compare(QLatin1String("Host"), Qt::CaseInsensitive) == 0 || keyword.compare(QLatin1String("Match"), Qt::CaseInsensitive) == 0
        || keyword.compare(QLatin1String("Include"), Qt::CaseInsensitive) == 0;
}
}

HostEditDialog::HostEditDialog(const SshHost &host, const QStringList &groups, Validator validator, QWidget *parent)
    : QDialog(parent)
    , m_original(host)
    , m_validator(std::move(validator))
{
    m_externalChangeLabel = new QLabel(this);
    m_externalChangeLabel->setWordWrap(true);
    m_externalChangeLabel->setText(
        i18n("The configuration file was changed by another program while this dialog was open. "
             "When you save, you will be asked before anything is overwritten."));
    m_externalChangeLabel->setFrameShape(QFrame::StyledPanel);
    m_externalChangeLabel->setMargin(6);
    m_externalChangeLabel->hide();

    m_alias = new QLineEdit(this);
    m_alias->setPlaceholderText(i18nc("@info:placeholder", "e.g. prod-db"));
    m_hostName = new QLineEdit(this);
    m_hostName->setPlaceholderText(i18nc("@info:placeholder", "Address or DNS name (defaults to the alias)"));
    m_user = new QLineEdit(this);
    m_port = new QLineEdit(this);
    m_port->setPlaceholderText(i18nc("@info:placeholder", "22"));
    m_proxyJump = new QLineEdit(this);
    m_proxyJump->setPlaceholderText(i18nc("@info:placeholder", "e.g. bastion"));

    m_identityFile = new QLineEdit(this);
    m_identityFile->setPlaceholderText(i18nc("@info:placeholder", "e.g. ~/.ssh/id_ed25519"));
    auto *browseButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action:button", "Browse…"), this);
    auto *identityRow = new QHBoxLayout;
    identityRow->addWidget(m_identityFile);
    identityRow->addWidget(browseButton);

    m_group = new QComboBox(this);
    m_group->setEditable(true);
    // Combo boxes don't expand by default, and some styles only grow expanding form fields.
    m_group->setSizePolicy(QSizePolicy::Expanding, m_group->sizePolicy().verticalPolicy());
    m_group->addItem(QString());
    m_group->addItems(groups);

    m_colorButton = new QPushButton(this);
    auto *clearColorButton = new QToolButton(this);
    clearColorButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    clearColorButton->setToolTip(i18nc("@info:tooltip", "Remove color"));
    auto *colorRow = new QHBoxLayout;
    colorRow->addWidget(m_colorButton);
    colorRow->addWidget(clearColorButton);
    colorRow->addStretch();

    m_note = new QLineEdit(this);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "Alias:"), m_alias);
    form->addRow(i18nc("@label:textbox", "Host name:"), m_hostName);
    form->addRow(i18nc("@label:textbox", "User:"), m_user);
    form->addRow(i18nc("@label:textbox", "Port:"), m_port);
    form->addRow(i18nc("@label:textbox", "Identity file:"), identityRow);
    form->addRow(i18nc("@label:textbox", "Proxy jump:"), m_proxyJump);
    form->addRow(i18nc("@label:listbox", "Group:"), m_group);
    form->addRow(i18nc("@label", "Color:"), colorRow);
    form->addRow(i18nc("@label:textbox", "Note:"), m_note);

    m_options = new QTableWidget(0, 2, this);
    m_options->setHorizontalHeaderLabels({i18nc("@title:column", "Keyword"), i18nc("@title:column", "Value")});
    m_options->horizontalHeader()->setSectionResizeMode(KeywordColumn, QHeaderView::ResizeToContents);
    m_options->horizontalHeader()->setStretchLastSection(true);
    m_options->verticalHeader()->hide();
    m_options->setItemDelegateForColumn(KeywordColumn, new KeywordDelegate(m_options));
    auto *addOptionButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Add Option"), this);
    auto *removeOptionButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove Option"), this);
    auto *optionButtons = new QHBoxLayout;
    optionButtons->addWidget(addOptionButton);
    optionButtons->addWidget(removeOptionButton);
    optionButtons->addStretch();

    auto *optionsBox = new QGroupBox(i18nc("@title:group", "Additional Options"), this);
    auto *optionsLayout = new QVBoxLayout(optionsBox);
    optionsLayout->addWidget(m_options);
    optionsLayout->addLayout(optionButtons);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_externalChangeLabel);
    layout->addLayout(form);
    layout->addWidget(optionsBox, 1);
    layout->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked, this, &HostEditDialog::chooseIdentityFile);
    connect(m_colorButton, &QPushButton::clicked, this, &HostEditDialog::chooseColor);
    connect(clearColorButton, &QToolButton::clicked, this, [this] {
        setColor(QString());
    });
    connect(addOptionButton, &QPushButton::clicked, this, [this] {
        addOptionRow(QString(), QString());
        m_options->setCurrentCell(m_options->rowCount() - 1, KeywordColumn);
        m_options->editItem(m_options->item(m_options->rowCount() - 1, KeywordColumn));
    });
    connect(removeOptionButton, &QPushButton::clicked, this, [this] {
        if (m_options->currentRow() >= 0) {
            m_options->removeRow(m_options->currentRow());
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &HostEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &HostEditDialog::reject);

    loadHost(host);
    resize(560, 620);
}

void HostEditDialog::loadHost(const SshHost &host)
{
    m_alias->setText(host.alias());
    const QList<QPair<QString, QLineEdit *>> fields{
        {QStringLiteral("HostName"), m_hostName},
        {QStringLiteral("User"), m_user},
        {QStringLiteral("Port"), m_port},
        {QStringLiteral("IdentityFile"), m_identityFile},
        {QStringLiteral("ProxyJump"), m_proxyJump},
    };
    QSet<QString> filled;
    for (const SshOption &option : host.options) {
        bool consumed = false;
        for (const auto &field : fields) {
            if (!filled.contains(field.first) && option.keyword.compare(field.first, Qt::CaseInsensitive) == 0) {
                field.second->setText(option.value);
                filled.insert(field.first);
                consumed = true;
                break;
            }
        }
        if (!consumed) {
            addOptionRow(option.keyword, option.value);
        }
    }
    m_group->setCurrentText(host.group());
    m_note->setText(host.note());
    setColor(host.color());
}

SshHost HostEditDialog::host() const
{
    SshHost result = m_original;
    result.kind = SshHost::Kind::Host;
    result.readOnly = false;

    const QString alias = m_alias->text().trimmed();
    const qsizetype aliasIndex = result.patterns.indexOf(m_original.alias());
    if (aliasIndex >= 0 && !m_original.alias().isEmpty()) {
        result.patterns[aliasIndex] = alias;
    } else {
        result.patterns = QStringList{alias};
    }

    result.options.clear();
    const QList<QPair<QString, QLineEdit *>> fields{
        {QStringLiteral("HostName"), m_hostName},
        {QStringLiteral("User"), m_user},
        {QStringLiteral("Port"), m_port},
        {QStringLiteral("IdentityFile"), m_identityFile},
        {QStringLiteral("ProxyJump"), m_proxyJump},
    };
    for (const auto &field : fields) {
        const QString value = field.second->text().trimmed();
        if (!value.isEmpty()) {
            result.options.append({field.first, value});
        }
    }
    for (int row = 0; row < m_options->rowCount(); ++row) {
        const QTableWidgetItem *keywordItem = m_options->item(row, KeywordColumn);
        const QTableWidgetItem *valueItem = m_options->item(row, ValueColumn);
        const QString keyword = keywordItem ? keywordItem->text().trimmed() : QString();
        const QString value = valueItem ? valueItem->text().trimmed() : QString();
        if (!keyword.isEmpty() || !value.isEmpty()) {
            result.options.append({keyword, value});
        }
    }

    result.setMetadataValue(QStringLiteral("group"), m_group->currentText().trimmed());
    result.setMetadataValue(QStringLiteral("color"), m_color);
    result.setMetadataValue(QStringLiteral("note"), m_note->text().trimmed());
    return result;
}

void HostEditDialog::showExternalChangeWarning()
{
    m_externalChangeLabel->show();
}

void HostEditDialog::accept()
{
    const SshHost edited = host();
    const QString error = validationError(edited);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, i18nc("@title:window", "Invalid Host"), error);
        return;
    }
    QDialog::accept();
}

QString HostEditDialog::validationError(const SshHost &host) const
{
    if (!SshValidation::isConcretePattern(m_alias->text().trimmed())) {
        return i18n("The alias must not be empty, start with \"-\", or contain spaces, control characters, quotes, \"#\", \"\\\" or wildcards.");
    }
    for (const SshOption &option : host.options) {
        if (!SshValidation::isValidKeyword(option.keyword) || isStructuralKeyword(option.keyword)) {
            return i18n("\"%1\" is not a valid option keyword. Keywords contain only letters and digits; Host, Match and Include are not allowed here.",
                        option.keyword);
        }
        if (!SshValidation::isValidValue(option.value)) {
            return i18n("The value of %1 must not be empty or contain control characters.", option.keyword);
        }
    }

    // Let OpenSSH itself judge the entry, so rejected keywords surface its own message.
    const SshCommandResult check = SshResolver::validateHost(host);
    if (check.started && !check.timedOut && check.exitCode != 0) {
        const QString details = QString::fromUtf8(check.standardError).trimmed();
        return i18n("OpenSSH rejected this entry:\n\n%1", details.isEmpty() ? i18n("(no error message)") : details);
    }

    return m_validator ? m_validator(host) : QString();
}

void HostEditDialog::addOptionRow(const QString &keyword, const QString &value)
{
    const int row = m_options->rowCount();
    m_options->insertRow(row);
    m_options->setItem(row, KeywordColumn, new QTableWidgetItem(keyword));
    m_options->setItem(row, ValueColumn, new QTableWidgetItem(value));
}

void HostEditDialog::chooseIdentityFile()
{
    const QString sshDir = QDir::homePath() + QStringLiteral("/.ssh");
    QString path = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Select Private Key"), sshDir);
    if (path.isEmpty()) {
        return;
    }
    const QString home = QDir::homePath();
    if (path.startsWith(home + QLatin1Char('/'))) {
        path = QLatin1Char('~') + path.mid(home.size());
    }
    if (path.contains(QLatin1Char(' '))) {
        path = QLatin1Char('"') + path + QLatin1Char('"');
    }
    m_identityFile->setText(path);
}

void HostEditDialog::chooseColor()
{
    const QColor current(m_color);
    const QColor chosen = QColorDialog::getColor(current.isValid() ? current : QColor(Qt::gray), this, i18nc("@title:window", "Host Color"));
    if (chosen.isValid()) {
        setColor(chosen.name());
    }
}

void HostEditDialog::setColor(const QString &color)
{
    m_color = color;
    const QIcon icon = HostTreeModel::colorIcon(color);
    m_colorButton->setIcon(icon);
    m_colorButton->setText(icon.isNull() ? i18nc("@action:button no color set", "Choose…") : color);
}
