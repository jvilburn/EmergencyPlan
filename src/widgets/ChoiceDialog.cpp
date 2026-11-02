#include "ChoiceDialog.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

std::optional<QString> ChoiceDialog::choose(QWidget* parent, const QString& title, const QString& label,
                                            const QList<QPair<QString, QString>>& choices)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(label, &dialog));

    QComboBox* combo = new QComboBox(&dialog);
    for (const QPair<QString, QString>& choice : choices)
    {
        combo->addItem(choice.first, choice.second);
    }
    layout->addWidget(combo);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || combo->currentIndex() < 0)
    {
        return std::nullopt;
    }
    return combo->currentData().toString();
}
