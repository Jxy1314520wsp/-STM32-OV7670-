/* ============================================================================
 * face_app.c - Application layer for face recognition
 *   Recognition = dual gate: patch MAD (primary) + LBPH chi-square (secondary).
 * ==========================================================================*/
#include "face_app.h"
#include "facedb.h"
#include "usart.h"
#include "OV7670.h"
#include <stdio.h>
#include <string.h>

FaceApp_State g_face;
volatile uint8_t g_display_scale = 2U;

static uint8_t s_boot_banner = 0U;
static uint8_t s_last_enroll_hint = 0xFFU;
static uint16_t s_avg_hist[LBPH_HIST_SIZE];
static uint8_t s_avg_patch[1024];

void FaceApp_Init(void)
{
    memset(&g_face, 0, sizeof(g_face));
    LBPH_Init();
    g_face.db_count = FaceDB_Load();
    g_face.threshold = FACE_APP_DEFAULT_THR;
    g_face.mad_threshold = FACE_APP_DEFAULT_MAD_THR;
    g_face.sec_ms = HAL_GetTick();

    if (!s_boot_banner)
    {
        printf("\r\n[FACE] STM32F407 face recognition ready\r\n");
        printf("[FACE] DB entries loaded: %u\r\n", (unsigned)g_face.db_count);
        printf("[FACE] commands: HELP ADD MORE DEL LIST CLEAR THR THRM DIST DISP FPS REG\r\n");
        s_boot_banner = 1U;
    }
}

const char *FaceApp_ModeText(void)
{
    if (g_face.enrolling)
    {
        return "ENROLL";
    }
    return "RECOG";
}

static void CmdHelp(void)
{
    printf("[FACE] ADD <id> <name>  enroll current face (3 samples)\r\n");
    printf("[FACE] MORE <id>        append another distance sample\r\n");
    printf("[FACE] DEL <id>         delete person\r\n");
    printf("[FACE] LIST             list database\r\n");
    printf("[FACE] CLEAR            clear database\r\n");
    printf("[FACE] THR <value>      set LBPH threshold\r\n");
    printf("[FACE] THRM <value>     set MAD threshold\r\n");
    printf("[FACE] DIST             show thresholds and last distances\r\n");
    printf("[FACE] DISP <0|1|2>     display mode\r\n");
printf("[FACE] REG <reg> <val>  write OV7670 register (hex)\r\n");
printf("[FACE] FPS              show current fps\r\n");
printf("[FACE] HELP             this help\r\n");
}

static const char *SkipSpaces(const char *p)
{
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    return p;
}

static void CmdAdd(const char *args)
{
    unsigned id;
    char name[16];

    name[0] = 0;
    if (sscanf(args, "%u %15s", &id, name) != 2)
    {
        printf("[FACE] usage: ADD <id> <name>\r\n");
        return;
    }
    if (id == 0U || id > FACEDB_MAX_ENTRIES)
    {
        printf("[FACE] id must be 1..%u\r\n", (unsigned)FACEDB_MAX_ENTRIES);
        return;
    }
    if (g_face.enrolling)
    {
        printf("[FACE] already enrolling\r\n");
        return;
    }

    g_face.enrolling = 1U;
    g_face.enroll_more = 0U;
    g_face.enroll_id = (uint8_t)id;
    strncpy(g_face.enroll_name, name, sizeof(g_face.enroll_name) - 1U);
    g_face.enroll_name[sizeof(g_face.enroll_name) - 1U] = 0;
    g_face.enroll_count = 0U;
    memset(g_face.enroll_sum, 0, sizeof(g_face.enroll_sum));
    memset(g_face.enroll_patch_sum, 0, sizeof(g_face.enroll_patch_sum));
    s_last_enroll_hint = 0xFFU;

    printf("[FACE] enrolling id=%u name=%s, %u samples\r\n",
           (unsigned)id, g_face.enroll_name, (unsigned)FACE_APP_ENROLL_SAMPLES);
}

