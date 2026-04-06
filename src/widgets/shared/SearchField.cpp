#include "SearchField.h"

#include <QTimer>
#include <QAction>
#include <QStyle>

SearchField::SearchField(QWidget* parent)
    : QLineEdit(parent)
    , m_debounceTimer(new QTimer(this))
{
    setPlaceholderText(tr("Search..."));
    setClearButtonEnabled(true);

    // Configure debounce timer
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300);

    connect(m_debounceTimer, &QTimer::timeout,
            this, &SearchField::emitSearchText);

    connect(this, &QLineEdit::textChanged,
            this, &SearchField::onTextChanged);
}

void SearchField::setDebounceDelay(int msec)
{
    m_debounceTimer->setInterval(msec);
}

void SearchField::onTextChanged(const QString& /*text*/)
{
    // Restart debounce timer on each keystroke
    m_debounceTimer->start();
}

void SearchField::emitSearchText()
{
    emit searchTextChanged(text());
}
