#include "FamilyEditPanel.h"
#include "AppStyles.h"
#include "MemberAccordion.h"
#include "Document.h"
#include "AutomaticUpdates.h"
#include "Address.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QIcon>
#include <QMessageBox>
#include <QSet>

FamilyEditPanel::FamilyEditPanel(AutomaticUpdates& automaticUpdates, QWidget* parent)
    : QFrame(parent)
    , m_automaticUpdates(automaticUpdates)
{
    setupUi();
    connect(&m_automaticUpdates, &AutomaticUpdates::answered, this, &FamilyEditPanel::onAnswered);
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
    m_closeButton = new QPushButton(this);
    m_closeButton->setIcon(QIcon(":/icons/win7/close.png"));
    m_closeButton->setIconSize(QSize(17, 17));
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
    connect(m_addressEdit, &QTextEdit::textChanged, this, &FamilyEditPanel::onAddressChanged);
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
    connect(m_latEdit, &QLineEdit::textChanged, this, &FamilyEditPanel::onCoordinatesTyped);
    coordLayout->addWidget(m_latEdit);
    coordLayout->addWidget(new QLabel(tr("Lon:"), this));
    m_lonEdit = new QLineEdit(this);
    m_lonEdit->setPlaceholderText(tr("Longitude"));
    connect(m_lonEdit, &QLineEdit::textChanged, this, &FamilyEditPanel::onCoordinatesTyped);
    coordLayout->addWidget(m_lonEdit);
    contentLayout->addLayout(coordLayout);

    // Note section
    contentLayout->addWidget(new QLabel(tr("Notes"), this));
    m_noteEdit = new QTextEdit(this);
    m_noteEdit->setMaximumHeight(80);
    m_noteEdit->setPlaceholderText(tr("Anything worth noting about this family"));
    connect(m_noteEdit, &QTextEdit::textChanged, this, &FamilyEditPanel::onNoteChanged);
    contentLayout->addWidget(m_noteEdit);

    // Separator
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    contentLayout->addWidget(separator);

    // Members section
    m_memberAccordion = new MemberAccordion(this);
    connect(m_memberAccordion, &MemberAccordion::membersChanged, this, &FamilyEditPanel::onMembersChanged);
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

void FamilyEditPanel::setFamily(const Family& family, const Document& document)
{
    m_familyId = family.id();
    m_originalFamily = family;
    m_editedFamily = family;
    m_addressCoordinates = document.addressCoordinates();
    m_lookingUp.clear();

    // Address
    {
        const QSignalBlocker addressBlocker(m_addressEdit);
        m_addressEdit->setPlainText(family.address().multiLine());
    }

    // Note
    {
        const QSignalBlocker noteBlocker(m_noteEdit);
        m_noteEdit->setPlainText(family.note());
    }

    // Location
    showEntryFor(family.address());
    updateCoordinatesEnabled();
    updateSaveEnabled();

    // Members
    m_memberAccordion->setMembers(family.members());

    updateTitle();
}

Family FamilyEditPanel::family() const
{
    return m_editedFamily;
}

std::optional<Coordinates> FamilyEditPanel::typedCoordinates() const
{
    if (!coordinatesTyped() || m_editedFamily.address().isEmpty())
    {
        return std::nullopt;
    }
    bool latitudeOk = false;
    bool longitudeOk = false;
    double latitude = m_latEdit->text().trimmed().toDouble(&latitudeOk);
    double longitude = m_lonEdit->text().trimmed().toDouble(&longitudeOk);
    if (!latitudeOk || !longitudeOk)
    {
        return std::nullopt;
    }
    return Coordinates(latitude, longitude);
}

bool FamilyEditPanel::isDirty() const
{
    std::optional<Coordinates> typed = typedCoordinates();
    return m_editedFamily != m_originalFamily
        || (typed.has_value() && typed != entryFor(m_editedFamily.address()));
}

std::optional<Coordinates> FamilyEditPanel::entryFor(const Address& address) const
{
    auto it = m_addressCoordinates.constFind(address.full());
    if (it != m_addressCoordinates.constEnd())
    {
        return *it;
    }
    return std::nullopt;
}

void FamilyEditPanel::showEntryFor(const Address& address)
{
    const QSignalBlocker latBlocker(m_latEdit);
    const QSignalBlocker lonBlocker(m_lonEdit);
    std::optional<Coordinates> entry = entryFor(address);
    if (entry.has_value())
    {
        m_latEdit->setText(QString::number(entry->latitude(), 'f', 6));
        m_lonEdit->setText(QString::number(entry->longitude(), 'f', 6));
    }
    else
    {
        m_latEdit->clear();
        m_lonEdit->clear();
    }
    m_shownLatitude = m_latEdit->text();
    m_shownLongitude = m_lonEdit->text();
}

bool FamilyEditPanel::coordinatesTyped() const
{
    return m_latEdit->text() != m_shownLatitude || m_lonEdit->text() != m_shownLongitude;
}

void FamilyEditPanel::updateCoordinatesEnabled()
{
    bool hasAddress = !m_editedFamily.address().isEmpty();
    m_latEdit->setEnabled(hasAddress);
    m_lonEdit->setEnabled(hasAddress);
    bool lookingUp = hasAddress && m_lookingUp == m_editedFamily.address().full();
    m_lookupButton->setEnabled(hasAddress && !lookingUp);
    m_lookupButton->setText(lookingUp ? tr("Looking up...") : tr("Look Up Coordinates"));
}

void FamilyEditPanel::refreshCoordinates(const Document& document)
{
    m_addressCoordinates = document.addressCoordinates();
    if (!coordinatesTyped())
    {
        showEntryFor(m_editedFamily.address());
    }
    updateSaveEnabled();
}

void FamilyEditPanel::updateSaveEnabled()
{
    bool unusable = coordinatesTyped()
        && !m_editedFamily.address().isEmpty()
        && !typedCoordinates().has_value();
    m_saveButton->setEnabled(!unusable);
    m_saveButton->setToolTip(unusable ? tr("Enter a latitude and a longitude") : QString());
}

void FamilyEditPanel::onLookUpCoordinates()
{
    m_lookingUp = m_editedFamily.address().full();
    updateCoordinatesEnabled();
    m_automaticUpdates.lookUp(m_lookingUp);
}

void FamilyEditPanel::onAnswered(const QString& address, const std::optional<Coordinates>& coordinates)
{
    if (address != m_lookingUp || address != m_editedFamily.address().full())
    {
        return;
    }
    m_lookingUp.clear();
    updateCoordinatesEnabled();

    if (coordinates.has_value())
    {
        m_latEdit->setText(QString::number(coordinates->latitude(), 'f', 6));
        m_lonEdit->setText(QString::number(coordinates->longitude(), 'f', 6));
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

void FamilyEditPanel::onAddressChanged()
{
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
    m_editedFamily.setAddress(address);

    // Until the user types coordinates, the fields show where the address shown is mapped.
    if (!coordinatesTyped())
    {
        showEntryFor(address);
    }
    updateCoordinatesEnabled();
    updateSaveEnabled();
    emit dataChanged();
}

void FamilyEditPanel::onCoordinatesTyped()
{
    updateSaveEnabled();
    emit dataChanged();
}

void FamilyEditPanel::onMembersChanged()
{
    QSet<PersonId> openedIds;
    for (const Person& member : m_originalFamily.members())
    {
        openedIds.insert(member.id());
    }

    QList<Person> accordionMembers = m_memberAccordion->members();
    QList<Person> kept;
    for (const Person& member : accordionMembers)
    {
        if (openedIds.contains(member.id()))
        {
            kept.append(member);
        }
    }
    m_editedFamily.setMembers(kept);

    // Each added member goes where the member order rule puts it among the members placed
    // so far, worked out again on every change as its birth year and role are filled in.
    for (const Person& member : accordionMembers)
    {
        if (!openedIds.contains(member.id()))
        {
            QList<Person> members = m_editedFamily.members();
            members.insert(m_editedFamily.insertionIndexFor(member), member);
            m_editedFamily.setMembers(members);
        }
    }
    emit dataChanged();
}

void FamilyEditPanel::onNoteChanged()
{
    m_editedFamily.setNote(m_noteEdit->toPlainText().trimmed());
    emit dataChanged();
}
