#pragma once

#include <QObject>

/// SelectionPreservingTreeView keeps the selected row and the expanded rows across a model
/// rebuild, by their keys, and expands top-level rows new since the last rebuild only when
/// asked to.
class TreeViewStateTest : public QObject
{
    Q_OBJECT

public:
    explicit TreeViewStateTest(QObject* parent);

private slots:
    void rebuildKeepsTheSelectedAndExpandedRows();
    void rebuildExpandsNewTopLevelRowsOnlyWhenAsked();
};
