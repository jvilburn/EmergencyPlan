#include "SidebarWidget.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

SidebarWidget::SidebarWidget(int firstRowCount, QWidget* parent)
    : QWidget(parent)
    , m_firstRowCount(firstRowCount)
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Button rows
    m_row1 = new QHBoxLayout();
    m_row1->setContentsMargins(0, 0, 0, 0);
    m_row1->setSpacing(0);
    outer->addLayout(m_row1);

    m_row2 = new QHBoxLayout();
    m_row2->setContentsMargins(0, 0, 0, 0);
    m_row2->setSpacing(0);
    outer->addLayout(m_row2);

    // Exclusive button group spanning both rows
    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    // Content pane with border matching QTabWidget::pane
    QFrame* pane = new QFrame();
    pane->setObjectName("sidebarPane");
    pane->setFrameShape(QFrame::StyledPanel);

    QVBoxLayout* paneLayout = new QVBoxLayout(pane);
    paneLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();
    paneLayout->addWidget(m_stack);

    outer->addWidget(pane, 1);

    connect(m_buttonGroup, &QButtonGroup::idClicked,
            this, &SidebarWidget::onButtonClicked);
}

void SidebarWidget::addPage(QWidget* page, const QString& label)
{
    int index = m_stack->count();

    QToolButton* button = new QToolButton();
    button->setText(label);
    button->setCheckable(true);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);

    if (index < m_firstRowCount)
    {
        m_row1->addWidget(button);
    }
    else
    {
        m_row2->addWidget(button);
    }

    m_buttonGroup->addButton(button, index);
    m_stack->addWidget(page);

    // Select the first page by default
    if (index == 0)
    {
        button->setChecked(true);
    }
}

QWidget* SidebarWidget::widget(int index) const
{
    return m_stack->widget(index);
}

int SidebarWidget::currentIndex() const
{
    return m_stack->currentIndex();
}

void SidebarWidget::setCurrentIndex(int index)
{
    QAbstractButton* button = m_buttonGroup->button(index);
    if (button)
    {
        button->setChecked(true);
        m_stack->setCurrentIndex(index);
        emit currentChanged(index);
    }
}

void SidebarWidget::setPageVisible(int index, bool visible)
{
    QAbstractButton* button = m_buttonGroup->button(index);
    if (!button)
    {
        return;
    }

    button->setVisible(visible);

    // If hiding the currently selected page, switch to first visible page
    if (!visible && m_stack->currentIndex() == index)
    {
        for (int i = 0; i < m_stack->count(); ++i)
        {
            QAbstractButton* other = m_buttonGroup->button(i);
            if (other && other->isVisible())
            {
                setCurrentIndex(i);
                return;
            }
        }
    }
}

int SidebarWidget::count() const
{
    return m_stack->count();
}

void SidebarWidget::onButtonClicked(int id)
{
    m_stack->setCurrentIndex(id);
    emit currentChanged(id);
}
