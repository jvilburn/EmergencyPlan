# Offline Map Tiles: Background Downloader

Status: planning. Nothing here is built. Decisions still open are listed at the end.

## Goal

During an emergency the internet may be down, and the map must still draw every member's
home and the streets around it. Blank tiles there are not acceptable; a slower pan or zoom
while offline is.

Today the cache holds only the tiles someone has viewed, so most of the ward is missing
offline. A background downloader fills in the tiles around each household ahead of time.

## How tiles are held today

- **Disk cache:** one file per tile under the app's cache folder (`QStandardPaths::CacheLocation`
  plus `/tiles`): 256x256 raw ARGB pixels (262 KB) plus a 160-byte metadata trailer (fetch
  date, provider, ETag, Last-Modified, scaled-up flag). Nothing is ever deleted from it.
- **Memory cache:** up to 512 decoded tiles (`MEMORY_CACHE_MAX_TILES`, about 128 MB). A tile
  drawn from memory never touches the disk.
- **Refresh:** a tile older than its provider's 30 days is asked for again with its ETag and
  Last-Modified; a 304 keeps the image under a new fetch date.
- **Failure:** a tile every provider failed is not asked for again for 60 seconds.
- **Providers:** street from OpenStreetMap, then Esri World Street Map; satellite from USGS
  Imagery, then Esri World Imagery.

Measured on the developer's laptop on 2026-11-10: 785 tiles (764 street, 21 satellite)
taking 201 MB.

## Which tiles: a radius around each household

The area to hold is a set of circles, one around each household with coordinates, with a
radius that shrinks as zoom grows. Each zoom level has four times the tiles of the one below,
so the most detailed levels decide the size. A household far from the rest (a member living
in another state) gets its own small set of circles and nothing in between.

| Zoom | One tile covers (near 36 deg N) | Radius | Tiles per household (estimate) |
|---|---|---|---|
| 18 | about 125 m | 250 m (the house and its street) | about 15 |
| 17 | about 250 m | 1 km | about 50 |
| 16 | about 500 m | 2 km | about 50 |
| 13-15 | 1-4 km | 5 km | about 40 |
| 12 and below | 8 km and more | every household's area | a few |

That is about 150 tiles per household per layer before the circles overlap. Households in
one town overlap heavily; 200 households is estimated at 10,000 to 15,000 tiles per layer.
These counts are estimates from tile sizes, not measured against the ward's addresses.

**Laptop and phone:** a laptop holds every level. A phone holds fewer, leaving out the most
detailed levels first: zoom 18, then 17.

## Where tiles may be downloaded from

Each provider's own terms, read on 2026-11-10:

| Provider | Free for this app | Downloading an area ahead of time |
|---|---|---|
| OpenStreetMap (street, raster) | yes, no key | **Forbidden.** "Bulk download ... or offer prefetch features" is not allowed and is "blocked without notice". Only tiles a user is viewing. Cache at least 7 days, refresh with ETag. |
| USGS Imagery (satellite, raster, JPEG) | yes, no key, public domain | No terms either way; the server offers tile export. Up to zoom 16, United States only. |
| Esri World Street Map (raster) | no key | Retires December 2029 ("mature support as of July 2021"). Terms not readable. |
| Esri World Imagery (raster) | no key | No retirement date found. Terms not readable. |
| Esri replacement basemaps | account and access key; 2 million tiles a month free, then $0.15 per 1,000 | Not settled: the terms are encrypted PDFs that did not read. One key built into the app bills one account for every copy. |
| OpenFreeMap (street, **vector only**) | yes, no key, no limits | **Allowed:** at most 3,000 tiles or 50 MB per download, 4 parallel requests, a unique User-Agent; larger by email to info@openfreemap.org. |
| Protomaps (street, **vector only**, self-hosted) | yes | **Allowed:** an extract of the area is the app's own copy under ODbL (credit OpenStreetMap). Up to zoom 15. |
| Stadia Maps (street; satellite is paid) | 200,000 tiles a month, account | Up to 100 MB per device, worded for mobile apps. |
| CARTO, MapTiler, Thunderforest (street) | free tiers with a key | Forbidden on the free tiers. |
| OpenTopoMap, USGS Topo (topographic) | yes | Terms silent. USGS Topo goes to zoom 16. |

So:

