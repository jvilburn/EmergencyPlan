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
    ResponseArea currentType,
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
    ResponseArea currentType)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Form layout for name and decoration type
    QFormLayout* formLayout = new QFormLayout();

    m_nameEdit = new QLineEdit();
    m_nameEdit->setText(currentName);
    formLayout->addRow(tr("Name:"), m_nameEdit);

    m_decorationCombo = new QComboBox();

    // Store enum values as item data for type-safe retrieval
    m_decorationCombo->addItem(QIcon(":/markers/marker_home.svg"), tr("None"),
                               static_cast<int>(ResponseArea::None));
    m_decorationCombo->addItem(QIcon(":/markers/marker_medical.svg"), tr("Medical"),
                               static_cast<int>(ResponseArea::Medical));
    m_decorationCombo->addItem(QIcon(":/markers/marker_recovery.svg"), tr("Recovery"),
                               static_cast<int>(ResponseArea::Recovery));
    m_decorationCombo->addItem(QIcon(":/markers/marker_antenna.svg"), tr("Communications"),
                               static_cast<int>(ResponseArea::Communications));

    // Set current selection based on currentType
    int index = m_decorationCombo->findData(static_cast<int>(currentType));
    if (index >= 0)
    {
        m_decorationCombo->setCurrentIndex(index);
    }

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
    r.decorationType = static_cast<ResponseArea>(
        m_decorationCombo->currentData().toInt());
    return r;
}

std::optional<CategoryEditDialog::Result> CategoryEditDialog::getCategory(
    QWidget* parent,
    const QString& title,
    const QString& currentName,
    ResponseArea currentType)
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
