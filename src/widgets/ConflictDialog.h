#pragma once

#include "DocumentSession.h"

#include <QDialog>

/// Someone else saved the directory while this device's changes to it were
/// unpublished, and the two cannot both be kept. Names who and when, and offers
/// three ways forward; the first, the default, destroys nothing.
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

    ConflictDialog(const DocumentSession::Conflict& conflict, QWidget* parent);

    /// SaveMineAsCopy unless another was chosen; closing the dialog chooses nothing
    /// destructive.
    Choice choice() const { return m_choice; }

private slots:
    void onKeepTheirs();
    void onKeepMine();

private:
    Choice m_choice = Choice::SaveMineAsCopy;
};
