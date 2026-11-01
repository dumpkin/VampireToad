#include "mod_gps.h"
#include <TinyGPS++.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>

extern TinyGPSPlus gps;

namespace ModuleGPS
{
    namespace
    {
        constexpr uint8_t MAX_SATELLITES = 32;
        constexpr uint8_t GSV_GROUP_COUNT = 5;
        constexpr unsigned long SATELLITE_STALE_MS = 4000;
        constexpr size_t NMEA_BUFFER_SIZE = 256;
        constexpr uint8_t MAX_NMEA_FIELDS = 40;

        struct GsvGroup {
            Satellite satellites[MAX_SATELLITES];
            uint8_t count;
            uint8_t inView;
            unsigned long lastUpdatedAt;
            bool hasUpdate;
        };

        char nmeaBuffer[NMEA_BUFFER_SIZE];
        size_t nmeaLength = 0;
        bool nmeaChecksumValid = false;
        bool nmeaOverflow = false;
        GsvGroup gsvGroups[GSV_GROUP_COUNT] = {};
        Satellite visibleSatellites[MAX_SATELLITES];
        uint8_t visibleCount = 0;
        uint8_t visibleInView = 0;
        unsigned long visibleUpdatedAt = 0;
        bool hasVisibleUpdate = false;
        portMUX_TYPE satelliteMux = portMUX_INITIALIZER_UNLOCKED;
        LastFix lastFix = {};
        portMUX_TYPE fixMux = portMUX_INITIALIZER_UNLOCKED;
        Preferences preferences;
        bool preferencesReady = false;
        double lastPersistedLatitude = 0.0;
        double lastPersistedLongitude = 0.0;
        unsigned long lastPersistedAt = 0;

        int parseNumber(const char *text)
        {
            if (text == nullptr || *text == '\0') return -1;

            int value = 0;
            while (*text >= '0' && *text <= '9') {
                value = value * 10 + (*text - '0');
                if (value > 999) return -1;
                ++text;
            }
            return *text == '\0' ? value : -1;
        }

        int findGsvGroup(const char *talker)
        {
            if (talker[0] == 'G' && talker[1] == 'P') return 0;
            if (talker[0] == 'G' && talker[1] == 'L') return 1;
            if (talker[0] == 'G' && talker[1] == 'B') return 2;
            if (talker[0] == 'B' && talker[1] == 'D') return 3;
            if (talker[0] == 'G' && talker[1] == 'N') return 4;
            return -1;
        }

        Constellation constellationFor(const char *talker, int satelliteId)
        {
            if (talker[0] == 'G' && talker[1] == 'L') return Constellation::GLONASS;
            if ((talker[0] == 'G' && talker[1] == 'B') ||
                (talker[0] == 'B' && talker[1] == 'D')) {
                return Constellation::BEIDOU;
            }

            if (talker[0] == 'G' && talker[1] == 'N') {
                if (satelliteId >= 65 && satelliteId <= 96) return Constellation::GLONASS;
                if (satelliteId >= 201 && satelliteId <= 237) return Constellation::BEIDOU;
            }
            return Constellation::GPS;
        }

        void addStagedSatellite(GsvGroup &group, const Satellite &satellite)
        {
            for (uint8_t i = 0; i < group.count; ++i) {
                if (group.satellites[i].id == satellite.id &&
                    group.satellites[i].constellation == satellite.constellation) {
                    group.satellites[i] = satellite;
                    return;
                }
            }

            if (group.count < MAX_SATELLITES) {
                group.satellites[group.count++] = satellite;
            }
        }

