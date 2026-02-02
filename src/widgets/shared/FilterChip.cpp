#include "FilterChip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

FilterChip::FilterChip(const QString& label,
                       const QString& value,
                       QWidget* parent)
    : QFrame(parent)
    , m_label(label)
    , m_value(value)
{
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(
        "FilterChip {"
        "  background: #e0e0e0;"
        "  border: 1px solid #b0b0b0;"
        "  border-radius: 3px;"
        "  padding: 2px 4px;"
        "}"
    );

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 2, 2);
    layout->setSpacing(4);

    // Combine label and value, truncate if too long
    QString displayText = value;
    if (!label.isEmpty())
    {
        displayText = label + ": " + value;
    }

    m_textLabel = new QLabel(this);
    if (displayText.length() > 30)
    {
        m_textLabel->setToolTip(displayText);
        displayText = displayText.left(27) + "...";
    }
    m_textLabel->setText(displayText);
    layout->addWidget(m_textLabel);

    m_removeButton = new QPushButton(tr("×"), this);
    m_removeButton->setFixedSize(16, 16);
    m_removeButton->setFlat(true);
    m_removeButton->setStyleSheet(
        "QPushButton { font-weight: bold; color: #606060; }"
        "QPushButton:hover { color: #000000; }"
    );
    layout->addWidget(m_removeButton);

    connect(m_removeButton, &QPushButton::clicked,
            this, &FilterChip::removeClicked);
}