static void CmdMore(const char *args)
{
    unsigned id;

    if (sscanf(args, "%u", &id) != 1)
    {
        printf("[FACE] usage: MORE <id>\r\n");
        return;
    }
    if (id == 0U || id > FACEDB_MAX_ENTRIES)
    {
        printf("[FACE] id must be 1..%u\r\n", (unsigned)FACEDB_MAX_ENTRIES);
        return;
    }
    if (g_face.enrolling)
    {
        printf("[FACE] already enrolling\r\n");
        return;
    }

    g_face.enrolling = 1U;
    g_face.enroll_more = 1U;
    g_face.enroll_id = (uint8_t)id;
    g_face.enroll_name[0] = 0;
    g_face.enroll_count = 0U;
    memset(g_face.enroll_sum, 0, sizeof(g_face.enroll_sum));
    memset(g_face.enroll_patch_sum, 0, sizeof(g_face.enroll_patch_sum));
    s_last_enroll_hint = 0xFFU;

    printf("[FACE] appending distance sample to id=%u (%u samples)\r\n",
           id, (unsigned)FACE_APP_ENROLL_SAMPLES);
}

static void CmdDel(const char *args)
{
    unsigned id;

    if (sscanf(args, "%u", &id) != 1)
    {
        printf("[FACE] usage: DEL <id>\r\n");
        return;
    }
    if (FaceDB_Delete((uint8_t)id))
    {
        if (FaceDB_Save())
        {
            printf("[FACE] deleted id=%u\r\n", id);
        }
        else
        {
            printf("[FACE] deleted id=%u, SAVE FAILED\r\n", id);
        }
    }
    else
    {
        printf("[FACE] id=%u not found\r\n", id);
    }
    g_face.db_count = FaceDB_Count();
}

static void CmdClear(void)
{
    FaceDB_Clear();
    if (FaceDB_Save())
    {
        printf("[FACE] database cleared\r\n");
    }
    else
    {
        printf("[FACE] cleared (RAM), SAVE FAILED\r\n");
    }
    g_face.db_count = FaceDB_Count();
}

static void CmdThr(const char *args)
{
    unsigned long v;

    if (sscanf(args, "%lu", &v) != 1)
    {
        printf("[FACE] LBPH threshold=%lu\r\n", (unsigned long)g_face.threshold);
        return;
    }
    g_face.threshold = (uint32_t)v;
    printf("[FACE] LBPH threshold=%lu\r\n", (unsigned long)g_face.threshold);
}

static void CmdThrM(const char *args)
{
    unsigned long v;

    if (sscanf(args, "%lu", &v) != 1)
    {
        printf("[FACE] MAD threshold=%lu\r\n", (unsigned long)g_face.mad_threshold);
        return;
    }
    g_face.mad_threshold = (uint32_t)v;
    printf("[FACE] MAD threshold=%lu\r\n", (unsigned long)g_face.mad_threshold);
}

static void CmdDisp(const char *args)
{
    unsigned v;

    if (sscanf(args, "%u", &v) != 1 || v > 2U)
    {
        printf("[FACE] usage: DISP 0(off) 1(160x120) 2(320x240)\r\n");
        return;
    }
    g_display_scale = (uint8_t)v;
    printf("[FACE] display scale=%u\r\n", (unsigned)g_display_scale);
}

static void CmdReg(const char *args)
{
    unsigned reg;
    unsigned val;

    if (sscanf(args, "%x %x", &reg, &val) != 2 || reg > 0xFFU || val > 0xFFU)
    {
        printf("[FACE] usage: REG <reg_hex> <val_hex>\r\n");
        return;
    }
    if (OV7670_WriteRegister((uint8_t)reg, (uint8_t)val) == HAL_OK)
    {
        printf("[FACE] reg 0x%02X = 0x%02X\r\n", reg, val);
    }
    else
    {
        printf("[FACE] reg write failed\r\n");
    }
}

