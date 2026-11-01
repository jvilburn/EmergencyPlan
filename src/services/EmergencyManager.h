#pragma once

#include <QList>
#include <QObject>
#include <optional>

#include "DocumentChange.h"
#include "EmergencyResponse.h"

/// Which of the open ward's emergencies in progress is shown, read access to it, and the
/// signals the views refresh on. Every change to a response is a command the views build and
/// commit through DocumentManager; this class turns the document's changes into the signals.
class EmergencyManager : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyManager(QObject* parent);

    /// Global accessor. Valid after construction.
    static EmergencyManager* instance();

    /// True while a response is shown, which is whenever the ward has one in progress.
    bool isActive() const;

    /// The shown response's id, or none when the ward has no emergency in progress.
    const std::optional<EmergencyResponseId>& shownId() const { return m_shownId; }

    /// The shown response, as the document holds it. Only while isActive().
    const EmergencyResponse& response() const;

    /// The ward's responses in progress, oldest first.
    QList<EmergencyResponse> responsesInProgress() const;

    /// Shows the response with that id, which the document holds.
    void show(const EmergencyResponseId& id);

    /// The family's status in the shown response, from the document's status table; Not
    /// Contacted when no response is shown.
    EffectiveContactStatus familyStatus(const FamilyId& familyId) const;

    /// The statistics of the shown response over the directory's families.
    int totalFamilies() const;
    int countByStatus(EffectiveContactStatus status) const;

signals:
    /// A response is shown that was not before: one started, one shown on opening a ward,
    /// or one switched to.
    void emergencyStarted();

    /// No response is shown any more.
    void emergencyEnded();

    /// The ward's responses in progress were added to or removed from, or one of them
    /// changed as a whole.
    void responsesChanged();

    /// One family's attempts or tasks in the shown response changed.
    void familyStatusChanged(const FamilyId& familyId);

    /// Something in the shown response changed, for progress bars and counts.
    void responseDataChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    /// Keeps the shown response when the document still holds it, otherwise shows the most
    /// recently started one in progress, or none, and emits what changed. True when the
    /// shown response changed.
    bool showCurrent();

    std::optional<EmergencyResponseId> m_shownId;

    static EmergencyManager* s_instance;
};
