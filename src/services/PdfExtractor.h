// pdfextractor.h
#pragma once

#include <QString>
#include <QList>
#include <QRectF>

struct PdfChar
{
    QChar ch;
    QRectF bbox;
    float fontSize;
    bool bold;
};

/// Text field from a PDF page
class PdfTextField
{
public:
    PdfTextField(const QString& text, double left, double right, double y, float fontSize, bool bold);

    const QString& text() const { return m_text; }
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
    double m_left;
    double m_right;
    double m_y;
    float m_fontSize;
    bool m_bold;
};

class PdfExtractor
{
public:
    PdfExtractor();
    ~PdfExtractor();

    bool open(const QString& path);
    void close();

    int pageCount() const;

    /// Get text fields from a page, sorted by Y then X.
    QList<PdfTextField> textFields(int page);

private:
    struct Impl;
    Impl* d;

    QList<PdfChar> extractChars(int page);
};