#pragma once

#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QStackedWidget;
/// Two-row navigation widget replacing QTabWidget for the sidebar.
/// Buttons are split across two rows: the first `firstRowCount` pages
/// go to row 1, the rest to row 2. A single QButtonGroup ensures
/// mutual exclusion across both rows.
class SidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarWidget(int firstRowCount, QWidget* parent);

    void addPage(QWidget* page, const QString& label);
    QWidget* widget(int index) const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    void setPageVisible(int index, bool visible);
    int count() const;

signals:
    void currentChanged(int index);

private slots:
    void onButtonClicked(int id);

private:
    const int m_firstRowCount;
    QHBoxLayout* m_row1 = nullptr;
    QHBoxLayout* m_row2 = nullptr;
    QButtonGroup* m_buttonGroup = nullptr;
    QStackedWidget* m_stack = nullptr;
};
