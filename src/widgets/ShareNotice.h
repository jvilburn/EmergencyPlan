#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

/// The bar across the window that says what to do about sharing the open ward: that it is no
/// longer shared with this device, with Delete this copy; that it has changes to share, with
/// Send Update; or what stops its sharing through the server, with nothing to press.
class ShareNotice : public QFrame
{
    Q_OBJECT

public:
    explicit ShareNotice(QWidget* parent);

    /// Shows that the ward is no longer shared with this device, offering to delete this copy.
    void showNoLongerShared();

    /// Shows that there are changes to share, offering Send Update.
    void showChangesToShare();

    /// Shows message, with no button.
    void showMessage(const QString& message);

    void clearNotice();

signals:
    void sendUpdateRequested();
    void deleteCopyRequested();

private slots:
    void onButtonClicked();

private:
    QLabel* m_label = nullptr;
    QPushButton* m_button = nullptr;
    bool m_offersDelete = false;  ///< the button deletes this copy; otherwise it sends an update
};
