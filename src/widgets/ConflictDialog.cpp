#include "ConflictDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

ConflictDialog::ConflictDialog(const DocumentSession::Conflict& conflict, const QString& error, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Two Versions of the Directory"));
    setMinimumWidth(500);

    QVBoxLayout* layout = new QVBoxLayout(this);

    if (!error.isEmpty())
    {
        QLabel* failed = new QLabel(tr("That could not be done: %1").arg(error), this);
        failed->setWordWrap(true);
        layout->addWidget(failed);
    }

    const QString when = QLocale().toString(conflict.writeTime.toLocalTime(), QLocale::ShortFormat);
    QLabel* what = new QLabel(tr("The directory was changed by %1 at %2 while your changes to it were not yet "
                                 "uploaded. You both changed the directory, so only one version of it can be kept.")
                                  .arg(conflict.writerName, when),
                              this);
    what->setWordWrap(true);
    layout->addWidget(what);

    QLabel* responses = new QLabel(tr("Emergency responses from both of you are kept whichever you choose."), this);
    responses->setWordWrap(true);
    layout->addWidget(responses);

    addOption(tr("Keep Theirs, Save Mine as a Copy"),
              tr("Their directory is kept. Yours is saved as a copy in your Documents folder to look at."),
              &ConflictDialog::onSaveMineAsCopy,
              true);
    addOption(tr("Keep Theirs"),
              tr("Their directory is kept. Your changes to it since your last upload are discarded."),
              &ConflictDialog::onKeepTheirs,
              false);
    addOption(tr("Keep Mine"),
              tr("Your directory is kept, and replaces their changes to it for everyone."),
              &ConflictDialog::onKeepMine,
              false);

    QDialogButtonBox* later = new QDialogButtonBox(this);
    later->addButton(tr("Decide Later"), QDialogButtonBox::RejectRole);
    layout->addWidget(later);
    connect(later, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ConflictDialog::addOption(const QString& button,
                               const QString& consequence,
                               void (ConflictDialog::*chosen)(),
                               bool isDefault)
{
    QPushButton* choose = new QPushButton(button, this);
    choose->setDefault(isDefault);
    QLabel* cost = new QLabel(consequence, this);
    cost->setWordWrap(true);
    cost->setContentsMargins(12, 0, 0, 8);

    layout()->addWidget(choose);
    layout()->addWidget(cost);
    connect(choose, &QPushButton::clicked, this, chosen);
}

void ConflictDialog::onSaveMineAsCopy()
{
    m_choice = Choice::SaveMineAsCopy;
    accept();
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
