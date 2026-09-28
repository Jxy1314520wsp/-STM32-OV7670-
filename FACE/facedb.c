/* ============================================================================
 * facedb.c - Face template database with internal flash persistence
 *   Flash sector: last 128 KB sector of STM32F407VE (0x08060000).
 * ==========================================================================*/
#include "facedb.h"
#include "img_proc.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include <stdio.h>
#include <string.h>

#define FACEDB_FLASH_BASE    0x08060000UL
#define FACEDB_FLASH_SECTOR  FLASH_SECTOR_7
#define FACEDB_MAGIC         0x31424446UL   /* "FDB1" little-endian bytes */

#define FACEDB_HEADER_SIZE   12U

typedef struct
{
    uint32_t magic;      /* 0x00 */
    uint16_t version;    /* 0x04 */
    uint16_t count;      /* 0x06 */
    uint32_t crc;        /* 0x08, covers version+count+entries with seed 0 */
} FaceDB_FlashHeader;

static FaceDB_Entry s_entries[FACEDB_MAX_ENTRIES];
static uint8_t s_count = 0U;

static uint8_t FlashWriteBuf(const uint8_t *src, uint32_t addr, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i += 4U)
    {
        uint32_t word = 0U;

        memcpy(&word, src + i, 4U);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, (uint64_t)word) != HAL_OK)
        {
            return 0U;
        }
    }
    return 1U;
}

static uint32_t FaceDBCrc(void)
{
    uint32_t crc;

    crc = ImgProc_CRC32((const uint8_t *)&s_entries, sizeof(s_entries), 0U);
    return crc;
}

uint8_t FaceDB_Load(void)
{
    const FaceDB_FlashHeader *hdr = (const FaceDB_FlashHeader *)FACEDB_FLASH_BASE;
    uint32_t crc;

    s_count = 0U;

    if (hdr->magic != FACEDB_MAGIC)
    {
        return 0U;
    }
    if (hdr->version != 1U)
    {
        return 0U;
    }
    if (hdr->count > FACEDB_MAX_ENTRIES)
    {
        return 0U;
    }

    crc = ImgProc_CRC32((const uint8_t *)(FACEDB_FLASH_BASE + FACEDB_HEADER_SIZE),
                        sizeof(s_entries), 0U);
    if (hdr->crc != crc)
    {
        return 0U;
    }

    memcpy(s_entries, (const void *)(FACEDB_FLASH_BASE + FACEDB_HEADER_SIZE), sizeof(s_entries));

    s_count = hdr->count;
    return s_count;
}

uint8_t FaceDB_Count(void)
{
    return s_count;
}

const FaceDB_Entry *FaceDB_Get(uint8_t index)
{
    uint8_t i;
    uint8_t n = 0U;

    for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
    {
        if (s_entries[i].id == 0U)
        {
            continue;
        }
        if (n == index)
        {
            return &s_entries[i];
        }
        n++;
    }
    return NULL;
}

uint8_t FaceDB_Add(uint8_t id, const char *name, const uint8_t *patch, const uint16_t *hist)
{
    uint8_t i;
    FaceDB_Entry *e = NULL;

    if (id == 0U || id > FACEDB_MAX_ENTRIES || patch == NULL || hist == NULL)
    {
        return 0U;
    }

    for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
    {
        if (s_entries[i].id == id)
        {
            e = &s_entries[i];
            break;
        }
    }
    if (e == NULL)
    {
        for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
        {
            if (s_entries[i].id == 0U)
            {
                e = &s_entries[i];
                break;
            }
        }
    }
    if (e == NULL)
    {
        return 0U;   /* DB full */
    }

    if (e->id == 0U)
    {
        if (name == NULL)
        {
            return 0U;   /* name required for a new person */
        }
        e->id = id;
        strncpy(e->name, name, FACEDB_NAME_LEN - 1U);
        e->name[FACEDB_NAME_LEN - 1U] = 0;
        e->reserved[0] = 0U;
        e->reserved[1] = 0U;
        e->nsamples = 0U;
        s_count++;
    }
    else if (name != NULL)
    {
        strncpy(e->name, name, FACEDB_NAME_LEN - 1U);
        e->name[FACEDB_NAME_LEN - 1U] = 0;
    }

    if (e->nsamples >= FACEDB_MAX_SAMPLES)
    {
        return 0U;   /* no room for another distance sample */
    }

    memcpy(e->patch[e->nsamples], patch, sizeof(e->patch[0]));
    memcpy(e->hist[e->nsamples], hist, sizeof(e->hist[0]));
    e->nsamples++;
    return 1U;
}

uint8_t FaceDB_Delete(uint8_t id)
{
    uint8_t i;

    for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
    {
        if (s_entries[i].id == id)
        {
            memset(&s_entries[i], 0, sizeof(s_entries[i]));
            s_count = 0U;
            for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
            {
                if (s_entries[i].id != 0U)
                {
                    s_count++;
                }
            }
            return 1U;
        }
    }
    return 0U;
}

void FaceDB_Clear(void)
{
    memset(s_entries, 0, sizeof(s_entries));
    s_count = 0U;
}

