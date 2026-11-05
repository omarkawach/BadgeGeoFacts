# Validation — GeoFacts 1.3.2

## Automated checks

- ESP-IDF v5.5.1 build for ESP32-C3 production badge rev 1.2 completed. The application fits its 3 MB partition.
- Host summary, details-table and framebuffer checks passed with AddressSanitizer and UndefinedBehaviorSanitizer. Cases cover complete sentence boundaries, decimals, abbreviations, nested parentheticals, Latin accents, unsupported scripts, oversized text, source units, approximation/limit qualifiers and missing measurements.
- Equal Earth / WGS84 forward projection (EPSG:8857) agreed with PROJ for 130 control points including poles and the antimeridian, within 1 mm in host double precision.
- Land bitmap and marker use the same transformation and a uniform map scale. Previews are rendered from the actual C framebuffer code.
- Public firmware and application hashes are recorded in `release.json` and `SHA256SUMS`. The factory image contains blank NVS/PHY settings and trailing data regions, and its application bytes match `application.bin`.

## Hardware checks

- Application-only updates were flashed and hash-verified while retaining stored Wi-Fi settings and migrating earlier cached fact formats.
- Authenticated Wi-Fi, DHCP, DNS, certificate-verified HTTPS, live fact downloads and completed display refreshes were observed.
- The side button fetched different online facts and updated map markers. Multiple quick presses queue one additional request.
- SHARE setup entry, short-tap exit and reconnection to saved Wi-Fi were confirmed on hardware. Setup timeout also restores saved Wi-Fi in the implementation.
- Gold header, readable complete summaries, details table, projection label and map alignment were checked on the physical display. The thin border was rendered in host previews and installed in version 1.3.1.

## Limits

- A full one-hour wall-clock soak has not been performed. Simulated-time tests verify the configured 3,600-second boundary, retries and persistence behavior.
- Local web-page access depends on peer reachability and network client-isolation policy.
- No battery-runtime optimization or measurement has been performed. This build keeps Wi-Fi and USB available for USB-powered use.
- Wikipedia entries and their available measurements can change. Missing or ambiguous details are omitted, not inferred.

Version 1.3.2 removes the map graticules. The Equal Earth coastline bitmap, border and location marker are retained.

## Scheduling correction

Version 1.4.0 removes the reconnect handler's unconditional new-fact request. The default is one hour. Simulated-time tests cover 100 offline/online transitions without a fetch or deadline movement, exact interval expiry, failure backoff, manual override, success reset, reboot recovery, delayed clock synchronization, legacy four-hour deadline migration, missing timing metadata and backward clocks. The header is derived from the same interval configuration.

Hardware scheduling verification for 1.4.0: a saved deadline remained partially elapsed across restart (2,999 seconds remaining before, 2,854 after). A real Wi-Fi reconnect updated connectivity without triggering an early fact fetch. No premature interval-triggered download appeared during the observation. A full one-hour real-time soak remains unperformed.

Version 1.4.1 replaces the table heading, yellow accent and dotted rules with a plain grid. Labels and values use the same 20-pixel glyph atlas; long values wrap without shrinking to 14 pixels. Existing sanitizer-backed layout, summary and details checks passed for the supplied examples.
