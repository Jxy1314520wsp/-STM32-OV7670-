#ifndef __FACEDB_H
#define __FACEDB_H

#include <stdint.h>
#include "lbph.h"

/* ============================================================================
 * facedb.h - Face template database (RAM copy + internal flash persistence)
 *   One averaged LBPH template (944 x uint16) per person.
 * ==========================================================================*/

#define FACEDB_MAX_ENTRIES  4
#define FACEDB_MAX_SAMPLES  3
#define FACEDB_NAME_LEN     16

typedef struct
{
    uint8_t  id;                    /* 1..FACEDB_MAX_ENTRIES, 0 = empty */
    char     name[FACEDB_NAME_LEN];
    uint8_t  nsamples;              /* 1..FACEDB_MAX_SAMPLES (distance templates) */
    uint8_t  reserved[2];
    uint8_t  patch[FACEDB_MAX_SAMPLES][1024];
    uint16_t hist[FACEDB_MAX_SAMPLES][LBPH_HIST_SIZE];
} FaceDB_Entry;

/* Load from flash; returns number of entries. */
uint8_t FaceDB_Load(void);

uint8_t FaceDB_Count(void);
const FaceDB_Entry *FaceDB_Get(uint8_t index);

/* Append a distance sample to a person (name may be NULL for MORE). Returns 1 on success. */
uint8_t FaceDB_Add(uint8_t id, const char *name, const uint8_t *patch, const uint16_t *hist);

uint8_t FaceDB_Delete(uint8_t id);
void FaceDB_Clear(void);

/* Persist the whole DB to internal flash. Returns 1 on success. */
uint8_t FaceDB_Save(void);

/* Dual-metric recognition: returns best LBPH match and best MAD patch match.
 * Returns 1 if DB non-empty. */
uint8_t FaceDB_Recognize(const uint8_t *patch, const uint16_t *hist,
                         uint8_t *out_id_lbph, uint32_t *out_dist_lbph,
                         uint8_t *out_id_mad, uint32_t *out_dist_mad);

/* Mean absolute difference between two 32x32 patches. */
uint32_t FaceDB_PatchMAD(const uint8_t *a, const uint8_t *b);

/* Print the DB over UART (printf). */
void FaceDB_List(void);

#endif /* __FACEDB_H */
