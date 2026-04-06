#pragma once

#include <QLineEdit>

class QTimer;
class QAction;

/// Search input field with debounced text changes and clear button.
class SearchField : public QLineEdit
{
    Q_OBJECT

public:
    explicit SearchField(QWidget* parent);

    /// Set the debounce delay in milliseconds (default 300ms).
    void setDebounceDelay(int msec);

signals:
    /// Emitted after debounce delay when search text changes.
    void searchTextChanged(const QString& text);

private slots:
    void onTextChanged(const QString& text);
    void emitSearchText();

private:
    QTimer* m_debounceTimer;
    QAction* m_clearAction;
};