        void publishSatellites()
        {
            Satellite mergedSatellites[MAX_SATELLITES];
            uint8_t mergedCount = 0;
            uint16_t totalInView = 0;
            const unsigned long now = millis();

            for (uint8_t groupIndex = 0; groupIndex < GSV_GROUP_COUNT; ++groupIndex) {
                const GsvGroup &group = gsvGroups[groupIndex];
                if (!group.hasUpdate ||
                    now - group.lastUpdatedAt > SATELLITE_STALE_MS) {
                    continue;
                }
                totalInView += group.inView;
                for (uint8_t satelliteIndex = 0;
                     satelliteIndex < group.count;
                     ++satelliteIndex) {
                    bool duplicate = false;
                    for (uint8_t mergedIndex = 0; mergedIndex < mergedCount; ++mergedIndex) {
                        if (mergedSatellites[mergedIndex].id == group.satellites[satelliteIndex].id &&
                            mergedSatellites[mergedIndex].constellation ==
                                group.satellites[satelliteIndex].constellation) {
                            mergedSatellites[mergedIndex] =
                                group.satellites[satelliteIndex];
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate && mergedCount < MAX_SATELLITES) {
                        mergedSatellites[mergedCount++] =
                            group.satellites[satelliteIndex];
                    }
                }
            }

            portENTER_CRITICAL(&satelliteMux);
            memcpy(visibleSatellites, mergedSatellites,
                   mergedCount * sizeof(Satellite));
            visibleCount = mergedCount;
            visibleInView = totalInView > 255 ? 255 : static_cast<uint8_t>(totalInView);
            visibleUpdatedAt = now;
            hasVisibleUpdate = true;
            portEXIT_CRITICAL(&satelliteMux);
        }

        void expireVisibleSatellites()
        {
            portENTER_CRITICAL(&satelliteMux);
            if (hasVisibleUpdate &&
                millis() - visibleUpdatedAt > SATELLITE_STALE_MS) {
                visibleCount = 0;
                visibleInView = 0;
                hasVisibleUpdate = false;
            }
            portEXIT_CRITICAL(&satelliteMux);
        }

        void parseGsvSentence(char *sentence)
        {
            char *fields[MAX_NMEA_FIELDS];
            uint8_t fieldCount = 0;
            fields[fieldCount++] = sentence;

            for (char *cursor = sentence; *cursor != '\0'; ++cursor) {
                if (*cursor == '*') {
                    *cursor = '\0';
                    break;
                }
                if (*cursor == ',' && fieldCount < MAX_NMEA_FIELDS) {
                    *cursor = '\0';
                    fields[fieldCount++] = cursor + 1;
                }
            }

            if (strlen(fields[0]) < 6 ||
                strcmp(fields[0] + strlen(fields[0]) - 3, "GSV") != 0 ||
                fieldCount < 4) {
                return;
            }

            const int messageCount = parseNumber(fields[1]);
            const int messageNumber = parseNumber(fields[2]);
            const int satellitesInView = parseNumber(fields[3]);
            if (messageCount < 1 || messageNumber < 1 ||
                messageNumber > messageCount || satellitesInView < 0) {
                return;
            }

            const char *talker = fields[0] + 1;
            const int groupIndex = findGsvGroup(talker);
            if (groupIndex < 0) return;
            GsvGroup &group = gsvGroups[groupIndex];

            if (messageNumber == 1) {
                group.count = 0;
                group.inView = satellitesInView > 255
                    ? 255
                    : static_cast<uint8_t>(satellitesInView);
            }

            for (uint8_t index = 0; index < 4; ++index) {
                const uint8_t fieldIndex = 4 + index * 4;
                if (fieldIndex >= fieldCount) break;

                const int id = parseNumber(fields[fieldIndex]);
                if (id < 1 || id > 999 || fieldIndex + 3 >= fieldCount) continue;

                const int elevation = parseNumber(fields[fieldIndex + 1]);
                const int azimuth = parseNumber(fields[fieldIndex + 2]);
                const int signalStrength = parseNumber(fields[fieldIndex + 3]);
                if (elevation < 0 || elevation > 90 ||
                    azimuth < 0 || azimuth > 359) {
                    continue;
                }

                Satellite satellite = {
                    static_cast<uint16_t>(id),
                    static_cast<uint16_t>(azimuth),
                    static_cast<uint8_t>(elevation),
                    static_cast<uint8_t>(signalStrength < 0 ? 0 : signalStrength),
                    constellationFor(talker, id)
                };
                addStagedSatellite(group, satellite);
            }

            if (messageNumber == messageCount) {
                group.lastUpdatedAt = millis();
                group.hasUpdate = true;
                publishSatellites();
            }
        }

        void processNmeaSentence()
        {
            if (nmeaOverflow) {
                Serial.println("[GPS NMEA] DROPPED: sentence exceeds buffer.");
            } else if (nmeaLength > 0) {
                nmeaBuffer[nmeaLength] = '\0';
                Serial.print("[GPS NMEA] ");
                Serial.println(nmeaBuffer);
            }
            if (!nmeaOverflow && nmeaChecksumValid &&
                nmeaLength > 0 && nmeaBuffer[0] == '$') {
                parseGsvSentence(nmeaBuffer);
            }
            nmeaLength = 0;
            nmeaChecksumValid = false;
            nmeaOverflow = false;
        }
    }

