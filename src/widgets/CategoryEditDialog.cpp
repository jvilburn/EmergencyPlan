#include "CategoryEditDialog.h"

#include <QLineEdit>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QIcon>

CategoryEditDialog::CategoryEditDialog(
    const QString& title,
    const QString& currentName,
    std::optional<MarkerDecorationType> currentType,
    QWidget* parent)
    : QDialog(parent)
    , m_nameEdit(nullptr)
    , m_decorationCombo(nullptr)
{
    setWindowTitle(title);
    setupUi(currentName, currentType);
}

void CategoryEditDialog::setupUi(
    const QString& currentName,
    std::optional<MarkerDecorationType> currentType)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Form layout for name and decoration type
    QFormLayout* formLayout = new QFormLayout();

    m_nameEdit = new QLineEdit();
    m_nameEdit->setText(currentName);
    formLayout->addRow(tr("Name:"), m_nameEdit);

    m_decorationCombo = new QComboBox();
    m_decorationCombo->addItem(QIcon(":/markers/marker_home.svg"), tr("None"));
    m_decorationCombo->addItem(QIcon(":/markers/marker_medical.svg"), tr("Medical"));
    m_decorationCombo->addItem(QIcon(":/markers/marker_recovery.svg"), tr("Recovery"));
    m_decorationCombo->addItem(QIcon(":/markers/marker_antenna.svg"), tr("Communications"));

    // Set current index based on currentType
    int index = 0;
    if (currentType.has_value())
    {
        switch (currentType.value())
        {
        case MarkerDecorationType::Medical:
            index = 1;
            break;
        case MarkerDecorationType::Recovery:
            index = 2;
            break;
        case MarkerDecorationType::Communications:
            index = 3;
            break;
        default:
            index = 0;
            break;
        }
    }
    m_decorationCombo->setCurrentIndex(index);

    formLayout->addRow(tr("Marker:"), m_decorationCombo);
    mainLayout->addLayout(formLayout);

    // OK/Cancel buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Set focus to name field
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

CategoryEditDialog::Result CategoryEditDialog::result() const
{
    Result r;
    r.name = m_nameEdit->text().trimmed();

    int index = m_decorationCombo->currentIndex();
    switch (index)
    {
    case 1:
        r.decorationType = MarkerDecorationType::Medical;
        break;
    case 2:
        r.decorationType = MarkerDecorationType::Recovery;
        break;
    case 3:
        r.decorationType = MarkerDecorationType::Communications;
        break;
    default:
        r.decorationType = std::nullopt;
        break;
    }

    return r;
}

std::optional<CategoryEditDialog::Result> CategoryEditDialog::getCategory(
    QWidget* parent,
    const QString& title,
    const QString& currentName,
    std::optional<MarkerDecorationType> currentType)
{
    CategoryEditDialog dialog(title, currentName, currentType, parent);

    if (dialog.exec() == QDialog::Accepted)
    {
        Result r = dialog.result();
        if (!r.name.isEmpty())
        {
            return r;
        }
    }

    return std::nullopt;
}
