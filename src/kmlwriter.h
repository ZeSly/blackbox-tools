#ifndef KMLWRITER_H_
#define KMLWRITER_H_

#include <stdint.h>
#include <stdio.h>

#include "parser.h"

typedef enum KmlWriterState {
    KMLWRITER_STATE_EMPTY = 0,
    KMLWRITER_STATE_WRITING_TRACK,
    KMLWRITER_STATE_ERROR
} KmlWriterState;

typedef struct kmlWriter_t {
    KmlWriterState state;
    FILE *file;
    char *filename;
    uint64_t startTime;
    bool trackModes;
} kmlWriter_t;

void kmlSetInfos(char *optarg);
bool kmlSetMinimums(char *optarg);
bool kmlSetMaximums(char *optarg);
bool kmlSetColor(char *optarg);

void kmlWriterAddPoint(kmlWriter_t *kml, flightLog_t *log, int64_t time, int64_t *frame, int64_t *bufferedMainFrame, int64_t *bufferedSlowFrame);
kmlWriter_t* kmlWriterCreate(const char *filename, bool trackModes);
void kmlWriterDestroy(kmlWriter_t* kml, flightLog_t *log);

#endif