uint8_t FaceDB_Save(void)
{
    FaceDB_FlashHeader hdr;
    uint32_t addr = FACEDB_FLASH_BASE;
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0U;
    uint8_t ok = 0U;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return 0U;
    }

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FACEDB_FLASH_SECTOR;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0U;
    }

    hdr.magic = FACEDB_MAGIC;
    hdr.version = 1U;
    hdr.count = s_count;
    hdr.crc = FaceDBCrc();

    ok = FlashWriteBuf((const uint8_t *)&hdr, addr, sizeof(hdr));
    if (ok)
    {
        ok = FlashWriteBuf((const uint8_t *)s_entries,
                           addr + sizeof(hdr), sizeof(s_entries));
    }

    HAL_FLASH_Lock();
    return ok;
}

uint32_t FaceDB_PatchMAD(const uint8_t *a, const uint8_t *b)
{
    uint32_t sum_a = 0U;
    uint32_t sum_b = 0U;
    uint32_t diff = 0U;
    uint32_t i;

    for (i = 0; i < 1024U; i++)
    {
        sum_a += a[i];
        sum_b += b[i];
    }

    /* Subtract each patch mean before comparing. This removes the remaining
     * global level offset and makes the score less sensitive to exposure. */
    for (i = 0; i < 1024U; i++)
    {
        int32_t d = ((int32_t)(a[i] * 1024U) - (int32_t)sum_a) -
                    ((int32_t)(b[i] * 1024U) - (int32_t)sum_b);

        if (d < 0)
        {
            d = -d;
        }
        diff += (uint32_t)d;
    }

    return ((diff / 1024U) + 512U) / 1024U;
}

static uint32_t PatchMADShift(const uint8_t *a, const uint8_t *b, int8_t dy, int8_t dx)
{
    uint32_t sum_a = 0U;
    uint32_t sum_b = 0U;
    uint32_t diff = 0U;
    uint32_t n = 0U;
    int16_t y;
    int16_t x;

    for (y = 0; y < 32; y++)
    {
        int16_t yy = (int16_t)(y + dy);

        if (yy < 0 || yy >= 32)
        {
            continue;
        }
        for (x = 0; x < 32; x++)
        {
            int16_t xx = (int16_t)(x + dx);

            if (xx < 0 || xx >= 32)
            {
                continue;
            }
            sum_a += a[(uint32_t)y * 32U + x];
            sum_b += b[(uint32_t)yy * 32U + xx];
            n++;
        }
    }

    if (n == 0U)
    {
        return 0xFFFFFFFFU;
    }

    for (y = 0; y < 32; y++)
    {
        int16_t yy = (int16_t)(y + dy);

        if (yy < 0 || yy >= 32)
        {
            continue;
        }
        for (x = 0; x < 32; x++)
        {
            int16_t xx = (int16_t)(x + dx);
            int32_t d;

            if (xx < 0 || xx >= 32)
            {
                continue;
            }
            d = ((int32_t)(a[(uint32_t)y * 32U + x] * 1024U) - (int32_t)sum_a) -
                ((int32_t)(b[(uint32_t)yy * 32U + xx] * 1024U) - (int32_t)sum_b);
            if (d < 0)
            {
                d = -d;
            }
            diff += (uint32_t)d;
        }
    }

    return ((diff / n) + 512U) / 1024U;
}

/* Best-of-9 local translations: absorbs the small crop jitter caused by
 * face movement at 160x120 resolution. */
static uint32_t FaceDB_BestPatchMAD(const uint8_t *a, const uint8_t *b)
{
    uint32_t best = 0xFFFFFFFFU;
    int8_t dy;
    int8_t dx;

    for (dy = -1; dy <= 1; dy++)
    {
        for (dx = -1; dx <= 1; dx++)
        {
            uint32_t d = PatchMADShift(a, b, dy, dx);

            if (d < best)
            {
                best = d;
            }
        }
    }
    return best;
}

uint8_t FaceDB_Recognize(const uint8_t *patch, const uint16_t *hist,
                         uint8_t *out_id_lbph, uint32_t *out_dist_lbph,
                         uint8_t *out_id_mad, uint32_t *out_dist_mad)
{
    uint8_t i;
    uint32_t best_lbph = 0xFFFFFFFFU;
    uint32_t best_mad = 0xFFFFFFFFU;
    uint8_t best_id_lbph = 0U;
    uint8_t best_id_mad = 0U;

    for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
    {
        uint8_t s;

        if (s_entries[i].id == 0U)
        {
            continue;
        }

        for (s = 0; s < s_entries[i].nsamples; s++)
        {
            uint32_t d;
            uint32_t m;

            d = LBPH_ChiSquare(hist, s_entries[i].hist[s]);
            m = FaceDB_BestPatchMAD(patch, s_entries[i].patch[s]);

            if (d < best_lbph)
            {
                best_lbph = d;
                best_id_lbph = s_entries[i].id;
            }
            if (m < best_mad)
            {
                best_mad = m;
                best_id_mad = s_entries[i].id;
            }
        }
    }

    if (out_id_lbph) *out_id_lbph = best_id_lbph;
    if (out_dist_lbph) *out_dist_lbph = best_lbph;
    if (out_id_mad) *out_id_mad = best_id_mad;
    if (out_dist_mad) *out_dist_mad = best_mad;
    return (best_id_lbph != 0U);
}

void FaceDB_List(void)
{
    uint8_t i;

    printf("[FACEDB] count=%u\r\n", (unsigned)s_count);
    for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
    {
        if (s_entries[i].id != 0U)
        {
            printf("  id=%u name=%s samples=%u\r\n",
                   (unsigned)s_entries[i].id, s_entries[i].name,
                   (unsigned)s_entries[i].nsamples);
        }
    }
}
