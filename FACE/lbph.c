/* ============================================================================
 * lbph.c - Local Binary Pattern Histograms (LBPH)
 * ==========================================================================*/
#include "lbph.h"
#include <string.h>

static uint8_t s_uniform[256];
static uint8_t s_ready = 0U;

static uint8_t Transitions(uint8_t v)
{
    uint8_t cnt = 0U;
    uint8_t i;

    for (i = 0; i < 8U; i++)
    {
        uint8_t b0 = (uint8_t)((v >> i) & 1U);
        uint8_t b1 = (uint8_t)((v >> ((i + 1U) & 7U)) & 1U);

        if (b0 != b1)
        {
            cnt++;
        }
    }
    return cnt;
}

void LBPH_Init(void)
{
    uint16_t v;
    uint8_t next = 0U;

    if (s_ready)
    {
        return;
    }

    for (v = 0; v < 256U; v++)
    {
        if (Transitions((uint8_t)v) <= 2U)
        {
            s_uniform[v] = next++;
        }
        else
        {
            s_uniform[v] = 58U;
        }
    }
    s_ready = 1U;
}

void LBPH_Histogram(const uint8_t *face32, uint16_t *hist)
{
    static uint32_t raw[LBPH_HIST_SIZE] __attribute__((section("ccmram")));
    uint32_t total = 0U;
    uint16_t y;
    uint16_t x;
    uint32_t i;

    if (!s_ready)
    {
        LBPH_Init();
    }

    memset(raw, 0, sizeof(raw));

    for (y = 1; y < LBPH_FACE_SIZE - 1U; y++)
    {
        for (x = 1; x < LBPH_FACE_SIZE - 1U; x++)
        {
            uint8_t c = face32[(uint32_t)y * LBPH_FACE_SIZE + x];
            uint8_t code = 0U;
            uint8_t bit = 0U;
            uint8_t n;

            /* 8 neighbors clockwise starting from right. */
            {
                uint8_t nb[8];

                nb[0] = face32[(uint32_t)y * LBPH_FACE_SIZE + (x + 1U)];
                nb[1] = face32[(uint32_t)(y + 1U) * LBPH_FACE_SIZE + (x + 1U)];
                nb[2] = face32[(uint32_t)(y + 1U) * LBPH_FACE_SIZE + x];
                nb[3] = face32[(uint32_t)(y + 1U) * LBPH_FACE_SIZE + (x - 1U)];
                nb[4] = face32[(uint32_t)y * LBPH_FACE_SIZE + (x - 1U)];
                nb[5] = face32[(uint32_t)(y - 1U) * LBPH_FACE_SIZE + (x - 1U)];
                nb[6] = face32[(uint32_t)(y - 1U) * LBPH_FACE_SIZE + x];
                nb[7] = face32[(uint32_t)(y - 1U) * LBPH_FACE_SIZE + (x + 1U)];

                for (n = 0; n < 8U; n++)
                {
                    if (nb[n] >= c)
                    {
                        code |= (uint8_t)(1U << bit);
                    }
                    bit++;
                }
            }

            {
                uint8_t cell_x = (uint8_t)(x / (LBPH_FACE_SIZE / LBPH_CELLS_X));
                uint8_t cell_y = (uint8_t)(y / (LBPH_FACE_SIZE / LBPH_CELLS_Y));
                uint32_t idx = ((uint32_t)cell_y * LBPH_CELLS_X + cell_x) * LBPH_BINS + s_uniform[code];

                raw[idx]++;
                total++;
            }
        }
    }

    if (total == 0U)
    {
        total = 1U;
    }

    /* Normalize to LBPH_NORM_SUM. */
    for (i = 0; i < LBPH_HIST_SIZE; i++)
    {
        hist[i] = (uint16_t)((raw[i] * LBPH_NORM_SUM + total / 2U) / total);
    }
}

uint32_t LBPH_ChiSquare(const uint16_t *a, const uint16_t *b)
{
    uint64_t acc = 0U;
    uint32_t i;

    for (i = 0; i < LBPH_HIST_SIZE; i++)
    {
        int32_t d = (int32_t)a[i] - (int32_t)b[i];
        uint32_t sum = (uint32_t)a[i] + (uint32_t)b[i] + 1U;

        if (d < 0)
        {
            d = -d;
        }
        acc += ((uint32_t)d * (uint32_t)d) / sum;
        if (acc > 0xFFFFFFFFU)
        {
            return 0xFFFFFFFFU;
        }
    }
    return (uint32_t)acc;
}

void LBPH_Average(const uint32_t *sum, uint32_t n, uint16_t *out)
{
    uint32_t i;

    if (n == 0U)
    {
        n = 1U;
    }
    for (i = 0; i < LBPH_HIST_SIZE; i++)
    {
        out[i] = (uint16_t)((sum[i] + n / 2U) / n);
    }
}
