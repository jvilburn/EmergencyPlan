#pragma once

#include "Command.h"
#include "EmergencyResource.h"

class AddEmergencyResourceCommand : public Command
{
public:
    explicit AddEmergencyResourceCommand(const EmergencyResource& resource);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyResource m_resource;
};

class UpdateEmergencyResourceCommand : public Command
{
public:
    UpdateEmergencyResourceCommand(const EmergencyResource& oldResource, const EmergencyResource& newResource);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyResource m_oldResource;
    EmergencyResource m_newResource;
};

class DeleteEmergencyResourceCommand : public Command
{
public:
    explicit DeleteEmergencyResourceCommand(const EmergencyResource& resource);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyResource m_resource;
};

class AssignEmergencyResourceToPersonCommand : public Command
{
public:
    AssignEmergencyResourceToPersonCommand(const QString& resourceId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_resourceId;
    QString m_personId;
};

class UnassignEmergencyResourceFromPersonCommand : public Command
{
public:
    UnassignEmergencyResourceFromPersonCommand(const QString& resourceId, const QString& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    QString m_resourceId;
    QString m_personId;
};
