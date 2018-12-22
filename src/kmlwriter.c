#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kmlwriter.h"
#include "units.h"

#define GPS_DEGREES_DIVIDER 10000000L

static const char KML_FILE_HEADER[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" xmlns:kml=\"http://www.opengis.net/kml/2.2\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
"<Document>\n";

static const char KML_FILE_TRAILER[] = "</Document>\n</kml>";

typedef struct coordList_t {
    int32_t lat;
    int32_t lon;
    int16_t altitude;
    uint32_t start;
    uint32_t hours, mins, secs, frac;
    uint64_t *extended_data;
    struct coordList_t *next;
} coordList_t;

static coordList_t *coordList = NULL;
static coordList_t *last = NULL;

typedef struct extendedData_t {
    char *name;
    int type;
    uint64_t* frameBuffer;
    int index;
} extendedData_t;

extendedData_t extended_data[10];
int nb_extended_data = 0;

static void clearCoordList()
{
    coordList_t *coord = coordList;
    while (coord != NULL)
    {
        coordList_t *p = coord->next;
        free(coord->extended_data);
        free(coord);
        coord = p;
    }
    coordList = NULL;
}

void kmlWriterAddPreamble(kmlWriter_t *kml, flightLog_t *log, int64_t *frame, int64_t *bufferedMainFrame, int64_t *bufferedSlowFrame)
{
    nb_extended_data = 0;
    char datetime[128];

    extended_data[nb_extended_data++].name = "rssi";
    extended_data[nb_extended_data++].name = "GPS_speed";
    extended_data[nb_extended_data++].name = "vbat";

    for (int e = 0; e < nb_extended_data; e++)
    {
        bool found = false;
        for (int i = 0; !found && i < log->frameDefs['I'].fieldCount; i++) {
            if (strcmp(log->frameDefs['I'].fieldName[i], extended_data[e].name) == 0)
            {
                extended_data[e].type = 'I';
                extended_data[e].frameBuffer = bufferedMainFrame;
                extended_data[e].index = i;
                found = true;
            }
        }
        for (int i = 0; !found && i < log->frameDefs['S'].fieldCount; i++) {
            if (strcmp(log->frameDefs['S'].fieldName[i], extended_data[e].name) == 0)
            {
                extended_data[e].type = 'S';
                extended_data[e].frameBuffer = bufferedSlowFrame;
                extended_data[e].index = i;
                found = true;
            }
        }
        for (int i = 0; !found && i < log->frameDefs['G'].fieldCount; i++) {
            if (strcmp(log->frameDefs['G'].fieldName[i], extended_data[e].name) == 0)
            {
                extended_data[e].type = 'G';
                extended_data[e].frameBuffer = frame;
                extended_data[e].index = i;
                found = true;
            }
        }

    }

    clearCoordList();

    strftime(datetime, sizeof(datetime), "%#c", &log->sysConfig.logStartTime);
    kml->file = fopen(kml->filename, "wb");

    fprintf(kml->file, KML_FILE_HEADER);
    fprintf(kml->file, "\t<name>Blackbox flight log %s</name>\n", kml->filename);
    fprintf(kml->file, "\t<snippet>Log started %s</snippet>\n", datetime);
    fprintf(kml->file, "\t<Style id=\"multiTrack_n\">\n");
    fprintf(kml->file, "\t\t<IconStyle>\n");
    fprintf(kml->file, "\t\t\t<Icon>\n");
    fprintf(kml->file, "\t\t\t\t<href>http://earth.google.com/images/kml-icons/track-directional/track-0.png</href>\n");
    fprintf(kml->file, "\t\t\t</Icon>\n");
    fprintf(kml->file, "\t\t</IconStyle>\n");
    fprintf(kml->file, "\t\t<LineStyle>\n");
    fprintf(kml->file, "\t\t\t<color>99ffac59</color>\n");
    fprintf(kml->file, "\t\t\t<width>6</width>\n");
    fprintf(kml->file, "\t\t</LineStyle>\n");
    fprintf(kml->file, "\t</Style>\n");
    fprintf(kml->file, "\t<StyleMap id=\"multiTrack\">\n");
    fprintf(kml->file, "\t\t<Pair>\n");
    fprintf(kml->file, "\t\t\t<key>normal</key>\n");
    fprintf(kml->file, "\t\t\t<styleUrl>#multiTrack_n</styleUrl>\n");
    fprintf(kml->file, "\t\t</Pair>\n");
    fprintf(kml->file, "\t\t<Pair>\n");
    fprintf(kml->file, "\t\t\t<key>highlight</key>\n");
    fprintf(kml->file, "\t\t\t<styleUrl>#multiTrack_h</styleUrl>\n");
    fprintf(kml->file, "\t\t</Pair>\n");
    fprintf(kml->file, "\t</StyleMap>\n");
    fprintf(kml->file, "\t<Style id=\"multiTrack_h\">\n");
    fprintf(kml->file, "\t\t<IconStyle>\n");
    fprintf(kml->file, "\t\t\t<scale>1.2</scale>\n");
    fprintf(kml->file, "\t\t\t<Icon>\n");
    fprintf(kml->file, "\t\t\t\t<href>http://earth.google.com/images/kml-icons/track-directional/track-0.png</href>\n");
    fprintf(kml->file, "\t\t\t</Icon>\n");
    fprintf(kml->file, "\t\t</IconStyle>\n");
    fprintf(kml->file, "\t\t<LineStyle>\n");
    fprintf(kml->file, "\t\t\t<color>99ffac59</color>\n");
    fprintf(kml->file, "\t\t\t<width>8</width>\n");
    fprintf(kml->file, "\t\t</LineStyle>\n");
    fprintf(kml->file, "\t</Style>\n");

    //fprintf(kml->file, "\t<Schema id=\"schema\">\n");
    //for (int e = 0; e < nb_extended_data; e++) {
    //    fprintf(kml->file, "\t\t<gx:SimpleArrayField name=\"%s\" type=\"int\">\n", extended_data[e].name);
    //    fprintf(kml->file, "\t\t\t<displayName>Heart Rate</displayName>\n");
    //    fprintf(kml->file, "\t\t</gx:SimpleArrayField>\n");
    //}
    //fprintf(kml->file, "\t</Schema>\n");


    fprintf(kml->file, "\t<Folder>\n");
    fprintf(kml->file, "\t\t<name>Tracks</name>\n");
    fprintf(kml->file, "\t\t<Placemark>\n");
    fprintf(kml->file, "\t\t\t<name>Blackbox flight log</name>\n");
    fprintf(kml->file, "\t\t\t<styleUrl>#multiTrack</styleUrl>\n");
    fprintf(kml->file, "\t\t\t<gx:Track>\n");
    fprintf(kml->file, "\t\t\t\t<altitudeMode>absolute</altitudeMode>\n");
}

kmlWriter_t* kmlWriterCreate(const char *filename)
{
    kmlWriter_t *result = malloc(sizeof(*result));

    result->filename = strdup(filename);
    result->state = KMLWRITER_STATE_EMPTY;
    result->file = NULL;

    return result;
}

void kmlWriterAddPoint(kmlWriter_t *kml, flightLog_t *log, int64_t time, int64_t *frame, int64_t *bufferedMainFrame, int64_t *bufferedSlowFrame)
{
    if (!kml)
        return;

    if (kml->state == KMLWRITER_STATE_EMPTY) {
        kmlWriterAddPreamble(kml, log, frame, bufferedMainFrame, bufferedSlowFrame);
        kml->startTime = log->sysConfig.logStartTime.tm_hour * 3600 + log->sysConfig.logStartTime.tm_min * 60 + log->sysConfig.logStartTime.tm_sec;
        kml->state = KMLWRITER_STATE_WRITING_TRACK;
    }



    if (time != -1) {

        coordList_t *coord = malloc(sizeof(*coord));
        if (coord != NULL)
        {
            //We'll just assume that the timespan is less than 24 hours, and make up a date
            time += kml->startTime * 1000000;

            coord->lat = frame[log->gpsFieldIndexes.GPS_coord[0]];
            coord->lon = frame[log->gpsFieldIndexes.GPS_coord[1]];
            coord->altitude = frame[log->gpsFieldIndexes.GPS_altitude];

            coord->frac = time % 1000000;
            coord->secs = time / 1000000;

            coord->mins = coord->secs / 60;
            coord->secs %= 60;

            coord->hours = coord->mins / 60;
            coord->mins %= 60;

            coord->extended_data = malloc(nb_extended_data * sizeof(*(coord->extended_data)));
            for (int e = 0; e < nb_extended_data; e++)
            {
                coord->extended_data[e] = extended_data[e].frameBuffer[extended_data[e].index];
            }

            coord->next = NULL;

            if (coordList == NULL)
            {
                last = coord;
                coordList = coord;
            }
            else
            {
                last->next = coord;
                last = coord;
            }
        }
    }
}

void fprintfGPSFields(FILE *file, uint64_t fieldvalue, int i);
bool fprintfMainFieldInUnit(flightLog_t *log, FILE *file, int fieldIndex, int64_t fieldValue, bool unitName);

void kmlWriterDestroy(kmlWriter_t* kml, flightLog_t *log)
{
        char negSign[] = "-";
        char noSign[] = "";

    if (!kml)
        return;

    //coordList_t **maximums = malloc(sizeof(*maximums) * (nb_extended_data + 1));
    int16_t max_altitude_value = INT16_MIN;
    coordList_t *max_altitude_coord;

    for (coordList_t *coord = coordList; coord != NULL; coord = coord->next)
    {
        fprintf(kml->file, "\t\t\t\t<when>%04u-%02u-%02uT%02u:%02u:%02u.%06uZ</when>\n",
            log->sysConfig.logStartTime.tm_year, log->sysConfig.logStartTime.tm_mon + 1, log->sysConfig.logStartTime.tm_mday,
            coord->hours, coord->mins, coord->secs, coord->frac);
    }
    for (coordList_t *coord = coordList; coord != NULL; coord = coord->next)
    {
        int32_t latDegrees = coord->lat / GPS_DEGREES_DIVIDER;
        int32_t lonDegrees = coord->lon / GPS_DEGREES_DIVIDER;

        uint32_t latFracDegrees = abs(coord->lat) % GPS_DEGREES_DIVIDER;
        uint32_t lonFracDegrees = abs(coord->lon) % GPS_DEGREES_DIVIDER;

        char *latSign = ((coord->lat < 0) && (latDegrees == 0)) ? negSign : noSign;
        char *lonSign = ((coord->lon < 0) && (lonDegrees == 0)) ? negSign : noSign;

        fprintf(kml->file, "\t\t\t\t<gx:coord>%s%d.%07u %s%d.%07u %d</gx:coord>\n", lonSign, lonDegrees, lonFracDegrees, latSign, latDegrees, latFracDegrees, coord->altitude);
        
        if (coord->altitude > max_altitude_value) {
            max_altitude_value = coord->altitude;
            max_altitude_coord = coord;
        }
    }

    if (nb_extended_data > 0)
    {
        fprintf(kml->file, "\t\t\t\t<ExtendedData>\n");
        fprintf(kml->file, "\t\t\t\t\t<SchemaData schemaUrl = \"#schema\">\n");
        for (int e = 0; e < nb_extended_data; e++)
        {
            fprintf(kml->file, "\t\t\t\t\t\t<gx:SimpleArrayData name=\"%s\">\n", extended_data[e].name);
            for (coordList_t *coord = coordList; coord != NULL; coord = coord->next)
            {
                if (extended_data[e].type == 'G')
                {
                    fputs("\t\t\t\t\t\t\t<gx:value>", kml->file);
                    fprintfGPSFields(kml->file, coord->extended_data[e], extended_data[e].index);
                    fputs("</gx:value>\n", kml->file);
                }
                else if (extended_data[e].type == 'I')
                {
                    fputs("\t\t\t\t\t\t\t<gx:value>", kml->file);
                    fprintfMainFieldInUnit(log, kml->file, extended_data[e].index, coord->extended_data[e], true);
                    fputs("</gx:value>\n", kml->file);
                }
            }
            fprintf(kml->file, "\t\t\t\t\t\t</gx:SimpleArrayData>\n");
        }
        fprintf(kml->file, "\t\t\t\t\t</SchemaData>\n");
        fprintf(kml->file, "\t\t\t\t</ExtendedData>\n");
    }   

    if (kml->state == KMLWRITER_STATE_WRITING_TRACK) {
        fprintf(kml->file, "\t\t\t</gx:Track>\n\t\t</Placemark>\n");
    }

    {
        int32_t latDegrees = max_altitude_coord->lat / GPS_DEGREES_DIVIDER;
        int32_t lonDegrees = max_altitude_coord->lon / GPS_DEGREES_DIVIDER;

        uint32_t latFracDegrees = abs(max_altitude_coord->lat) % GPS_DEGREES_DIVIDER;
        uint32_t lonFracDegrees = abs(max_altitude_coord->lon) % GPS_DEGREES_DIVIDER;

        char *latSign = ((max_altitude_coord->lat < 0) && (latDegrees == 0)) ? negSign : noSign;
        char *lonSign = ((max_altitude_coord->lon < 0) && (lonDegrees == 0)) ? negSign : noSign;

        fprintf(kml->file, "\t\t<Placemark>\n");
        fprintf(kml->file, "\t\t\t<name>Max Altitude = %u</name>\n", max_altitude_coord->altitude);
        fprintf(kml->file, "\t\t\t<Point>\n");
        fprintf(kml->file, "\t\t\t\t<altitudeMode>absolute</altitudeMode>\n");
        fprintf(kml->file, "\t\t\t\t<coordinates>%s%d.%07u,%s%d.%07u,%d</coordinates>\n", lonSign, lonDegrees, lonFracDegrees, latSign, latDegrees, latFracDegrees, max_altitude_coord->altitude);
        fprintf(kml->file, "\t\t\t</Point>\n");
        fprintf(kml->file, "\t\t</Placemark>\n");
    }

    clearCoordList();
    fprintf(kml->file, "</Folder>\n");

    if (kml->state != KMLWRITER_STATE_EMPTY) {
        fprintf(kml->file, KML_FILE_TRAILER);
        fclose(kml->file);
    }

    free(kml->filename);
    free(kml);
}
