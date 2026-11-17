#include "ServerWaitDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

ServerWaitDialog::ServerWaitDialog(ServerReading* reading, const QString& title, const QString& text,
                                   QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(text, this));
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    reading->setParent(this);
    connect(reading, &ServerReading::answered, this, &ServerWaitDialog::onAnswered);
}

void ServerWaitDialog::onAnswered(const ServerRead& read)
{
    m_answer = read;
    accept();
}