    void init()
    {
        Serial2.begin(9600, SERIAL_8N1, 18, 17);
        nmeaLength = 0;
        nmeaChecksumValid = false;
        nmeaOverflow = false;
        memset(gsvGroups, 0, sizeof(gsvGroups));

        preferencesReady = preferences.begin("gps-fix", false);
        if (!preferencesReady) {
            Serial.println("[GPS] Could not open NVS namespace for last fix.");
        } else if (preferences.getBool("valid", false)) {
            LastFix storedFix = {};
            storedFix.latitude = preferences.getDouble("latitude", 0.0);
            storedFix.longitude = preferences.getDouble("longitude", 0.0);
            storedFix.valid = isfinite(storedFix.latitude) &&
                              isfinite(storedFix.longitude) &&
                              fabs(storedFix.latitude) <= 90.0 &&
                              fabs(storedFix.longitude) <= 180.0;
            if (storedFix.valid) {
                portENTER_CRITICAL(&fixMux);
                lastFix = storedFix;
                portEXIT_CRITICAL(&fixMux);
                lastPersistedLatitude = storedFix.latitude;
                lastPersistedLongitude = storedFix.longitude;
            } else {
                Serial.println("[GPS] Stored last fix is invalid.");
            }
        }

        portENTER_CRITICAL(&satelliteMux);
        visibleCount = 0;
        visibleInView = 0;
        visibleUpdatedAt = 0;
        hasVisibleUpdate = false;
        portEXIT_CRITICAL(&satelliteMux);
    }

    void encode(char character)
    {
        if (gps.encode(character)) nmeaChecksumValid = true;

        if (character == '$') {
            nmeaLength = 0;
            nmeaChecksumValid = false;
            nmeaOverflow = false;
        }
        if (character == '\r') return;
        if (character == '\n') {
            processNmeaSentence();
            return;
        }

        if (nmeaOverflow) return;
        if (nmeaLength + 1 >= NMEA_BUFFER_SIZE) {
            nmeaLength = 0;
            nmeaOverflow = true;
            return;
        }
        nmeaBuffer[nmeaLength++] = character;
    }

    void recordFix(double latitude, double longitude)
    {
        if (!isfinite(latitude) || !isfinite(longitude) ||
            fabs(latitude) > 90.0 || fabs(longitude) > 180.0) {
            return;
        }

        LastFix fix = {latitude, longitude, true};
        portENTER_CRITICAL(&fixMux);
        lastFix = fix;
        portEXIT_CRITICAL(&fixMux);

        if (!preferencesReady) {
            return;
        }

        const unsigned long now = millis();
        const double latitudeDeltaMeters = (latitude - lastPersistedLatitude) * 111320.0;
        const double longitudeScale = cos(latitude * 0.0174532925);
        const double longitudeDeltaMeters =
            (longitude - lastPersistedLongitude) * 111320.0 * longitudeScale;
        const double movedMeters = sqrt(latitudeDeltaMeters * latitudeDeltaMeters +
                                         longitudeDeltaMeters * longitudeDeltaMeters);
        const bool firstPersist = !preferences.getBool("valid", false);
        const bool movedEnough = movedMeters >= 1000.0;
        const bool saveIntervalElapsed = now - lastPersistedAt >= 3600000UL;

        if (!firstPersist && !movedEnough && !saveIntervalElapsed) {
            return;
        }

        const size_t written = preferences.putDouble("latitude", latitude) +
                               preferences.putDouble("longitude", longitude);
        if (written == 0 || !preferences.putBool("valid", true)) {
            Serial.println("[GPS] Failed to save last fix to NVS.");
            return;
        }

        lastPersistedLatitude = latitude;
        lastPersistedLongitude = longitude;
        lastPersistedAt = now;
    }

    LastFix getLastFix()
    {
        portENTER_CRITICAL(&fixMux);
        const LastFix fix = lastFix;
        portEXIT_CRITICAL(&fixMux);
        return fix;
    }

    uint8_t copySatellites(Satellite *destination, uint8_t capacity)
    {
        if (destination == nullptr || capacity == 0) return 0;

        expireVisibleSatellites();
        portENTER_CRITICAL(&satelliteMux);
        const uint8_t count = visibleCount < capacity ? visibleCount : capacity;
        memcpy(destination, visibleSatellites, count * sizeof(Satellite));
        portEXIT_CRITICAL(&satelliteMux);
        return count;
    }

    uint8_t satellitesInView()
    {
        expireVisibleSatellites();
        portENTER_CRITICAL(&satelliteMux);
        const uint8_t count = visibleInView;
        portEXIT_CRITICAL(&satelliteMux);
        return count;
    }
}
