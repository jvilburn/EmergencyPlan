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
class OpenWardDialog : public QDialog
{
    Q_OBJECT

public:
    OpenWardDialog(const QList<DocumentLibrary::Entry>& entries, QWidget* parent);

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
