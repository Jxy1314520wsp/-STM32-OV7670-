/* ============================================================================
 * face_detect.c - Skin-color + geometry face detection (v1)
 * ==========================================================================*/
#include "face_detect.h"
#include <string.h>

#define DET_W   80
#define DET_H   60
#define DET_MAX_COMP  24

static uint8_t s_mask[DET_W * DET_H] __attribute__((section("ccmram")));
static uint8_t s_tmp[DET_W * DET_H] __attribute__((section("ccmram")));
static uint8_t s_labels[DET_W * DET_H] __attribute__((section("ccmram")));
static ImgProc_Component s_comps[DET_MAX_COMP];

void FaceDetect_DefaultConfig(FaceDetect_Config *cfg)
{
    cfg->skin.y_min = 50U;
    cfg->skin.y_max = 255U;
    cfg->skin.cb_min = 77U;
    cfg->skin.cb_max = 127U;
    cfg->skin.cr_min = 133U;
    cfg->skin.cr_max = 173U;
    cfg->min_area = 60U;
    cfg->min_face_w = 36U;
    cfg->min_aspect_x10 = 6U;
    cfg->max_aspect_x10 = 20U;
    cfg->min_fill = 30U;
    cfg->eye_var_min = 0U;   /* 0 = disabled; set >0 to require an eye band */
}

/* Eye-band variance check on the original 160x120 gray image. */
static uint8_t FaceCheck(const uint8_t *gray, uint16_t x, uint16_t y,
                         uint16_t w, uint16_t h, uint8_t var_min)
{
    uint32_t sum = 0U;
    uint32_t sq = 0U;
    uint32_t n = 0U;
    uint16_t y0;
    uint16_t y1;
    uint16_t yy;
    uint16_t xx;

    if (gray == NULL)
    {
        return 1U;
    }
    if (w < 4U || h < 8U)
    {
        return 1U;
    }

    y0 = y + (uint16_t)((uint32_t)h * 30U / 100U);
    y1 = y + (uint16_t)((uint32_t)h * 55U / 100U);
    if (y1 <= y0)
    {
        y1 = (uint16_t)(y0 + 1U);
    }
    if (y1 >= 120U)
    {
        y1 = 119U;
    }

    for (yy = y0; yy <= y1; yy++)
    {
        for (xx = x; xx < x + w; xx++)
        {
            if (xx >= 160U)
            {
                break;
            }
            sum += gray[(uint32_t)yy * 160U + xx];
            sq += (uint32_t)gray[(uint32_t)yy * 160U + xx] * gray[(uint32_t)yy * 160U + xx];
            n++;
        }
    }

    if (n == 0U)
    {
        return 1U;
    }

    {
        uint32_t mean = sum / n;
        uint32_t sqn = sq / n;
        uint32_t var;

        if (sqn > (mean * mean))
        {
            var = sqn - (mean * mean);
        }
        else
        {
            var = 0U;
        }
        if (var < var_min)
        {
            return 0U;
        }
    }
    return 1U;
}

uint8_t FaceDetect_Run(const uint8_t *rgb565, const uint8_t *gray,
                       const FaceDetect_Config *cfg, FaceDetect_Result *res)
{
    uint8_t ncomp;
    uint8_t i;
    uint8_t best_pass = 0xFFU;   /* index of largest candidate passing feature check */
    uint8_t best_geom = 0xFFU;   /* index of largest geometry-only candidate */
    uint32_t best_pass_area = 0U;
    uint32_t best_geom_area = 0U;

    res->found = 0U;

    if (rgb565 == NULL || cfg == NULL)
    {
        return 0U;
    }

    /* 1) Skin mask at half resolution. */
    ImgProc_SkinMask2x(rgb565, s_mask, 160U, 120U, DET_W, DET_H, &cfg->skin);

    /* 2) Morphological open (erode then dilate) to remove speckles. */
    ImgProc_Erode3(s_mask, s_tmp, DET_W, DET_H);
    ImgProc_Dilate3(s_tmp, s_mask, DET_W, DET_H);

    /* 3) Connected components. */
    ncomp = ImgProc_Label(s_mask, s_labels, DET_W, DET_H, s_comps, DET_MAX_COMP);

    /* 4) Geometry + feature filtering. */
    for (i = 0; i < ncomp; i++)
    {
        uint16_t bw = (uint16_t)(s_comps[i].x1 - s_comps[i].x0 + 1U);
        uint16_t bh = (uint16_t)(s_comps[i].y1 - s_comps[i].y0 + 1U);
        uint32_t box_area = (uint32_t)bw * bh;
        uint16_t aspect_x10;
        uint16_t ow;
        uint16_t oh;
        uint16_t ox;
        uint16_t oy;
        uint8_t  pass;

        if (box_area == 0U)
        {
            continue;
        }

        if (s_comps[i].area < cfg->min_area)
        {
            continue;
        }

        aspect_x10 = (uint16_t)(((uint32_t)bw * 10U) / bh);
        if (aspect_x10 < cfg->min_aspect_x10 || aspect_x10 > cfg->max_aspect_x10)
        {
            continue;
        }

        if (((s_comps[i].area * 100U) / box_area) < cfg->min_fill)
        {
            continue;
        }

        /* Original-scale box. */
        ox = (uint16_t)(s_comps[i].x0 * 2U);
        oy = (uint16_t)(s_comps[i].y0 * 2U);
        ow = (uint16_t)(bw * 2U);
        oh = (uint16_t)(bh * 2U);

        if (ow < cfg->min_face_w)
        {
            continue;
        }

        /* Track best geometry-only candidate. */
        if (s_comps[i].area > best_geom_area)
        {
            best_geom_area = s_comps[i].area;
            best_geom = i;
        }

        /* Feature check (eyes). */
        pass = FaceCheck(gray, ox, oy, ow, oh, cfg->eye_var_min);
        if (pass && s_comps[i].area > best_pass_area)
        {
            best_pass_area = s_comps[i].area;
            best_pass = i;
        }
    }

    if (best_pass != 0xFFU)
    {
        i = best_pass;
    }
    else if (best_geom != 0xFFU)
    {
        i = best_geom;
    }
    else
    {
        return 0U;
    }

    res->x = (uint16_t)(s_comps[i].x0 * 2U);
    res->y = (uint16_t)(s_comps[i].y0 * 2U);
    res->w = (uint16_t)((s_comps[i].x1 - s_comps[i].x0 + 1U) * 2U);
    res->h = (uint16_t)((s_comps[i].y1 - s_comps[i].y0 + 1U) * 2U);
    res->found = 1U;
    return 1U;
}
