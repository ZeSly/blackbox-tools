#ifndef KMLWRITER_H_
#define KMLWRITER_H_

#include <stdint.h>
#include <stdio.h>

#include "parser.h"

typedef enum KmlWriterState {
    KMLWRITER_STATE_EMPTY = 0,
    KMLWRITER_STATE_WRITING_TRACK,
} KmlWriterState;

typedef struct kmlWriter_t {
    KmlWriterState state;
    FILE *file;
    char *filename;
    uint64_t startTime;
} kmlWriter_t;

void kmlWriterAddPoint(kmlWriter_t *kml, flightLog_t *log, int64_t time, int64_t *frame, int64_t *bufferedMainFrame, int64_t *bufferedSlowFrame);
kmlWriter_t* kmlWriterCreate(const char *filename);
void kmlWriterDestroy(kmlWriter_t* kml, flightLog_t *log);

#endif