static void CmdFps(void)
{
    printf("[FACE] fps=%lu\r\n", (unsigned long)g_face.fps);
}

static void CmdDist(void)
{
    printf("[FACE] thr_lbph=%lu thr_mad=%lu last_lbph=%lu last_mad=%lu id=%u unknown=%u\r\n",
           (unsigned long)g_face.threshold,
           (unsigned long)g_face.mad_threshold,
           (unsigned long)g_face.last_dist,
           (unsigned long)g_face.last_mad,
           (unsigned)g_face.last_id,
           (unsigned)g_face.last_unknown);
}

void FaceApp_ProcessCommand(void)
{
    char cmd[16];
    const char *rest;

    if ((g_usart_rx_sta & 0x8000U) == 0U)
    {
        return;
    }

    g_usart_rx_buf[g_usart_rx_sta & 0x3FFFU] = 0;
    g_usart_rx_sta = 0;

    cmd[0] = 0;
    if (sscanf((const char *)g_usart_rx_buf, "%15s", cmd) != 1)
    {
        return;
    }

    rest = SkipSpaces((const char *)g_usart_rx_buf);
    while (*rest && *rest != ' ' && *rest != '\t')
    {
        rest++;
    }
    rest = SkipSpaces(rest);

    if (strcmp(cmd, "HELP") == 0)
    {
        CmdHelp();
    }
    else if (strcmp(cmd, "ADD") == 0)
    {
        CmdAdd(rest);
    }
    else if (strcmp(cmd, "MORE") == 0)
    {
        CmdMore(rest);
    }
    else if (strcmp(cmd, "DEL") == 0)
    {
        CmdDel(rest);
    }
    else if (strcmp(cmd, "CLEAR") == 0)
    {
        CmdClear();
    }
    else if (strcmp(cmd, "THR") == 0)
    {
        CmdThr(rest);
    }
    else if (strcmp(cmd, "THRM") == 0)
    {
        CmdThrM(rest);
    }
    else if (strcmp(cmd, "DISP") == 0)
    {
        CmdDisp(rest);
    }
    else if (strcmp(cmd, "REG") == 0)
    {
        CmdReg(rest);
    }
    else if (strcmp(cmd, "FPS") == 0)
    {
        CmdFps();
    }
    else if (strcmp(cmd, "DIST") == 0)
    {
        CmdDist();
    }
    else if (strcmp(cmd, "LIST") == 0)
    {
        FaceDB_List();
    }
    else
    {
        printf("[FACE] unknown: %s\r\n", cmd);
    }
}

