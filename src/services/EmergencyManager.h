#pragma once

#include <QObject>
#include <optional>

#include "Document.h"
#include "EmergencyResponse.h"

struct DocumentChange;

class EmergencyManager : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyManager(QObject* parent);

    /// Global accessor. Valid after construction.
    static EmergencyManager* instance();

    // Lifecycle
    bool isActive() const;
    void startEmergency(const QString& name);
    void endEmergency();

    // Read access
    const EmergencyResponse& response() const;
    const FamilyResponseRecord* recordForFamily(const FamilyId& familyId) const;
    EffectiveContactStatus familyStatus(const FamilyId& familyId) const;

    // Contact status
    void setContactStatus(const FamilyId& familyId, ContactStatus status);

    // Contact attempts
    void addContactAttempt(const FamilyId& familyId, const ContactAttempt& attempt);
    void removeContactAttempt(const FamilyId& familyId, const ContactAttemptId& attemptId);

    // Tasks
    void addTask(const FamilyId& familyId, const ResponseTask& task);
    void updateTask(const FamilyId& familyId, const ResponseTask& task);
    void removeTask(const FamilyId& familyId, const TaskId& taskId);
    void resolveTask(const FamilyId& familyId, const TaskId& taskId, const QString& notes);
    void reopenTask(const FamilyId& familyId, const TaskId& taskId);

    // Task assignment
    void assignTaskToTeam(const FamilyId& familyId, const TaskId& taskId, const TeamId& teamId, const QString& notes);
    void assignTaskToPerson(const FamilyId& familyId, const TaskId& taskId, const PersonId& personId, const QString& notes);
    void unassignTask(const FamilyId& familyId, const TaskId& taskId);

    // Task notification
    void notifyAssignee(const FamilyId& familyId, const TaskId& taskId, const TaskNotification& notification);

    // Task categories
    QStringList taskCategories() const;
    void addTaskCategory(const QString& category);

    // Statistics
    int totalFamilies() const;
    int countByStatus(EffectiveContactStatus status) const;

signals:
    void emergencyStarted();
    void emergencyEnded();
    void familyStatusChanged(const FamilyId& familyId);
    void responseDataChanged();  // generic "something changed" for progress bars, counts

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void syncFamilies();  // add records for new families, called on document changes

    std::optional<EmergencyResponse> m_response;

    static EmergencyManager* s_instance;
};
