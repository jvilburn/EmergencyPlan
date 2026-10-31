#include "FakeGeocoder.h"

FakeGeocoder::FakeGeocoder(QObject* parent)
    : Geocoder(parent)
{
}

void FakeGeocoder::geocode(const QString& address)
{
    m_requests.append(address);
}

void FakeGeocoder::resolve(const QString& address, const Coordinates& coordinates)
{
    emit resolved(address, coordinates);
}

void FakeGeocoder::fail(const QString& address)
{
    emit failed(address);
}