- **Satellite:** USGS, to zoom 16, inside the United States. Zoom 17 and above, and any
  household outside the US, stay view-only.
- **Street:** OpenStreetMap tiles cannot be downloaded ahead of time. A street download needs
  OpenFreeMap or Protomaps, which serve vector tiles; the app draws raster tiles today, so it
  would need a vector map renderer.

## Storage format

Raw pixels make the cache 13 to 50 times the size of what was downloaded. Phones cannot
afford that: a budget phone from about 2023 shipped with 32 to 64 GB, and with the system,
apps and photos, free space is often a few GB or less (figures recalled, not verified).

Size and decode time per tile, 8 real tiles of each kind, timed in Qt from Python on the
developer's laptop. Python timing is noisy: trust the order more than the milliseconds.

| Format | Street size | Street decode | Satellite size | Satellite decode |
|---|---|---|---|---|
| Downloaded file (PNG street, JPEG satellite) | 5.2-7.0 KB | 0.17-0.51 ms | 15.0-26.3 KB | 2.1-2.6 ms |
| Raw pixels (today) | 256 KB | 0.007-0.036 ms | 256 KB | 0.034-0.044 ms |
| QOI | 8.0-15.2 KB | 0.023-0.17 ms | 101-139 KB | 0.99-1.4 ms |
| LZ4 | 12.7-22.6 KB | 0.025-0.28 ms | 185-245 KB | 0.28-0.43 ms |
| Zstandard 3 | 7.8-11.1 KB | 0.047-0.28 ms | 118-170 KB | 1.6-2.0 ms |
| zlib 1 (Qt `qCompress`) | 10.1-13.6 KB | 0.18-1.0 ms | 128-172 KB | 4.4-5.7 ms |

An earlier run timed JPEG at 0.62 ms per tile; the app should be measured before deciding on
speed grounds.

Satellite tiles are photographs and compress poorly without loss; the downloaded JPEG is
lossless against what the server sent. Street tiles are flat colors and every lossless format
shrinks them well.

Estimated size for 6,000 tiles per layer:

| Storage | Street | Satellite | Total |
|---|---|---|---|
| Raw pixels | 1.5 GB | 1.5 GB | 3 GB |
| QOI street, downloaded JPEG satellite | about 70 MB | about 120 MB | about 190 MB |
| Downloaded files for both | about 35 MB | about 120 MB | about 155 MB |
| QOI for both | about 70 MB | about 720 MB | about 790 MB |

A cache file in the new format is told from a raw one by its size, so tiles already cached
keep reading, and each is rewritten in the new format the next time it refreshes.

QOI needs its single-file decoder (about 650 lines, MIT license); LZ4 and Zstandard come
through vcpkg; PNG and JPEG need nothing new.

## Speed

Panning and zooming must stay smooth. Decoding happens only when a tile misses the memory
cache: the first view of an area in a session, or after moving far enough to push older
tiles out. A screen holds about 40 to 55 tiles on a laptop and 15 to 25 on a phone, and one
miss can read up to 5 tiles from disk to build a temporary image from nearby zoom levels.

- At 60 frames per second a frame is 16 ms. From the timings above, a full screen of new
  street tiles adds about 5 ms, and of new satellite tiles about 18 ms, which can drop one
  frame.
- Decoding on a background thread keeps drawing from waiting: a new tile appears a frame
  later, drawn meanwhile from a nearby zoom level, as already happens while tiles download.
- A larger memory cache on a laptop (for example 2,000 tiles, about 500 MB) makes misses rare.
  A phone's memory cache has to stay smaller.

## Open decisions

1. Storage format: QOI street with downloaded JPEG satellite, QOI for both, or downloaded
   files for both.
2. Decode on load, or on a background thread.
3. The radius at each zoom, and which levels a phone leaves out.
4. The street source for downloading: OpenFreeMap or Protomaps (with a vector renderer), a
   paid provider that permits offline use, or street tiles stay view-only.
5. Whether to read Esri's terms (Location Platform Agreement and product terms E300) for the
   satellite fallback and the replacement basemaps.
6. The old tile cache folder left at `<local app data>/Ward Planning/Ward Planning/cache/tiles`
   (212 tiles, 54 MB, last written 20 Sep 2026): deleted once at startup, or left alone.
7. The memory cache size on a laptop and on a phone.
