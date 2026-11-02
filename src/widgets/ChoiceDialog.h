#pragma once

#include <QList>
#include <QPair>
#include <QString>

#include <optional>

class QWidget;

/// A dialog that asks for one of a list of names, each carrying an id.
namespace ChoiceDialog
{

/// Asks the user to choose one of choices, each a name and the id it carries, listed in
/// order. The chosen one's id, or none when the user cancelled.
std::optional<QString> choose(QWidget* parent, const QString& title, const QString& label,
                              const QList<QPair<QString, QString>>& choices);

}  // namespace ChoiceDialog
