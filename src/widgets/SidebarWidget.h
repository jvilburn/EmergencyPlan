#pragma once

#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QStackedWidget;
class QToolButton;

/// Two-row navigation widget replacing QTabWidget for the sidebar.
/// Buttons are split across two rows: the first `firstRowCount` pages
/// go to row 1, the rest to row 2. A single QButtonGroup ensures
/// mutual exclusion across both rows.
class SidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarWidget(int firstRowCount = 4, QWidget* parent = nullptr);

    void addPage(QWidget* page, const QString& label);
    QWidget* widget(int index) const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    int count() const;

signals:
    void currentChanged(int index);

private:
    int m_firstRowCount;
    QHBoxLayout* m_row1;
    QHBoxLayout* m_row2;
    QButtonGroup* m_buttonGroup;
    QStackedWidget* m_stack;
};
