#ifndef __FACE_APP_H
#define __FACE_APP_H

#include <stdint.h>
#include "lbph.h"
#include "face_preprocess.h"

/* ============================================================================
 * face_app.h - Application layer: serial commands, enrollment, recognition
 * ==========================================================================*/

#define FACE_APP_ENROLL_SAMPLES   3
#define FACE_APP_ENROLL_MIN_W     56
#define FACE_APP_DEFAULT_THR      350000U
#define FACE_APP_DEFAULT_MAD_THR  20U

typedef struct
{
    /* enrollment */
    uint8_t  enrolling;
    uint8_t  enroll_more;      /* 1 = append another distance sample */
    uint8_t  enroll_id;
    char     enroll_name[16];
    uint8_t  enroll_count;
    uint32_t enroll_sum[LBPH_HIST_SIZE];
    uint32_t enroll_patch_sum[1024];

    /* database */
    uint8_t  db_count;

    /* last recognition result */
    uint8_t  last_id;
    char     last_name[16];
    uint32_t last_dist;      /* LBPH distance */
    uint32_t last_mad;       /* patch MAD distance */
    uint8_t  last_unknown;

    uint32_t threshold;        /* LBPH chi-square threshold */
    uint32_t mad_threshold;    /* patch MAD threshold */

    /* stats */
    uint32_t fps;
    uint32_t frame_count;
    uint32_t fps_base;
    uint32_t sec_ms;

    uint8_t  last_face[FACE_PIXELS];
    uint8_t  last_face_valid;

    uint32_t ms_gray;
    uint32_t ms_detect;
    uint32_t ms_preprocess;
    uint32_t ms_lbph;
} FaceApp_State;

extern FaceApp_State g_face;
extern volatile uint8_t g_display_scale;

void FaceApp_Init(void);

/* Poll UART and execute commands (call every loop). */
void FaceApp_ProcessCommand(void);

/* Feed one processed face into the app (recognition or enrollment). */
void FaceApp_OnFace(const uint8_t *face32, const uint16_t *hist,
                    uint16_t box_w, uint8_t pp_ok, uint8_t pp_reason);

/* Frame-rate tick (call once per processed frame). */
void FaceApp_Tick(void);

const char *FaceApp_ModeText(void);

#endif /* __FACE_APP_H */
