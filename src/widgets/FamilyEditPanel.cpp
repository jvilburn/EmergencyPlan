#include "FamilyEditPanel.h"
#include "AppStyles.h"
#include "MemberAccordion.h"
#include "DocumentManager.h"
#include "GeocodingService.h"
#include "Address.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QMessageBox>

FamilyEditPanel::FamilyEditPanel(DocumentManager* docManager, QWidget* parent)
    : QFrame(parent)
    , m_docManager(docManager)
{
    setupUi();
}

void FamilyEditPanel::setupUi()
{
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(300);

    // Apply edit highlight styling
    setStyleSheet(QString(
        "FamilyEditPanel { "
        "  background-color: %1; "
        "  border-left: 3px solid %2; "
        "}"
    ).arg(AppStyles::EditHighlight::BackgroundColor, AppStyles::EditHighlight::BorderColor));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 12);
    mainLayout->setSpacing(12);

    // Title bar with close button
    QHBoxLayout* titleLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(tr("Edit Family"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    m_closeButton = new QPushButton(tr("X"), this);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setToolTip(tr("Close"));
    connect(m_closeButton, &QPushButton::clicked, this, &FamilyEditPanel::closeRequested);
    titleLayout->addWidget(m_closeButton);
    mainLayout->addLayout(titleLayout);

    // Scroll area for content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* scrollContent = new QWidget(scrollArea);
    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(0, 0, 8, 0);  // Right margin for scrollbar
    contentLayout->setSpacing(12);

    // Address section
    contentLayout->addWidget(new QLabel(tr("Address"), this));
    m_addressEdit = new QTextEdit(this);
    m_addressEdit->setMaximumHeight(80);
    m_addressEdit->setPlaceholderText(tr("Enter address (one line per row)"));
    connect(m_addressEdit, &QTextEdit::textChanged, this, &FamilyEditPanel::dataChanged);
    contentLayout->addWidget(m_addressEdit);

    // Location section
    QHBoxLayout* locationHeaderLayout = new QHBoxLayout();
    locationHeaderLayout->addWidget(new QLabel(tr("Location"), this));
    locationHeaderLayout->addStretch();
    m_lookupButton = new QPushButton(tr("Look Up Coordinates"), this);
    connect(m_lookupButton, &QPushButton::clicked, this, &FamilyEditPanel::onLookUpCoordinates);
    locationHeaderLayout->addWidget(m_lookupButton);
    contentLayout->addLayout(locationHeaderLayout);

    QHBoxLayout* coordLayout = new QHBoxLayout();
    coordLayout->addWidget(new QLabel(tr("Lat:"), this));
    m_latEdit = new QLineEdit(this);
    m_latEdit->setPlaceholderText(tr("Latitude"));
    connect(m_latEdit, &QLineEdit::textChanged, this, &FamilyEditPanel::dataChanged);
    coordLayout->addWidget(m_latEdit);
    coordLayout->addWidget(new QLabel(tr("Lon:"), this));
    m_lonEdit = new QLineEdit(this);
    m_lonEdit->setPlaceholderText(tr("Longitude"));
    connect(m_lonEdit, &QLineEdit::textChanged, this, &FamilyEditPanel::dataChanged);
    coordLayout->addWidget(m_lonEdit);
    contentLayout->addLayout(coordLayout);

    // Separator
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    contentLayout->addWidget(separator);

    // Members section
    m_memberAccordion = new MemberAccordion(this);
    connect(m_memberAccordion, &MemberAccordion::membersChanged, this, &FamilyEditPanel::dataChanged);
    contentLayout->addWidget(m_memberAccordion);

    contentLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &FamilyEditPanel::cancelRequested);
    buttonLayout->addWidget(m_cancelButton);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setDefault(true);
    connect(m_saveButton, &QPushButton::clicked, this, &FamilyEditPanel::saveRequested);
    buttonLayout->addWidget(m_saveButton);
    mainLayout->addLayout(buttonLayout);
}

void FamilyEditPanel::setFamily(const Family& family)
{
    m_familyId = family.id();
    m_originalFamily = family;

    // Block signals during population
    const QSignalBlocker blocker(this);

    // Address
    m_addressEdit->setPlainText(family.address().multiLine());

    // Location
    if (family.latitude().has_value())
    {
        m_latEdit->setText(QString::number(*family.latitude(), 'f', 6));
    }
    else
    {
        m_latEdit->clear();
    }

    if (family.longitude().has_value())
    {
        m_lonEdit->setText(QString::number(*family.longitude(), 'f', 6));
    }
    else
    {
        m_lonEdit->clear();
    }

    // Members
    m_memberAccordion->setMembers(family.members());

    updateTitle();
}

Family FamilyEditPanel::family() const
{
    // Build address from text
    Address address;
    QStringList lines = m_addressEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines)
    {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
        {
            address.addLine(trimmed);
        }
    }

    // Parse coordinates
    std::optional<double> lat;
    std::optional<double> lon;
    bool latOk = false, lonOk = false;
    double latVal = m_latEdit->text().trimmed().toDouble(&latOk);
    double lonVal = m_lonEdit->text().trimmed().toDouble(&lonOk);
    if (latOk)
    {
        lat = latVal;
    }
    if (lonOk)
    {
        lon = lonVal;
    }

    Family f = Family::createWithId(
        m_familyId,
        lat,
        lon,
        address,
        m_memberAccordion->members()
    );

    return f;
}

bool FamilyEditPanel::isDirty() const
{
    return family() != m_originalFamily;
}

void FamilyEditPanel::onLookUpCoordinates()
{
    QString addressText = m_addressEdit->toPlainText().trimmed();
    if (addressText.isEmpty())
    {
        QMessageBox::warning(this, tr("No Address"), tr("Please enter an address first."));
        return;
    }

    m_lookupButton->setEnabled(false);
    m_lookupButton->setText(tr("Looking up..."));

    GeocodingService* geocoder = new GeocodingService(this);
    connect(geocoder, &GeocodingService::geocodingComplete,
            this, &FamilyEditPanel::onGeocodingComplete);
    connect(geocoder, &GeocodingService::geocodingComplete,
            geocoder, &QObject::deleteLater);

    // Join address lines with commas for geocoding
    QString query = addressText.replace('\n', ", ");
    geocoder->geocodeAddress(query);
}

void FamilyEditPanel::onGeocodingComplete(const GeocodingResult& result)
{
    m_lookupButton->setEnabled(true);
    m_lookupButton->setText(tr("Look Up Coordinates"));

    if (result.success && result.latitude.has_value() && result.longitude.has_value())
    {
        m_latEdit->setText(QString::number(*result.latitude, 'f', 6));
        m_lonEdit->setText(QString::number(*result.longitude, 'f', 6));
    }
    else
    {
        QMessageBox::warning(this, tr("Lookup Failed"),
            tr("Could not find coordinates for this address."));
    }
}

void FamilyEditPanel::updateTitle()
{
    QString surname = m_originalFamily.surname();
    if (surname.isEmpty())
    {
        surname = tr("Family");
    }
    m_titleLabel->setText(tr("Edit: %1").arg(surname));
}
