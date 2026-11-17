#pragma once

#include "Server.h"

#include <QDialog>
#include <QString>

/// A small dialog shown while one read from the server is awaited. It closes when the answer
/// arrives, and Cancel gives up waiting.
class ServerWaitDialog : public QDialog
{
    Q_OBJECT

public:
    /// Waits for reading's answer under text; reading becomes this dialog's child, so giving up
    /// drops its answer.
    ServerWaitDialog(ServerReading* reading, const QString& title, const QString& text, QWidget* parent);

    /// What the server answered, once the dialog was accepted.
    const ServerRead& answer() const { return m_answer; }

private slots:
    void onAnswered(const ServerRead& read);

private:
    ServerRead m_answer{ServerReadStatus::NoAnswer, QByteArray()};
};
