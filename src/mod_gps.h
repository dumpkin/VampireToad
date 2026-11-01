#ifndef MOD_GPS_H
#define MOD_GPS_H

#include <Arduino.h>

namespace ModuleGPS
{
    enum class Constellation : uint8_t {
        GPS,
        GLONASS,
        BEIDOU
    };

    struct Satellite {
        uint16_t id;
        uint16_t azimuth;
        uint8_t elevation;
        uint8_t signalStrength;
        Constellation constellation;
    };

    struct LastFix {
        double latitude;
        double longitude;
        bool valid;
    };

    void init();
    void encode(char character);
    void recordFix(double latitude, double longitude);
    LastFix getLastFix();
    uint8_t copySatellites(Satellite *destination, uint8_t capacity);
    uint8_t satellitesInView();
}

#endif
