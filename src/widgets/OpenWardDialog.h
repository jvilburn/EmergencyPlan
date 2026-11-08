#pragma once

#include "Id.h"
#include "WardList.h"

#include <QDialog>
#include <QList>

class QListWidget;
class QPushButton;

/// What the user chose in the ward list.
enum class WardListChoice
{
    Open,     ///< open the chosen ward
    NewWard,  ///< create a ward
    Close     ///< nothing
};

/// The ward list: the wards this device holds, by name, with the ones whose changes are
/// waiting marked, and a ward whose file does not read listed by why, which cannot be chosen.
class OpenWardDialog : public QDialog
{
    Q_OBJECT

public:
    OpenWardDialog(const QList<WardListEntry>& wards, QWidget* parent);

    /// Shows the list until the user chooses.
    WardListChoice choose();

    /// The ward chosen to open, when choose answered Open.
    DocumentId chosenWard() const;

    /// What the user is told when a ward does not open, for each way it can fail.
    static QString openFailureMessage(OpenStatus status);

private slots:
    void onNewWard();
    void updateOpenButton();

private:
    /// The result exec answers when the user chooses New Ward.
    static constexpr int NewWardResult = 2;

    QListWidget* m_list;
    QPushButton* m_openButton;
};
