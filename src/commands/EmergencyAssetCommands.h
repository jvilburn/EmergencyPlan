#pragma once

#include "Command.h"
#include "EmergencyAsset.h"

class AddEmergencyAssetCommand : public Command
{
public:
    explicit AddEmergencyAssetCommand(const EmergencyAsset& asset);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyAsset m_asset;
};

class UpdateEmergencyAssetCommand : public Command
{
public:
    UpdateEmergencyAssetCommand(const EmergencyAsset& oldAsset, const EmergencyAsset& newAsset);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyAsset m_oldAsset;
    EmergencyAsset m_newAsset;
};

class DeleteEmergencyAssetCommand : public Command
{
public:
    explicit DeleteEmergencyAssetCommand(const EmergencyAsset& asset);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyAsset m_asset;
};

class AssignEmergencyAssetToPersonCommand : public Command
{
public:
    AssignEmergencyAssetToPersonCommand(const EmergencyAssetId& assetId, const PersonId& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyAssetId m_assetId;
    PersonId m_personId;
};

class UnassignEmergencyAssetFromPersonCommand : public Command
{
public:
    UnassignEmergencyAssetFromPersonCommand(const EmergencyAssetId& assetId, const PersonId& personId);

    void execute(Document& document) override;
    void undo(Document& document) override;
    QString description() const override;
    DocumentChange documentChange() const override;

private:
    EmergencyAssetId m_assetId;
    PersonId m_personId;
};
