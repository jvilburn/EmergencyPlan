#pragma once

#include <QObject>

#include "DocumentChange.h"
#include "EmergencyResponse.h"

/// Read access to the open ward's emergency in progress, and the signals the views refresh
/// on. Every change to the emergency is a command the views build and commit through
/// DocumentManager; this class turns the document's changes into the signals, each emitted
/// after its own state is up to date.
class EmergencyManager : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyManager(QObject* parent);

    /// Global accessor. Valid after construction.
    static EmergencyManager* instance();

    /// True while the ward has an emergency in progress.
    bool isActive() const { return m_active; }

    /// The emergency in progress, as the document holds it. Only while isActive().
    const EmergencyResponse& response() const;

    /// The statistics of the emergency in progress over the directory's families; 0 when
    /// none is in progress.
    int totalFamilies() const;
    int countByStatus(EffectiveContactStatus status) const;

signals:
    /// The ward went from no emergency in progress to one.
    void emergencyStarted();

    /// The ward went from an emergency in progress to none.
    void emergencyEnded();

    /// The statistics may have changed: the counts, the progress bar and the map.
    void responseDataChanged();

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    bool m_active = false;

    static EmergencyManager* s_instance;
};
