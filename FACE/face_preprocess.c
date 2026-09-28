/* ============================================================================
 * face_preprocess.c - Face crop / normalize / quality
 * ==========================================================================*/
#include "face_preprocess.h"
#include <stddef.h>
#include <string.h>
#include "img_proc.h"

static uint16_t s_min_w = 44U;      /* min face width in 160x120 space */
static uint32_t s_min_q = 100U;     /* min Laplacian variance */
static uint8_t  s_min_mean = 40U;
static uint8_t  s_max_mean = 235U;

void FacePreprocess_SetMinWidth(uint16_t w)
{
    s_min_w = w;
}

void FacePreprocess_SetMinQuality(uint32_t q)
{
    s_min_q = q;
}

/* 3x3 box blur on a 32x32 image, in place (stabilizes LBP against noise). */
static void FaceBlur(uint8_t *img)
{
    static uint8_t tmp[FACE_PIXELS] __attribute__((section("ccmram")));
    uint16_t y;
    uint16_t x;

    for (y = 1; y < FACE_SIZE - 1U; y++)
    {
        for (x = 1; x < FACE_SIZE - 1U; x++)
        {
            uint32_t s = 0U;
            int16_t dy;
            int16_t dx;

            for (dy = -1; dy <= 1; dy++)
            {
                for (dx = -1; dx <= 1; dx++)
                {
                    s += img[(uint32_t)(y + dy) * FACE_SIZE + (x + dx)];
                }
            }
            tmp[(uint32_t)y * FACE_SIZE + x] = (uint8_t)(s / 9U);
        }
    }

    for (y = 1; y < FACE_SIZE - 1U; y++)
    {
        memcpy(&img[(uint32_t)y * FACE_SIZE + 1U], &tmp[(uint32_t)y * FACE_SIZE + 1U],
               FACE_SIZE - 2U);
    }
}

uint8_t FacePreprocess_Run(const uint8_t *gray160, const FaceDetect_Result *box,
                           FacePreprocess_Result *res)
{
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
    uint16_t cw;
    uint16_t ch;
    uint32_t sum = 0U;
    uint32_t i;

    res->ok = 0U;
    res->reason = FACE_PP_OK;

    if (gray160 == NULL || box == NULL || !box->found)
    {
        return 0U;
    }

    if (box->w < s_min_w)
    {
        res->reason = FACE_PP_ERR_FAR;
        return 0U;
    }

    /* Square center crop keeps aspect ratio stable when the face moves
     * closer/farther. Stretching a rectangular box to 32x32 was one of the
     * main reasons a same face looked different at different distances. */
    {
        uint16_t max_side = (box->w > box->h) ? box->w : box->h;
        uint32_t side = ((uint32_t)max_side * 110U) / 100U;
        int32_t cx = (int32_t)box->x + (int32_t)box->w / 2;
        int32_t cy = (int32_t)box->y + (int32_t)box->h / 2;
        int32_t xs0;
        int32_t ys0;

        if (side < (uint32_t)s_min_w)
        {
            side = (uint32_t)s_min_w;
        }
        if (side > 120U)
        {
            side = 120U;
        }

        xs0 = cx - (int32_t)(side / 2U);
        ys0 = cy - (int32_t)(side / 2U);

        if (xs0 < 0) xs0 = 0;
        if (ys0 < 0) ys0 = 0;
        if (xs0 > 160 - (int32_t)side) xs0 = 160 - (int32_t)side;
        if (ys0 > 120 - (int32_t)side) ys0 = 120 - (int32_t)side;

        x0 = (uint16_t)xs0;
        y0 = (uint16_t)ys0;
        x1 = (uint16_t)(xs0 + (int32_t)side - 1);
        y1 = (uint16_t)(ys0 + (int32_t)side - 1);
    }

    cw = (uint16_t)(x1 - x0 + 1U);
    ch = (uint16_t)(y1 - y0 + 1U);
    if (cw < 2U || ch < 2U)
    {
        res->reason = FACE_PP_ERR_FAR;
        return 0U;
    }

    res->x = x0;
    res->y = y0;
    res->w = cw;
    res->h = ch;

    /* Nearest-neighbor resize crop -> 32x32. */
    for (i = 0; i < FACE_PIXELS; i++)
    {
        uint16_t dx = (uint16_t)(i % FACE_SIZE);
        uint16_t dy = (uint16_t)(i / FACE_SIZE);
        uint16_t sx = (uint16_t)(((uint32_t)dx * cw) / FACE_SIZE);
        uint16_t sy = (uint16_t)(((uint32_t)dy * ch) / FACE_SIZE);
        res->face[i] = gray160[(uint32_t)(y0 + sy) * 160U + (x0 + sx)];
    }

    /* Histogram equalization. */
    ImgProc_HistEq(res->face, FACE_PIXELS);

    /* Mean brightness. */
    sum = 0U;
    for (i = 0; i < FACE_PIXELS; i++)
    {
        sum += res->face[i];
    }
    res->mean = (uint8_t)(sum / FACE_PIXELS);
    if (res->mean < s_min_mean || res->mean > s_max_mean)
    {
        res->reason = FACE_PP_ERR_LIGHT;
        return 0U;
    }

    /* Sharpness (before blur, so it reflects the original capture). */
    res->quality = ImgProc_LaplaceVar(res->face, FACE_SIZE, FACE_SIZE);
    if (res->quality < s_min_q)
    {
        res->reason = FACE_PP_ERR_BLUR;
        return 0U;
    }

    /* Smooth before LBP to stabilize local binary patterns on noise. */
    FaceBlur(res->face);

    res->ok = 1U;
    return 1U;
}
