#pragma once

#include "MarkerDecorationType.h"

#include <QDialog>
#include <QString>
#include <optional>

class QLineEdit;
class QComboBox;

/// Dialog for adding or editing a category (Skill or Equipment).
/// Provides name field and decoration type dropdown with marker icons.
class CategoryEditDialog : public QDialog
{
    Q_OBJECT

public:
    struct Result
    {
        QString name;
        std::optional<MarkerDecorationType> decorationType;
    };

    /// Show dialog to add or edit a category.
    /// @param parent Parent widget
    /// @param title Dialog title (e.g., "Add Category" or "Edit Category")
    /// @param currentName Current name (empty for new category)
    /// @param currentType Current decoration type (nullopt for new category)
    /// @return Result if OK was clicked, nullopt if cancelled
    static std::optional<Result> getCategory(
        QWidget* parent,
        const QString& title,
        const QString& currentName = QString(),
        std::optional<MarkerDecorationType> currentType = std::nullopt);

private:
    explicit CategoryEditDialog(
        const QString& title,
        const QString& currentName,
        std::optional<MarkerDecorationType> currentType,
        QWidget* parent);

    void setupUi(const QString& currentName, std::optional<MarkerDecorationType> currentType);
    Result result() const;

    QLineEdit* m_nameEdit;
    QComboBox* m_decorationCombo;
};
