#include "ShareNotice.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

ShareNotice::ShareNotice(QWidget* parent)
    : QFrame(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);

    m_label = new QLabel(this);
    m_button = new QPushButton(this);

    layout->addWidget(m_label, 1);
    layout->addWidget(m_button);

    setStyleSheet(
        "ShareNotice {"
        "  background-color: #FFF3CD;"
        "}"
        "ShareNotice QLabel {"
        "  color: #333;"
        "}");
    hide();

    connect(m_button, &QPushButton::clicked, this, &ShareNotice::onButtonClicked);
}

void ShareNotice::showNoLongerShared()
{
    m_offersDelete = true;
    m_label->setText(tr("This ward is no longer shared with this device."));
    m_button->setText(tr("Delete this copy"));
    show();
}

void ShareNotice::showChangesToShare()
{
    m_offersDelete = false;
    m_label->setText(tr("You have changes to share"));
    m_button->setText(tr("Send Update"));
    show();
}

void ShareNotice::clearNotice()
{
    hide();
}

void ShareNotice::onButtonClicked()
{
    if (m_offersDelete)
    {
        emit deleteCopyRequested();
        return;
    }
    emit sendUpdateRequested();
}
