#include "ConflictDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

ConflictDialog::ConflictDialog(const DocumentSession::Conflict& conflict, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Two Versions of the Directory"));
    setMinimumWidth(480);

    QVBoxLayout* layout = new QVBoxLayout(this);

    const QString when = QLocale().toString(conflict.writeTime.toLocalTime(), QLocale::ShortFormat);
    QLabel* what = new QLabel(tr("%1 saved changes to the directory at %2, while yours were not yet "
                                 "uploaded. Both changed the same things, so only one can be kept.")
                                  .arg(conflict.writerName, when),
                              this);
    what->setWordWrap(true);
    layout->addWidget(what);

    QLabel* responses = new QLabel(tr("Emergency responses from both of you are kept either way."), this);
    responses->setWordWrap(true);
    layout->addWidget(responses);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    QPushButton* saveCopy = buttons->addButton(tr("Keep Theirs, Save Mine as a Copy"), QDialogButtonBox::AcceptRole);
    QPushButton* keepTheirs = buttons->addButton(tr("Keep Theirs"), QDialogButtonBox::ActionRole);
    QPushButton* keepMine = buttons->addButton(tr("Keep Mine"), QDialogButtonBox::ActionRole);
    saveCopy->setDefault(true);
    layout->addWidget(buttons);

    QLabel* explained = new QLabel(tr("Keep Mine replaces their changes to the directory with yours for "
                                      "everyone."),
                                   this);
    explained->setWordWrap(true);
    layout->addWidget(explained);

    connect(saveCopy, &QPushButton::clicked, this, &QDialog::accept);
    connect(keepTheirs, &QPushButton::clicked, this, &ConflictDialog::onKeepTheirs);
    connect(keepMine, &QPushButton::clicked, this, &ConflictDialog::onKeepMine);
}

void ConflictDialog::onKeepTheirs()
{
    m_choice = Choice::KeepTheirs;
    accept();
}

void ConflictDialog::onKeepMine()
{
    m_choice = Choice::KeepMine;
    accept();
}
