#pragma once

#include "Id.h"
#include "ItemType.h"

#include <QString>
#include <QStringList>

class Document;
class Person;

/// Emoji prefixes for contact detail display text.
namespace ContactIcons
{
    inline const QString Phone = QString::fromUtf8("\xf0\x9f\x93\x9e ");    // 📞
    inline const QString Email = QString::fromUtf8("\xf0\x9f\x93\xa7 ");    // 📧
    inline const QString Address = QString::fromUtf8("\xf0\x9f\x93\x8d ");  // 📍
}

/// Returns " — phone — email" suffix for display, or empty string if no contact info.
QString formatContactSuffix(const Person& person);

/// The contact rows shown under a person, each after its icon: their phone, alt phone and
/// email, then their family's address; the ones they lack, or all of them for a person the
/// document lacks, are left out.
QStringList personContactLines(const PersonId& personId, const Document& document);

/// The contact rows shown under a family: its phone, then its address, each after its icon;
/// the ones it lacks are left out.
QStringList familyContactLines(const FamilyId& familyId, const Document& document);

/// Appends a ContactDetail row under node for each of lines. A row is a copy of node with no
/// children of its own, so it carries node's ids for the views that read a row's person or
/// family.
template <typename Node>
void appendContactRows(Node* node, const QStringList& lines)
{
    for (const QString& line : lines)
    {
        Node* row = new Node(*node);
        row->children.clear();
        row->type = ItemType::ContactDetail;
        row->displayText = line;
        row->parent = node;
        node->children.append(row);
    }
}
