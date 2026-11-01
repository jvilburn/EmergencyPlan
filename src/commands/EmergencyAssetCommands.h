#pragma once

#include "Command.h"
#include "EmergencyAsset.h"
#include "FieldChange.h"

#include <optional>

/// A new emergency asset. Its start condition is that every person on it exists; its
/// result holds when an asset with its id is already there.
class AddEmergencyAssetCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "addEmergencyAsset";

    /// The command that adds asset, naming its people as document holds them.
    static UndoableCommandPtr fromNew(const EmergencyAsset& asset, const Document& document);

    AddEmergencyAssetCommand(const EmergencyAsset& asset, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyAsset m_asset;
};

/// An edit to one emergency asset, built from the difference between the asset as opened
/// and as saved: its name, and its people as the ids added and the ids removed. Every person
/// it adds must exist.
class UpdateEmergencyAssetCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "updateEmergencyAsset";

    /// The edit from the asset as opened to the asset as saved, naming people as document
    /// holds them, or an empty pointer when nothing differs, which WardSession::commit takes
    /// as nothing to do.
    static UndoableCommandPtr fromEdit(const EmergencyAsset& opened, const EmergencyAsset& saved, const Document& document);

    UpdateEmergencyAssetCommand(const EmergencyAssetId& assetId,
                                const std::optional<FieldChange<QString>>& name,
                                const IdSetChange<PersonId>& people,
                                const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyAssetId m_assetId;
    std::optional<FieldChange<QString>> m_name;
    IdSetChange<PersonId> m_people;
};

/// An emergency asset deleted, held as last seen: a change to the asset since makes the
/// deletion conflict, and the asset being gone already means its result holds.
class DeleteEmergencyAssetCommand : public UndoableCommand
{
public:
    static constexpr const char* TypeName = "deleteEmergencyAsset";

    /// The command that deletes asset as the user sees it, naming its people as document
    /// holds them.
    static UndoableCommandPtr fromSeen(const EmergencyAsset& asset, const Document& document);

    DeleteEmergencyAssetCommand(const EmergencyAsset& asset, const CommandDescription& description);

    QString type() const override { return TypeName; }
    CommandOutcome execute(Document& document) const override;
    UndoableCommandPtr inverse() const override;
    QJsonObject toJson() const override;
    DocumentChange documentChange() const override;

    static CommandPtr fromJson(const QJsonObject& json, const CommandDescription& description);

private:
    EmergencyAsset m_asset;
};
