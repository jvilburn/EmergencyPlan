#pragma once

#include <QObject>
#include <optional>

#include "Document.h"
#include "EmergencyResponse.h"

class DocumentManager;
struct DocumentChange;

class EmergencyManager : public QObject
{
    Q_OBJECT

public:
    explicit EmergencyManager(DocumentManager* documentManager, QObject* parent);

    // Lifecycle
    bool isActive() const;
    void startEmergency(const QString& name);
    void endEmergency(bool archive);

    // Archive viewing (read-only mode)
    bool loadArchive(const QString& filePath, QString* errorMessage = nullptr);
    void closeArchive();
    bool isViewingArchive() const;

    // Reopen archived emergency as active
    bool reopenArchive(const QString& filePath, QString* errorMessage = nullptr);

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
    void archiveViewOpened();
    void archiveViewClosed();
    void familyStatusChanged(const FamilyId& familyId);
    void responseDataChanged();  // generic "something changed" for progress bars, counts

private slots:
    void onDocumentChanged(const DocumentChange& change);

private:
    void persistResponseData();
    bool saveArchive();
    void syncFromDocument();
    void syncFamilies();  // add records for new families, called on document changes

    DocumentManager* m_documentManager;
    std::optional<EmergencyResponse> m_response;

    // Archive viewing state
    std::optional<EmergencyResponse> m_savedResponse;  // stashed live response during archive view
};