void FaceApp_OnFace(const uint8_t *face32, const uint16_t *hist,
                    uint16_t box_w, uint8_t pp_ok, uint8_t pp_reason)
{
    if (g_face.enrolling)
    {
        if (!pp_ok)
        {
            if (s_last_enroll_hint != pp_reason)
            {
                s_last_enroll_hint = pp_reason;
                if (pp_reason == FACE_PP_ERR_FAR)
                {
                    printf("[FACE] enroll: face too small\r\n");
                }
                else if (pp_reason == FACE_PP_ERR_BLUR)
                {
                    printf("[FACE] enroll: image blurry\r\n");
                }
                else if (pp_reason == FACE_PP_ERR_LIGHT)
                {
                    printf("[FACE] enroll: bad lighting\r\n");
                }
            }
            return;
        }
        if (box_w < FACE_APP_ENROLL_MIN_W)
        {
            if (s_last_enroll_hint != 0xFEU)
            {
                s_last_enroll_hint = 0xFEU;
                printf("[FACE] enroll: come closer\r\n");
            }
            return;
        }
        if (face32 == NULL || hist == NULL)
        {
            return;
        }

        {
            uint32_t i;

            for (i = 0; i < LBPH_HIST_SIZE; i++)
            {
                g_face.enroll_sum[i] += hist[i];
            }
            for (i = 0; i < 1024U; i++)
            {
                g_face.enroll_patch_sum[i] += face32[i];
            }
        }
        g_face.enroll_count++;

        printf("[FACE] enroll sample %u/%u\r\n",
               (unsigned)g_face.enroll_count, (unsigned)FACE_APP_ENROLL_SAMPLES);

        if (g_face.enroll_count >= FACE_APP_ENROLL_SAMPLES)
        {
            uint32_t i;

            LBPH_Average(g_face.enroll_sum, g_face.enroll_count, s_avg_hist);
            for (i = 0; i < 1024U; i++)
            {
                s_avg_patch[i] = (uint8_t)((g_face.enroll_patch_sum[i] +
                                            g_face.enroll_count / 2U) /
                                           g_face.enroll_count);
            }

            if (FaceDB_Add(g_face.enroll_id,
                           g_face.enroll_more ? NULL : g_face.enroll_name,
                           s_avg_patch, s_avg_hist))
            {
                printf("[FACE] saving database...\r\n");
                if (FaceDB_Save())
                {
                    printf("[FACE] registered id=%u name=%s\r\n",
                           (unsigned)g_face.enroll_id, g_face.enroll_name);
                }
                else
                {
                    printf("[FACE] SAVE FAILED\r\n");
                }
            }
            else
            {
                printf("[FACE] register failed (DB full?)\r\n");
            }
            g_face.enrolling = 0U;
            g_face.enroll_count = 0U;
            g_face.db_count = FaceDB_Count();
        }
        return;
    }

    /* Recognition path. */
    if (face32 == NULL || hist == NULL || !pp_ok)
    {
        return;
    }
    {
        uint8_t id_lbph = 0U;
        uint8_t id_mad = 0U;
        uint32_t dist_lbph = 0U;
        uint32_t dist_mad = 0U;
        uint8_t id = 0U;
        uint8_t ok = 0U;

        if (g_face.db_count == 0U)
        {
            g_face.last_id = 0U;
            g_face.last_unknown = 1U;
            g_face.last_dist = 0U;
            g_face.last_mad = 0U;
        }
        else if (FaceDB_Recognize(face32, hist, &id_lbph, &dist_lbph, &id_mad, &dist_mad))
        {
            g_face.last_dist = dist_lbph;
            g_face.last_mad = dist_mad;

            if ((id_lbph == id_mad) && id_lbph != 0U &&
                (dist_lbph <= g_face.threshold) &&
                (dist_mad <= g_face.mad_threshold))
            {
                id = id_lbph;
                ok = 1U;
            }

            g_face.last_id = id;
            g_face.last_unknown = ok ? 0U : 1U;

            if (ok)
            {
                const FaceDB_Entry *e = NULL;
                uint8_t i;

                for (i = 0; i < FACEDB_MAX_ENTRIES; i++)
                {
                    const FaceDB_Entry *t = FaceDB_Get(i);

                    if (t != NULL && t->id == id)
                    {
                        e = t;
                        break;
                    }
                }
                if (e != NULL)
                {
                    strncpy(g_face.last_name, e->name, sizeof(g_face.last_name) - 1U);
                    g_face.last_name[sizeof(g_face.last_name) - 1U] = 0;
                }
                else
                {
                    g_face.last_name[0] = 0;
                }
            }
        }
        else
        {
            g_face.last_id = 0U;
            g_face.last_unknown = 1U;
            g_face.last_dist = 0U;
            g_face.last_mad = 0U;
        }
    }
}

void FaceApp_Tick(void)
{
    uint32_t now = HAL_GetTick();

    g_face.frame_count++;
    if (now - g_face.sec_ms >= 1000U)
    {
        g_face.fps = g_face.frame_count - g_face.fps_base;
        g_face.fps_base = g_face.frame_count;
        g_face.sec_ms = now;
    }
}
