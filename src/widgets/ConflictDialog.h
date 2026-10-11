#pragma once

#include "DocumentSession.h"

#include <QDialog>

/// Someone else saved the directory while this device's changes to it were
/// unpublished, and the two cannot both be kept. Names who and when, and offers
/// three ways forward, each with what it costs; the first, the default, destroys
/// nothing. Closing it sets the choice aside: nothing changes, nothing uploads,
/// and File > Choose Between Two Versions asks again.
///
/// The roster is already settled by the time this opens: the session took the
/// other side's.
class ConflictDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Choice
    {
        SaveMineAsCopy,     ///< theirs is kept, and mine saved as a copy to look at
        KeepTheirs,
        KeepMine
    };

    /// error, when not empty, is why the last choice could not be carried out.
    ConflictDialog(const DocumentSession::Conflict& conflict, const QString& error, QWidget* parent);

    /// Meaningful once the dialog is accepted.
    Choice choice() const { return m_choice; }

private slots:
    void onSaveMineAsCopy();
    void onKeepTheirs();
    void onKeepMine();

private:
    void addOption(const QString& button, const QString& consequence, void (ConflictDialog::*chosen)(), bool isDefault);

    Choice m_choice = Choice::SaveMineAsCopy;
};
