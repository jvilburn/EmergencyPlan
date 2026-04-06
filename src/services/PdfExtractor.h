// pdfextractor.h
#pragma once

#include <QString>
#include <QList>
#include <QRectF>

struct PdfChar {
    QChar ch;
    QRectF bbox;
    float fontSize;
    bool bold;
};

/// Classified text field from a PDF page
class PdfTextField
{
public:
    PdfTextField(const QString& text, int type, double left, double right, double y, float fontSize, bool bold);

    const QString& text() const { return m_text; }
    int type() const { return m_type; }
    double left() const { return m_left; }
    double right() const { return m_right; }
    double y() const { return m_y; }
    void addY(double offset) { m_y += offset; }
    float fontSize() const { return m_fontSize; }
    bool bold() const { return m_bold; }

    /// Comparison for sorting by position (Y then left)
    bool operator<(const PdfTextField& other) const;

private:
    QString m_text;
    int m_type;
    double m_left;
    double m_right;
    double m_y;
    float m_fontSize;
    bool m_bold;
};

/// Text alignment for classification rules
enum class FieldAlignment
{
    Left,
    Right,
    Center
};

/// Text classification function - returns field type or -1 if unrecognized
using TextClassifyFunc = int (*)(const QString&);

/// Rule for classifying a text field by position and font
struct FieldClassificationRule {
    int fieldType;              ///< Used only if textClassify is null
    double x;
    double fontSize;
    bool bold = false;
    FieldAlignment alignment = FieldAlignment::Left;
    TextClassifyFunc textClassify = nullptr;  ///< If set, determines field type (may return -1)
};


class PdfExtractor {
public:
    PdfExtractor();
    ~PdfExtractor();

    bool open(const QString& path);
    void close();

    int pageCount() const;

    /// Set classification rules for field typing.
    /// Rules are checked in order; first match wins.
    void setClassifications(const QList<FieldClassificationRule>& rules);

    /// Get classified text fields from a page, sorted by Y then X.
    /// If classifications are set, fields are typed.
    QList<PdfTextField> textFields(int page);

private:
    struct Impl;
    Impl* d;

    QList<PdfChar> extractChars(int page);
    int classifyField(const QString& text, double left, double right, float fontSize, bool bold) const;

    QList<FieldClassificationRule> m_rules;
};