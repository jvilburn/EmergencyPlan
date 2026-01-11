#pragma once

#include <QString>

/// Centralized UI styles for consistent appearance across the application.
/// Apply globally via QApplication::setStyleSheet() or use individual styles.
namespace AppStyles
{

/// 3D beveled button style - raised appearance with gradient and beveled borders.
/// Use for toolbar buttons, map controls, and other interactive buttons.
inline QString beveledButton()
{
    return
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffffff, stop:0.03 #f4f4f4, stop:0.5 #e8e8e8, stop:0.97 #d8d8d8, stop:1 #c8c8c8);"
        "  color: #404040;"
        "  border: 2px solid #a0a0a0;"
        "  border-top-color: #ffffff;"
        "  border-left-color: #f0f0f0;"
        "  border-right-color: #909090;"
        "  border-bottom-color: #808080;"
        "  border-radius: 5px;"
        "  padding: 6px 12px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffffff, stop:0.03 #f8f8f8, stop:0.5 #f0f0f0, stop:0.97 #e4e4e4, stop:1 #d8d8d8);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #c8c8c8, stop:0.03 #d0d0d0, stop:0.5 #e0e0e0, stop:0.97 #e8e8e8, stop:1 #f0f0f0);"
        "  border-top-color: #808080;"
        "  border-left-color: #909090;"
        "  border-right-color: #e0e0e0;"
        "  border-bottom-color: #f0f0f0;"
        "}"
        "QPushButton:disabled {"
        "  background: #e8e8e8;"
        "  color: #a0a0a0;"
        "  border-color: #c0c0c0;"
        "}";
}

/// Compact version of beveled button for icon-only buttons (map controls, etc.)
inline QString beveledButtonCompact()
{
    return
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffffff, stop:0.03 #f4f4f4, stop:0.5 #e8e8e8, stop:0.97 #d8d8d8, stop:1 #c8c8c8);"
        "  color: #404040;"
        "  border: 2px solid #a0a0a0;"
        "  border-top-color: #ffffff;"
        "  border-left-color: #f0f0f0;"
        "  border-right-color: #909090;"
        "  border-bottom-color: #808080;"
        "  border-radius: 5px;"
        "  padding: 4px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffffff, stop:0.03 #f8f8f8, stop:0.5 #f0f0f0, stop:0.97 #e4e4e4, stop:1 #d8d8d8);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #c8c8c8, stop:0.03 #d0d0d0, stop:0.5 #e0e0e0, stop:0.97 #e8e8e8, stop:1 #f0f0f0);"
        "  border-top-color: #808080;"
        "  border-left-color: #909090;"
        "  border-right-color: #e0e0e0;"
        "  border-bottom-color: #f0f0f0;"
        "}";
}

/// Global application stylesheet - apply via QApplication::setStyleSheet()
inline QString globalStylesheet()
{
    return beveledButton();
}

/// Edit highlight colors - used for FamilyEditPanel and corresponding tree row
namespace EditHighlight
{
    inline const QString BackgroundColor = "#e3f2fd";  // Light blue
    inline const QString BorderColor = "#90caf9";
}

}  // namespace AppStyles
