// PdfExtractor.cpp
#include "PdfExtractor.h"

// MuPDF uses setjmp/longjmp for error handling (fz_try/fz_catch).
// Suppress MSVC warning about interaction with C++ destructors.
#ifdef _MSC_VER
#pragma warning(disable: 4611)
#endif

#include <mupdf/fitz.h>
#include <QFile>
#include <algorithm>

// ============================================================================
// PdfTextField
// ============================================================================

PdfTextField::PdfTextField(const QString& text, int type, double left, double right, double y, float fontSize, bool bold)
    : m_text(text)
    , m_type(type)
    , m_left(left)
    , m_right(right)
    , m_y(y)
    , m_fontSize(fontSize)
    , m_bold(bold)
{
}

bool PdfTextField::operator<(const PdfTextField& other) const
{
    if (m_y != other.m_y)
    {
        return m_y < other.m_y;
    }
    return m_left < other.m_left;
}

// ============================================================================
// PdfExtractor
// ============================================================================

struct PdfExtractor::Impl {
    fz_context* ctx = nullptr;
    fz_document* doc = nullptr;
};

PdfExtractor::PdfExtractor() : d(new Impl) {
    d->ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    fz_register_document_handlers(d->ctx);
}

PdfExtractor::~PdfExtractor() {
    close();
    if (d->ctx) fz_drop_context(d->ctx);
    delete d;
}

bool PdfExtractor::open(const QString& path) {
    close();
    // Convert to C string before fz_try to avoid C++ temporaries in setjmp scope
    QByteArray pathUtf8 = path.toUtf8();
    const char* pathCStr = pathUtf8.constData();
    fz_try(d->ctx) {
        d->doc = fz_open_document(d->ctx, pathCStr);
    }
    fz_catch(d->ctx) {
        return false;
    }
    return true;
}

void PdfExtractor::close() {
    if (d->doc) {
        fz_drop_document(d->ctx, d->doc);
        d->doc = nullptr;
    }
}

int PdfExtractor::pageCount() const {
    if (!d->doc) return 0;
    return fz_count_pages(d->ctx, d->doc);
}

QList<PdfChar> PdfExtractor::extractChars(int pageNum) {
    QList<PdfChar> result;
    if (!d->doc) return result;

    fz_page* page = nullptr;
    fz_stext_page* textPage = nullptr;

    fz_try(d->ctx) {
        page = fz_load_page(d->ctx, d->doc, pageNum);
        textPage = fz_new_stext_page_from_page(d->ctx, page, nullptr);

        for (fz_stext_block* block = textPage->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT) continue;

            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                    fz_rect bbox = fz_rect_from_quad(ch->quad);
                    const char* fontName = fz_font_name(d->ctx, ch->font);

                    PdfChar pc;
                    pc.ch = QChar(ch->c);
                    pc.bbox = QRectF(bbox.x0, bbox.y0, bbox.x1 - bbox.x0, bbox.y1 - bbox.y0);
                    pc.fontSize = ch->size;
                    pc.bold = fontName && (strstr(fontName, "Bold") || strstr(fontName, "bold"));

                    result.append(pc);
                }
            }
        }
    }
    fz_always(d->ctx) {
        if (textPage) fz_drop_stext_page(d->ctx, textPage);
        if (page) fz_drop_page(d->ctx, page);
    }
    fz_catch(d->ctx) {
        // error, return what we have
    }

    return result;
}

void PdfExtractor::setClassifications(const QList<FieldClassificationRule>& rules)
{
    m_rules = rules;
}

int PdfExtractor::classifyField(const QString& text, double left, double right, float fontSize, bool bold) const
{
    constexpr double X_TOL = 5.0;  // Allow for X drift across pages (observed up to ~4 points)
    constexpr double SIZE_TOL = 0.5;

    for (const FieldClassificationRule& rule : m_rules)
    {
        // Check position based on alignment
        double fieldX;
        switch (rule.alignment)
        {
            case FieldAlignment::Left:
                fieldX = left;
                break;
            case FieldAlignment::Right:
                fieldX = right;
                break;
            case FieldAlignment::Center:
                fieldX = (left + right) / 2.0;
                break;
        }

        if (qAbs(fieldX - rule.x) > X_TOL)
        {
            continue;
        }
        if (qAbs(fontSize - rule.fontSize) > SIZE_TOL)
        {
            continue;
        }
        if (rule.bold && !bold)
        {
            continue;
        }

        // Position/font/bold matched - determine field type
        if (rule.textClassify)
        {
            return rule.textClassify(text);
        }
        return rule.fieldType;
    }
    return -1;  // Unknown
}

QList<PdfTextField> PdfExtractor::textFields(int pageNum)
{
    QList<PdfChar> chars = extractChars(pageNum);
    QList<PdfTextField> fields;

    if (chars.isEmpty())
    {
        return fields;
    }

    // Tolerances for grouping chars into runs
    constexpr float yTolerance = 5.0f;
    constexpr float xGapMax = 10.0f;
    constexpr float fontSizeTolerance = 0.5f;

    // Accumulate run in local variables
    QString currentText = QString(chars[0].ch);
    QRectF currentBbox = chars[0].bbox;
    float currentFontSize = chars[0].fontSize;
    bool currentBold = chars[0].bold;

    for (int i = 1; i < chars.size(); ++i)
    {
        const PdfChar& prev = chars[i - 1];
        const PdfChar& curr = chars[i];

        bool isSameRun = qAbs(prev.bbox.y() - curr.bbox.y()) < yTolerance
            && (curr.bbox.x() - prev.bbox.right()) < xGapMax
            && qAbs(prev.fontSize - curr.fontSize) < fontSizeTolerance
            && prev.bold == curr.bold;

        if (isSameRun)
        {
            currentText += curr.ch;
            currentBbox = currentBbox.united(curr.bbox);
        }
        else
        {
            // Finalize current field
            int type = m_rules.isEmpty() ? -1 : classifyField(currentText, currentBbox.x(), currentBbox.right(), currentFontSize, currentBold);
            fields.append(PdfTextField(currentText, type, currentBbox.x(), currentBbox.right(), currentBbox.y(), currentFontSize, currentBold));

            // Start new field
            currentText = QString(curr.ch);
            currentBbox = curr.bbox;
            currentFontSize = curr.fontSize;
            currentBold = curr.bold;
        }
    }

    // Finalize last field
    int type = m_rules.isEmpty() ? -1 : classifyField(currentText, currentBbox.x(), currentBbox.right(), currentFontSize, currentBold);
    fields.append(PdfTextField(currentText, type, currentBbox.x(), currentBbox.right(), currentBbox.y(), currentFontSize, currentBold));

    // Sort by Y (top to bottom), then X (left to right)
    std::sort(fields.begin(), fields.end());

    return fields;
}