#pragma once

#include "Geocoder.h"

#include <QStringList>

/// A geocoder that records each request in order and answers when the test says.
class FakeGeocoder : public Geocoder
{
    Q_OBJECT

public:
    explicit FakeGeocoder(QObject* parent);

    void geocode(const QString& address) override;

    /// Every address requested, in order.
    const QStringList& requests() const { return m_requests; }

    /// Answers the request for address with coordinates.
    void resolve(const QString& address, const Coordinates& coordinates);

    /// Answers the request for address as not found.
    void fail(const QString& address);

private:
    QStringList m_requests;
};
