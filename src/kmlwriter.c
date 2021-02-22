#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "kmlwriter.h"
#include "units.h"

void fprintfGPSFields(FILE *file, uint64_t fieldvalue, int i, bool unitName);
bool fprintfMainFieldInUnit(flightLog_t *log, FILE *file, int fieldIndex, int64_t fieldValue, bool unitName);
void fprintfSlowFrameFields(flightLog_t *log, FILE *file, int64_t fieldValue, int i);
char *GetFieldUnit(int type, int i);

#define NB_EXTENDED_DATA_MAX 10

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

typedef enum {
    PLACE_MIN = (1 << 0),
    PLACE_MAX = (1 << 1),
} placeFlags_t;

typedef struct {
    char *name;
    int type;
    int64_t* frameBuffer;
    int index;
    uint8_t placeFlags;
} extendedData_t;

typedef struct {
    uint64_t max;
    uint64_t min;
} placeValue_t;

static extendedData_t extended_data[NB_EXTENDED_DATA_MAX];
static int nb_extended_data = 0;
static int change_track_data = -1;
static int nb_change_track = 0;
static int color_extended_data = -1;

static uint8_t altitude_place_flags = 0;

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

bool kmlSetMinimums(char *optarg)
{
    bool found = false;
    char *tok = strtok(optarg, ",");

    while (tok != NULL)
    {
        for (int e = 0; e < nb_extended_data; e++)
        {
            if (strcmp(tok, extended_data[e].name) == 0)
            {
                extended_data[e].placeFlags |= PLACE_MIN;
                found = true;
            }
            else if (strcmp(tok, "altitude") == 0)
            {
                altitude_place_flags |= PLACE_MIN;
                found = true;
            }
        }

        if (!found)
        {
            extended_data[nb_extended_data].placeFlags |= PLACE_MIN;
            extended_data[nb_extended_data++].name = tok;
        }

        tok = strtok(NULL, ",");
    }

    return found;
}

bool kmlSetMaximums(char *optarg)
{
    bool found = false;
    char *tok = strtok(optarg, ",");

    while (tok != NULL)
    {
        for (int e = 0; e < nb_extended_data; e++)
        {
            if (strcmp(tok, extended_data[e].name) == 0)
            {
                extended_data[e].placeFlags |= PLACE_MAX;
                found = true;
            }
            else if (strcmp(tok, "altitude") == 0)
            {
                altitude_place_flags |= PLACE_MAX;
                found = true;
            }
        }
        if (!found)
        {
            extended_data[nb_extended_data].placeFlags |= PLACE_MAX;
            extended_data[nb_extended_data++].name = tok;
        }

        tok = strtok(NULL, ",");
    }

    return found;
}

bool kmlSetColor(char * optarg)
{
    bool found = false;
    for (int e = 0; e < nb_extended_data && !found; e++)
    {
        if (strcmp(optarg, extended_data[e].name) == 0)
        {
            color_extended_data = e;
            found = true;
        }
    }

    if (!found)
    {
        color_extended_data = nb_extended_data;
        extended_data[nb_extended_data++].name = optarg;
    }

    return found;
}

void kmlSetInfos(char *optarg)
{
    nb_extended_data = 0;

    char *tok = strtok(optarg, ",");
    while (tok != NULL && nb_extended_data < NB_EXTENDED_DATA_MAX)
    {
        extended_data[nb_extended_data++].name = tok;
        tok = strtok(NULL, ",");
    }
    if (nb_extended_data == NB_EXTENDED_DATA_MAX)
        fprintf(stderr, "Waring, max infos for --kml-infos is %u\n", NB_EXTENDED_DATA_MAX);
}

kmlWriter_t* kmlWriterCreate(const char *filename, bool trackModes)
{
    kmlWriter_t *result = malloc(sizeof(*result));

    result->filename = _strdup(filename);
    result->state = KMLWRITER_STATE_EMPTY;
    result->file = NULL;
    result->trackModes = trackModes;

    return result;
}

