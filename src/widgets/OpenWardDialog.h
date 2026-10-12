#pragma once

#include "DocumentLibrary.h"

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QListWidget;
class QPushButton;

/// The wards on this device, by name, each saying whether it is shared. Choosing
/// one - double-click, or select and Open - gives its document id.
///
/// Below them, the wards on their way: each this device asked for, and each shared
/// with this person that waits for a version sealed to this device. Neither can be
/// opened yet, and neither has a name here until it arrives.
class OpenWardDialog : public QDialog
{
    Q_OBJECT

public:
    OpenWardDialog(const QList<DocumentLibrary::Entry>& entries,
                   const QStringList& requested,
                   const QStringList& waiting,
                   QWidget* parent);

    /// Empty until a ward is chosen.
    QString selectedDocumentId() const { return m_selected; }

private slots:
    void onSelectionChanged();
    void onAccepted();

private:
    static constexpr int FetchRole = Qt::UserRole + 1;
    static constexpr int NameRole = Qt::UserRole + 2;

    QListWidget* m_list = nullptr;
    QPushButton* m_openButton = nullptr;
    QString m_selected;
};
