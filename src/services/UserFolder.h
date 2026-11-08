#pragma once

#include <QByteArray>
#include <QString>

/// The folder the app writes the files it hands to the user: update files to send, and
/// to-redo files.
class UserFolder
{
public:
    explicit UserFolder(const QString& root);

    /// The Ward Planning folder in the user's Documents.
    static UserFolder standard();

    const QString& path() const { return m_root; }

    /// Writes bytes under name in the folder, made when missing, in place of any file of that
    /// name. The path written, or empty with problem set.
    QString write(const QString& name, const QByteArray& bytes, QString* problem) const;

    /// Writes bytes under baseName followed by extension, or when a file has that name, under
    /// the first of baseName (2), baseName (3) and so on that none has. The path written, or
    /// empty with problem set.
    QString writeNew(const QString& baseName, const QString& extension, const QByteArray& bytes,
                     QString* problem) const;

private:
    QString m_root;
};
