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

    // Store enum values as item data for type-safe retrieval
    m_decorationCombo->addItem(QIcon(":/markers/marker_home.svg"), tr("None"),
                               static_cast<int>(MarkerDecorationType::None));
    m_decorationCombo->addItem(QIcon(":/markers/marker_medical.svg"), tr("Medical"),
                               static_cast<int>(MarkerDecorationType::Medical));
    m_decorationCombo->addItem(QIcon(":/markers/marker_recovery.svg"), tr("Recovery"),
                               static_cast<int>(MarkerDecorationType::Recovery));
    m_decorationCombo->addItem(QIcon(":/markers/marker_antenna.svg"), tr("Communications"),
                               static_cast<int>(MarkerDecorationType::Communications));

    // Set current selection based on currentType (nullopt maps to None)
    int dataValue = currentType.has_value()
                    ? static_cast<int>(currentType.value())
                    : static_cast<int>(MarkerDecorationType::None);
    int index = m_decorationCombo->findData(dataValue);
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

    MarkerDecorationType type = static_cast<MarkerDecorationType>(
        m_decorationCombo->currentData().toInt());

    // None maps to nullopt (current design uses optional)
    if (type == MarkerDecorationType::None)
    {
        r.decorationType = std::nullopt;
    }
    else
    {
        r.decorationType = type;
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