void kmlWriterAddPreamble(kmlWriter_t *kml, flightLog_t *log, int64_t *frame, int64_t *bufferedMainFrame, int64_t *bufferedSlowFrame)
{
    if (kml->trackModes) {
        extended_data[nb_extended_data++].name = "flightModeFlags";
        change_track_data = nb_extended_data - 1;
    }

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

        if (!found)
        {
            fprintf(stderr, "Unknown field %s in --kml-infos\n", extended_data[e].name);
            //free(kml->filename);
            //kml->state = KMLWRITER_STATE_ERROR;
            //return;
        }

    }

    clearCoordList();
}

void kmlWriteStyles(kmlWriter_t *kml)
{
    //uint32_t color[NB_EXTENDED_DATA_MAX] =
    //{
    //    0x99ffac59;
    //};

    //for (int s = 0; s < nb_change_track; s++)
    {
        fprintf(kml->file, "\t<Style id=\"multiTrack_n\">\n");
        fprintf(kml->file, "\t\t<IconStyle>\n");
        fprintf(kml->file, "\t\t\t<Icon>\n");
        fprintf(kml->file, "\t\t\t\t<href>http://earth.google.com/images/kml-icons/track-directional/track-0.png</href>\n");
        fprintf(kml->file, "\t\t\t</Icon>\n");
        fprintf(kml->file, "\t\t</IconStyle>\n");
        fprintf(kml->file, "\t\t<LineStyle>\n");
        if (nb_change_track > 0)
        {
            fprintf(kml->file, "\t\t\t<color>ffffff00</color>\n");
            fprintf(kml->file, "\t\t\t<colorMode>random</colorMode>\n");
        }
        else
        {
            fprintf(kml->file, "\t\t\t<color>99ffac59</color>\n");
        }
        fprintf(kml->file, "\t\t\t<width>6</width>\n");
        fprintf(kml->file, "\t\t</LineStyle>\n");
        fprintf(kml->file, "\t</Style>\n");

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

        //fprintf(kml->file, "\t\t<BalloonStyle>\n");
        //fprintf(kml->file, "\t\t\t<text>\n");
        //fprintf(kml->file, "\t\t\t\t<![CDATA[\n");
        //for (int e = 0; e < nb_extended_data; e++) {
        //    fprintf(kml->file, "\t\t\t\t\t%s : $[schema/%s/displayName]</br>\n", extended_data[e].name, extended_data[e].name);
        //}
        //fprintf(kml->file, "\t\t\t\t]]>\n");
        //fprintf(kml->file, "\t\t\t</text>\n");
        //fprintf(kml->file, "\t\t</BalloonStyle>\n");

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

        //fprintf(kml->file, "\t<Style id=\"infos_balloon\">\n");
        //fprintf(kml->file, "\t\t<BalloonStyle>\n");
        //fprintf(kml->file, "\t\t\t<text>\n");
        //fprintf(kml->file, "\t\t\t\t<![CDATA[\n");
        //for (int e = 0; e < nb_extended_data; e++) {
        //    fprintf(kml->file, "\t\t\t\t\t%s : $[%s]</br>\n", extended_data[e].name, extended_data[e].name);
        //}
        //fprintf(kml->file, "\t\t\t\t]]>\n");
        //fprintf(kml->file, "\t\t\t</text>\n");
        //fprintf(kml->file, "\t\t</BalloonStyle>\n");
        //fprintf(kml->file, "\t</Style>\n");

        fprintf(kml->file, "\t<Schema name=\"schema\" id=\"schema\">\n");
        for (int e = 0; e < nb_extended_data; e++) {
            char *t;
            if (extended_data[e].type == 'I' || extended_data[e].type == 'G')
                t = "float";
            else
                t = "string";
            fprintf(kml->file, "\t\t<gx:SimpleArrayField name=\"%s\" type=\"%s\">\n", extended_data[e].name, t);
            fprintf(kml->file, "\t\t\t<displayName>%s (%s)</displayName>\n", extended_data[e].name, GetFieldUnit(extended_data[e].type, extended_data[e].index));
            fprintf(kml->file, "\t\t</gx:SimpleArrayField>\n");
        }
        fprintf(kml->file, "\t</Schema>\n");
    }
}

