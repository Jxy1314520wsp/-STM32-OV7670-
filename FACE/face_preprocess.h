#ifndef __FACE_PREPROCESS_H
#define __FACE_PREPROCESS_H

#include <stdint.h>
#include "face_detect.h"

/* ============================================================================
 * face_preprocess.h - Crop a detected face, normalize to 32x32 grayscale
 * and evaluate capture quality.
 * ==========================================================================*/

#define FACE_SIZE        32
#define FACE_PIXELS      (FACE_SIZE * FACE_SIZE)

#define FACE_PP_OK         0
#define FACE_PP_ERR_FAR    1   /* face too small / too far */
#define FACE_PP_ERR_BLUR   2   /* low sharpness */
#define FACE_PP_ERR_LIGHT  3   /* too dark or overexposed */

typedef struct
{
    uint16_t x, y;        /* expanded region in 160x120 */
    uint16_t w, h;
    uint8_t  face[FACE_PIXELS];   /* normalized 32x32 grayscale */
    uint32_t quality;     /* Laplacian variance */
    uint8_t  mean;        /* mean brightness after eq (informational) */
    uint8_t  ok;
    uint8_t  reason;
} FacePreprocess_Result;

void FacePreprocess_SetMinWidth(uint16_t w);
void FacePreprocess_SetMinQuality(uint32_t q);

uint8_t FacePreprocess_Run(const uint8_t *gray160, const FaceDetect_Result *box,
                           FacePreprocess_Result *res);

#endif /* __FACE_PREPROCESS_H */
