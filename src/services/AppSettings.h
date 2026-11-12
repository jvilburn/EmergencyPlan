#pragma once

#include "Coordinates.h"
#include "Id.h"

#include <QJsonObject>
#include <QString>

#include <optional>

/// The app's own settings, app-wide, in one JSON file: read when made, and written whole by
/// every change. A missing file, or one that does not read as an object, reads as empty, which
/// is every default.
class AppSettings
{
public:
    explicit AppSettings(const QString& path);

    /// Use the Ward Planning server: off unless the user turns it on.
    bool useServer() const { return m_json.value("useServer").toBool(); }
    void setUseServer(bool use);

    /// This device's name, which the first start asks for; empty until then.
    QString deviceName() const { return m_json.value("deviceName").toString(); }
    void setDeviceName(const QString& name);

    /// Your name, which the first start asks for, and which this device's user takes when
    /// creating or joining a ward; empty until then.
    QString userName() const { return m_json.value("userName").toString(); }
    void setUserName(const QString& name);

    /// Your email, which the first start asks for after Your name and the user may leave empty,
    /// and which this device's user takes with Your name; empty until then.
    QString email() const { return m_json.value("email").toString(); }
    void setEmail(const QString& email);

    /// The ward open last, which the app opens when it starts.
    std::optional<DocumentId> lastWard() const;
    void setLastWard(const DocumentId& document);

    /// Where the map opens.
    std::optional<Coordinates> mapHome() const;
    void setMapHome(const Coordinates& home);

private:
    void save() const;

    QString m_path;
    QJsonObject m_json;
};