void kmlWriterAddPoint(kmlWriter_t *kml, flightLog_t *log, int64_t time, int64_t *frame, int64_t *bufferedMainFrame, int64_t *bufferedSlowFrame)
{
    static uint64_t prev_change_value = 0;

    if (!kml)
        return;

    if (kml->state == KMLWRITER_STATE_EMPTY) {
        kml->state = KMLWRITER_STATE_WRITING_TRACK;
        kmlWriterAddPreamble(kml, log, frame, bufferedMainFrame, bufferedSlowFrame);
        kml->startTime = log->sysConfig.logStartTime.tm_hour * 3600 + log->sysConfig.logStartTime.tm_min * 60 + log->sysConfig.logStartTime.tm_sec;
        prev_change_value = 0;
    }

    if (kml->state == KMLWRITER_STATE_ERROR)
        return;

    if (time != -1) {

        coordList_t *coord = malloc(sizeof(*coord));
        if (coord != NULL)
        {
            //We'll just assume that the timespan is less than 24 hours, and make up a date
            time += kml->startTime * 1000000;

            coord->lat = (int32_t)frame[log->gpsFieldIndexes.GPS_coord[0]];
            coord->lon = (int32_t)frame[log->gpsFieldIndexes.GPS_coord[1]];
            coord->altitude = (int16_t)frame[log->gpsFieldIndexes.GPS_altitude];

            coord->frac = (uint32_t)(time % 1000000);
            coord->secs = (uint32_t)(time / 1000000);

            coord->mins = coord->secs / 60;
            coord->secs %= 60;

            coord->hours = coord->mins / 60;
            coord->mins %= 60;

            coord->extended_data = malloc(nb_extended_data * sizeof(*(coord->extended_data)));
            for (int e = 0; e < nb_extended_data; e++)
            {
                if (extended_data[e].frameBuffer != NULL)
                {
                    coord->extended_data[e] = extended_data[e].frameBuffer[extended_data[e].index];
                    if (e == change_track_data)
                    {
                        if (prev_change_value != coord->extended_data[e]) nb_change_track++;
                        prev_change_value = coord->extended_data[change_track_data];
                    }
                }
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

void kmlWriteCoordinates(kmlWriter_t *kml, int32_t lat, int32_t lon, uint16_t altitude, char sep)
{
    const long GPS_DEGREES_DIVIDER = 10000000L;
    char negSign[] = "-";
    char noSign[] = "";

    int32_t latDegrees = lat / GPS_DEGREES_DIVIDER;
    int32_t lonDegrees = lon / GPS_DEGREES_DIVIDER;

    uint32_t latFracDegrees = abs(lat) % GPS_DEGREES_DIVIDER;
    uint32_t lonFracDegrees = abs(lon) % GPS_DEGREES_DIVIDER;

    char *latSign = ((lat < 0) && (latDegrees == 0)) ? negSign : noSign;
    char *lonSign = ((lon < 0) && (lonDegrees == 0)) ? negSign : noSign;

    fprintf(kml->file, "%s%d.%07u%c%s%d.%07u%c%d", lonSign, lonDegrees, lonFracDegrees, sep, latSign, latDegrees, latFracDegrees, sep, altitude);
}

void kmlWriteExtendedDataValue(kmlWriter_t *kml, flightLog_t *log, uint64_t value, int e, bool unit)
{
    if (extended_data[e].type == 'G')
    {
        fprintfGPSFields(kml->file, value, extended_data[e].index, unit);
    }
    else if (extended_data[e].type == 'I')
    {
        fprintfMainFieldInUnit(log, kml->file, extended_data[e].index, value, unit);
    }
    else if (extended_data[e].type == 'S')
    {
        fprintfSlowFrameFields(log, kml->file, value, extended_data[e].index);
    }
}

void kmlWritePlacemark(kmlWriter_t *kml, flightLog_t *log, char *name, coordList_t *coord, int e)
{
    fprintf(kml->file, "\t\t<Placemark>\n");
    if (e >= 0)
    {
        fprintf(kml->file, "\t\t\t<name>%s %s = ", name, extended_data[e].name);
        kmlWriteExtendedDataValue(kml, log, coord->extended_data[e], e, true);
    }
    else
    {
        fprintf(kml->file, "\t\t\t<name>%s altitude = %i m", name, coord->altitude);
    }
    fprintf(kml->file, "</name>\n");
    fprintf(kml->file, "\t\t\t<Point>\n");
    fprintf(kml->file, "\t\t\t\t<altitudeMode>absolute</altitudeMode>\n");
    fprintf(kml->file, "\t\t\t\t<coordinates>");
    kmlWriteCoordinates(kml, coord->lat, coord->lon, coord->altitude, ',');
    fprintf(kml->file, "</coordinates>\n");
    fprintf(kml->file, "\t\t\t</Point>\n");
    fprintf(kml->file, "\t\t</Placemark>\n");
}

void kmlWriterDestroy(kmlWriter_t* kml, flightLog_t *log)
{
    if (!kml)
        return;

    if (kml->state == KMLWRITER_STATE_ERROR || kml->state == KMLWRITER_STATE_EMPTY)
        return;

    char datetime[128];

    strftime(datetime, sizeof(datetime), "%#c", &log->sysConfig.logStartTime);
    kml->file = fopen(kml->filename, "wb");

    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", kml->file);
    fputs("<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" xmlns:kml=\"http://www.opengis.net/kml/2.2\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n", kml->file);
    fputs("<Document>\n" , kml->file);

    fprintf(kml->file, "\t<name>Blackbox flight log %s</name>\n", kml->filename);
    fprintf(kml->file, "\t<visibility>1</visibility>\n");
    fprintf(kml->file, "\t<open>1</open>\n");
    fprintf(kml->file, "\t<snippet>Log started %s</snippet>\n", datetime);

    kmlWriteStyles(kml);

    fprintf(kml->file, "\t<Folder id =\"track\">\n");
    fprintf(kml->file, "\t\t<name>Blackbox flight log Tracks</name>\n");
    fprintf(kml->file, "\t\t<visibility>1</visibility>\n");
    fprintf(kml->file, "\t\t<open>1</open>\n");
    fprintf(kml->file, "\t\t<Snippet></Snippet>\n");
    fprintf(kml->file, "\t\t<description><![CDATA[&nbsp;]]></description>\n");


    int16_t min_altitude_value = INT16_MAX;
    int16_t max_altitude_value = INT16_MIN;
    placeValue_t placeValue[NB_EXTENDED_DATA_MAX];
    for (int i = 0; i < NB_EXTENDED_DATA_MAX; i++)
    {
        placeValue[i].min = UINT64_MAX;
        placeValue[i].max = 0;
    }

    coordList_t *min_altitude_coord;
    coordList_t *max_altitude_coord;
    coordList_t *placeCoordMin[NB_EXTENDED_DATA_MAX];
    coordList_t *placeCoordMax[NB_EXTENDED_DATA_MAX];

    double max_distance = 0.0;
    coordList_t *max_distance_coord = NULL;

    coordList_t *coordTrak = coordList;
    int track_num = 1;
    while ( coordTrak != NULL)
    {
        fprintf(kml->file, "\t\t<Placemark>\n");
        if (kml->trackModes)
        {
            fputs("\t\t\t<name>", kml->file);
            kmlWriteExtendedDataValue(kml, log, coordTrak->extended_data[change_track_data], change_track_data, true);
            fputs("</name>\n", kml->file);
        }
        else
        {
            fprintf(kml->file, "\t\t\t<name>Track %u</name>\n", track_num++);
        }
        fprintf(kml->file, "\t\t\t<styleUrl>#multiTrack</styleUrl>\n");
        fprintf(kml->file, "\t\t\t<gx:Track>\n");
        fprintf(kml->file, "\t\t\t\t<altitudeMode>absolute</altitudeMode>\n");

        coordList_t *coord;

        uint64_t prev_change_value = coordTrak->extended_data[change_track_data];
        for (coord = coordTrak; coord != NULL; coord = coord->next)
        {
            if (nb_extended_data > 0 && change_track_data >= 0)
            {
                if (coord->extended_data[change_track_data] != prev_change_value)
                    break;

                prev_change_value = coord->extended_data[change_track_data];
            }

            fprintf(kml->file, "\t\t\t\t<when>%04u-%02u-%02uT%02u:%02u:%02u.%06uZ</when>\n",
                log->sysConfig.logStartTime.tm_year + 1900, log->sysConfig.logStartTime.tm_mon + 1, log->sysConfig.logStartTime.tm_mday,
                coord->hours, coord->mins, coord->secs, coord->frac);
        }

        prev_change_value = coordTrak->extended_data[change_track_data];
        for (coord = coordTrak; coord != NULL; coord = coord->next)
        {
            if (nb_extended_data > 0 && change_track_data >= 0)
            {
                if (coord->extended_data[change_track_data] != prev_change_value)
                    break;

                prev_change_value = coord->extended_data[change_track_data];
            }

            fputs("\t\t\t\t<gx:coord>", kml->file);
            kmlWriteCoordinates(kml, coord->lat, coord->lon, coord->altitude, ' ');
            fputs("</gx:coord>\n", kml->file);

            if (altitude_place_flags & PLACE_MAX && coord->altitude > max_altitude_value) {
                max_altitude_value = coord->altitude;
                max_altitude_coord = coord;
            }
            if (altitude_place_flags & PLACE_MIN && coord->altitude < min_altitude_value) {
                min_altitude_value = coord->altitude;
                min_altitude_coord = coord;
            }
        }

        if (nb_extended_data > 0)
        {
            fprintf(kml->file, "\t\t\t\t<ExtendedData>\n");
            fprintf(kml->file, "\t\t\t\t\t<SchemaData schemaUrl = \"#schema\">\n");

            for (int e = 0; e < nb_extended_data; e++)
            {
                if (extended_data[e].frameBuffer != NULL)
                {
                    fprintf(kml->file, "\t\t\t\t\t\t<gx:SimpleArrayData name=\"%s\">\n", extended_data[e].name);
                    prev_change_value = coordTrak->extended_data[change_track_data];
                    for (coord = coordTrak; coord != NULL; coord = coord->next)
                    {
                        if (change_track_data >= 0)
                        {
                            if (coord->extended_data[change_track_data] != prev_change_value)
                                break;

                            prev_change_value = coord->extended_data[change_track_data];
                        }

                        fputs("\t\t\t\t\t\t\t<gx:value>", kml->file);
                        kmlWriteExtendedDataValue(kml, log, coord->extended_data[e], e, false);
                        fputs("</gx:value>\n", kml->file);

                        if (/*extended_data[e].placeFlags & PLACE_MAX && */coord->extended_data[e] > placeValue[e].max) {
                            placeValue[e].max = coord->extended_data[e];
                            placeCoordMax[e] = coord;
                        }
                        if (/*extended_data[e].placeFlags & PLACE_MIN && */coord->extended_data[e] < placeValue[e].min) {
                            placeValue[e].min = coord->extended_data[e];
                            placeCoordMin[e] = coord;
                        }
                    }
                    fprintf(kml->file, "\t\t\t\t\t\t</gx:SimpleArrayData>\n");
                }
                else
                {
                    extended_data[e].placeFlags = 0;
                }
            }

            fprintf(kml->file, "\t\t\t\t\t</SchemaData>\n");
            fprintf(kml->file, "\t\t\t\t</ExtendedData>\n");
        }

        coordTrak = coord;

        fprintf(kml->file, "\t\t\t</gx:Track>\n");
        fprintf(kml->file, "\t\t</Placemark>\n");
    }

    if (altitude_place_flags & PLACE_MAX)
    {
        kmlWritePlacemark(kml, log, "Max", max_altitude_coord, -1);
    }
    if (altitude_place_flags & PLACE_MIN)
    {
        kmlWritePlacemark(kml, log, "Min", min_altitude_coord, -1);
    }

    for (int e = 0; e < nb_extended_data; e++)
    {
        if (extended_data[e].placeFlags & PLACE_MAX)
        {
            kmlWritePlacemark(kml, log, "Max", placeCoordMax[e], e);
        }

        if (extended_data[e].placeFlags & PLACE_MIN)
        {
            kmlWritePlacemark(kml, log, "Min", placeCoordMin[e], e);
        }
    }

    int trkpt;
    coordList_t *coordPrev;
    if (color_extended_data >= 0)
    {
        uint64_t first_value = coordList->extended_data[color_extended_data];
        uint64_t min = placeCoordMin[color_extended_data]->extended_data[color_extended_data];
        uint64_t max = placeCoordMax[color_extended_data]->extended_data[color_extended_data];
        uint64_t delta = max - min;
        uint64_t half_delta = delta / 2;

        bool inverted_colors = false;
        if (first_value - min < max - first_value)
            inverted_colors = true;

        fprintf(kml->file, "\t<Folder id=\"colored_track\">\n");
        fprintf(kml->file, "\t\t<name>Colored %s</name>\n", extended_data[color_extended_data].name);
        fprintf(kml->file, "\t\t<visibility>1</visibility>\n");
        fprintf(kml->file, "\t\t<open>0</open>\n");
        fprintf(kml->file, "\t\t<Snippet></Snippet>\n");
        fprintf(kml->file, "\t\t<description><![CDATA[&nbsp;]]></description>\n");
        trkpt = 1;
        coordPrev = coordList;
        for (coordList_t *coord = coordList->next; coord != NULL; coord = coord->next)
        {
            uint64_t value = coord->extended_data[color_extended_data];
            value = value - min;
            if (inverted_colors)
            {
                value = delta - value;
            }
            int red, green;
            if (value < half_delta)
            {
                green = value * 255000 / half_delta / 1000;
                red = 255;
            }
            else
            {
                red = (delta - value) * 255000 / half_delta / 1000;
                green = 255;
            }

            fprintf(kml->file, "\t\t<Placemark>\n");

            fputs("\t\t\t<name>", kml->file);
            kmlWriteExtendedDataValue(kml, log, coord->extended_data[color_extended_data], color_extended_data, true);
            fputs("</name>\n", kml->file);

            fprintf(kml->file, "\t\t\t<TimeStamp>\n");
            fprintf(kml->file, "\t\t\t\t<when>%04u-%02u-%02uT%02u:%02u:%02u.%06uZ</when>\n",
                log->sysConfig.logStartTime.tm_year + 1900, log->sysConfig.logStartTime.tm_mon + 1, log->sysConfig.logStartTime.tm_mday,
                coord->hours, coord->mins, coord->secs, coord->frac);
            fprintf(kml->file, "\t\t\t</TimeStamp>\n");
            fprintf(kml->file, "\t\t\t<Style>\n");
            fprintf(kml->file, "\t\t\t\t<LineStyle>\n");
            fprintf(kml->file, "\t\t\t\t\t<color>FF00%02X%02X</color>\n", green, red);
            fprintf(kml->file, "\t\t\t\t\t<width>2</width>\n");
            fprintf(kml->file, "\t\t\t\t</LineStyle>\n");
            fprintf(kml->file, "\t\t\t</Style>\n");
            fprintf(kml->file, "\t\t\t<LineString>\n");
            fprintf(kml->file, "\t\t\t\t<tessellate>0</tessellate>\n");
            fprintf(kml->file, "\t\t\t\t<altitudeMode>absolute</altitudeMode>\n");
            fprintf(kml->file, "\t\t\t\t<coordinates>");
            kmlWriteCoordinates(kml, coordPrev->lat, coordPrev->lon, coordPrev->altitude, ',');
            fputs(" ", kml->file);
            kmlWriteCoordinates(kml, coord->lat, coord->lon, coord->altitude, ',');
            fprintf(kml->file, "</coordinates>\n");
            fprintf(kml->file, "\t\t\t</LineString>\n");
            fprintf(kml->file, "\t\t</Placemark>\n");
            coordPrev = coordPrev->next;
        }
        fprintf(kml->file, "\t</Folder>\n");
    }

    fprintf(kml->file, "\t<Folder id=\"shadow\">\n");
    fprintf(kml->file, "\t\t<name>Shadow</name>\n");
    fprintf(kml->file, "\t\t<visibility>1</visibility>\n");
    fprintf(kml->file, "\t\t<open>0</open>\n");
    fprintf(kml->file, "\t\t<Snippet></Snippet>\n");
    fprintf(kml->file, "\t\t<description><![CDATA[&nbsp;]]></description>\n");
    trkpt = 1;
    coordPrev = coordList;
    for (coordList_t *coord = coordList->next; coord != NULL; coord = coord->next)
    {
        fprintf(kml->file, "\t\t<Placemark>\n");
        fprintf(kml->file, "\t\t\t<name>trkpt %u</name>\n", trkpt++);
        fprintf(kml->file, "\t\t\t<TimeStamp>\n");
        fprintf(kml->file, "\t\t\t\t<when>%04u-%02u-%02uT%02u:%02u:%02u.%06uZ</when>\n",
            log->sysConfig.logStartTime.tm_year + 1900, log->sysConfig.logStartTime.tm_mon + 1, log->sysConfig.logStartTime.tm_mday,
            coord->hours, coord->mins, coord->secs, coord->frac);
        fprintf(kml->file, "\t\t\t</TimeStamp>\n");
        fprintf(kml->file, "\t\t\t<Style>\n");
        fprintf(kml->file, "\t\t\t\t<LineStyle>\n");
        fprintf(kml->file, "\t\t\t\t\t<color>99000000</color>\n");
        fprintf(kml->file, "\t\t\t\t\t<width>8</width>\n");
        fprintf(kml->file, "\t\t\t\t</LineStyle>\n");
        fprintf(kml->file, "\t\t\t</Style>\n");
        fprintf(kml->file, "\t\t\t<LineString>\n");
        fprintf(kml->file, "\t\t\t\t<tessellate>0</tessellate>\n");
        fprintf(kml->file, "\t\t\t\t<altitudeMode>clampToGround</altitudeMode>\n");
        fprintf(kml->file, "\t\t\t\t<coordinates>");
        kmlWriteCoordinates(kml, coordPrev->lat, coordPrev->lon, coordPrev->altitude, ',');
        fputs(" ", kml->file);
        kmlWriteCoordinates(kml, coord->lat, coord->lon, coord->altitude, ',');
        fprintf(kml->file, "</coordinates>\n");
        fprintf(kml->file, "\t\t\t</LineString>\n");
        fprintf(kml->file, "\t\t</Placemark>\n");
        coordPrev = coordPrev->next;
    }
    fprintf(kml->file, "\t</Folder>\n");

    clearCoordList();
    fprintf(kml->file, "\t</Folder>\n");

    if (kml->state != KMLWRITER_STATE_EMPTY) {
        fprintf(kml->file, KML_FILE_TRAILER);
        fclose(kml->file);
    }

    free(kml->filename);
    free(kml);
}
