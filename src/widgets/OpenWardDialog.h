#pragma once

#include "Id.h"
#include "WardList.h"

#include <QDialog>
#include <QList>

class QListWidget;
class QListWidgetItem;
class QPushButton;

/// What the user chose in the ward list.
enum class WardListChoice
{
    Open,            ///< open the chosen ward
    NewWard,         ///< create a ward
    OpenInvitation,  ///< open an invitation file
    Close            ///< nothing
};

/// The ward list: the wards this device holds, by name, with the ones no longer shared with
/// this device and the ones with changes to share marked, and a ward whose file does not read
/// listed by why, which cannot be chosen.
class OpenWardDialog : public QDialog
{
    Q_OBJECT

public:
    OpenWardDialog(const QList<WardListEntry>& wards, QWidget* parent);

    /// Shows the list until the user chooses.
    WardListChoice choose();

    /// The ward chosen to open, when choose answered Open.
    DocumentId chosenWard() const { return m_chosenWard; }

    /// What the user is told when a ward does not open, for each way it can fail.
    static QString openFailureMessage(OpenStatus status);

private slots:
    void onOpen();
    void onItemActivated(QListWidgetItem* item);
    void onNewWard();
    void onOpenInvitation();
    void updateOpenButton();

private:
    /// The results exec answers when the user chooses New Ward or Open Invitation.
    static constexpr int NewWardResult = 2;
    static constexpr int OpenInvitationResult = 3;

    /// Chooses the ward item names and closes the list with Open.
    void openItem(const QListWidgetItem* item);

    QListWidget* m_list;
    QPushButton* m_openButton;
    DocumentId m_chosenWard;
};
