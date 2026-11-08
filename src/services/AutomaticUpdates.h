#pragma once

#include "Command.h"
#include "Coordinates.h"
#include "Document.h"
#include "DocumentChange.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class Geocoder;
class UnitLookup;

/// The automatic updates of one open ward: geocoding into the table of address coordinates,
/// and ward and stake lookup into the ward details. Their results are derived data, so they
/// change the document outside the journal. WardSession owns one and hands it the document
/// it works on.
class AutomaticUpdates : public QObject
{
    Q_OBJECT

public:
    /// Works on document, which outlives it. geocoder and unitLookup answer the requests;
    /// it does not own them.
    AutomaticUpdates(Document& document, Geocoder* geocoder, UnitLookup* unitLookup,
                     QObject* parent);

    /// A ward has been created or opened: the failed list, which lasts while the ward stays
    /// open, starts empty.
    /// Requests under way carry on, and apply to the document as it stands when they answer;
    /// a result no family's address uses is dropped with the batch's unused entries.
    void wardOpened();

    /// command has applied: a run starts when it gives a family an address the batch would
    /// request, or gives the document a ward unit a lookup is due for.
    void commandApplied(const Command& command);

    /// Starts a run. The batch requests, once each, every address some family has that the
    /// table of address coordinates lacks, skipping addresses with no street, those on the
    /// failed list and those it has requested already. Each result maps the families at its
    /// address as it arrives, and the batch finishes when its last request answers; a batch
    /// with nothing to request finishes at once. Units are looked up one at a time: each
    /// stake a ward names that the document lacks, and each ward lacking its details,
    /// skipping units on the failed list. A unit whose lookup fails, or a ward a lookup finds
    /// without a chapel, goes on the failed list.
    void run();

signals:
    void documentChanged(const DocumentChange& change);

    /// The batch's requests answered, out of those it has made.
    void geocodingProgress(int completed, int total);

    /// The last request of a batch that made any has answered.
    void geocodingFinished();

    /// A batch or lookup changed the document: once per batch, at its end, and once per
    /// lookup that changed something.
    void applied();

private slots:
    void onGeocoded(const QString& address, const Coordinates& coordinates);
    void onGeocodeFailed(const QString& address);
    void onWardFound(const QString& wardUnitNumber, const Ward& ward);
    void onStakeFound(const QString& stakeUnitNumber, const Stake& stake);
    void onLookupFailed(const QString& unitNumber, const QString& error);

private:
    /// True when the batch would request address: it has a street, the table lacks it, and
    /// it is not on the failed list or requested already.
    bool needsCoordinates(const Address& address) const;

    /// One request of the batch has answered; the last one finishes the batch.
    void onBatchRequestAnswered();

    /// Finishes the batch: each held result adds the entry for its address where the table
    /// has none, which restores any a replaced document lost, then every entry no family's
    /// address uses is dropped. applied is emitted once when the batch changed the table,
    /// mid-batch or now.
    void applyBatch();

    /// Starts the next unit lookup, unless one is under way or none is needed.
    void lookUpNextUnit();

    /// The first stake, by ward unit number, that a ward names and the document lacks and
    /// that is not on the failed list; empty when there is none.
    QString nextStakeToLookUp() const;

    /// The first ward, by unit number, lacking its details and not on the failed list;
    /// empty when there is none.
    QString nextWardToLookUp() const;

    /// True when the ward lacks its details and its unit is not on the failed list.
    bool wardNeedsDetails(const Ward& ward) const;

    /// True when the stake is named, the document lacks it, and it is not on the failed list.
    bool stakeNeedsDetails(const QString& stakeUnitNumber) const;

    /// True when a lookup is due for the ward with this unit: the ward needs its details,
    /// or the stake it names does.
    bool unitNeedsLookup(const QString& wardUnitNumber) const;

    Document& m_document;
    Geocoder* m_geocoder;
    UnitLookup* m_unitLookup;
    QSet<QString> m_failed;                  ///< addresses and unit numbers that did not resolve
    QSet<QString> m_pending;                 ///< the batch's addresses not yet answered
    QHash<QString, Coordinates> m_resolved;  ///< the batch's results, held by address
    int m_batchTotal = 0;                    ///< the requests the batch has made
    bool m_batchChanged = false;             ///< a result has mapped a family mid-batch
    QString m_unitInFlight;                  ///< the unit being looked up; empty when none
};
