/* ============================================================================
 * mian.c - Face recognition: OV7670 QQVGA 160x120 -> detection -> LBPH
 *
 * Pipeline per frame:
 *   OV7670_ToGray -> FaceDetect_Run -> FacePreprocess_Run -> LBPH_Histogram
 *   -> FaceApp_OnFace (recognition or enrollment) -> LCD display + overlay
 *
 * Robustness:
 *   - DCMI/DMA errors recover instead of hanging.
 *   - Frame timeout uses both HAL tick and DWT cycles.
 *   - IWDG watchdog resets the MCU if the main loop ever hangs.
 * ==========================================================================*/
#include "stm32f4xx_hal.h"
#include "sys.h"
#include "delay.h"
#include "LCD.h"
#include "usart.h"
#include "OV7670.h"
#include "img_proc.h"
#include "face_detect.h"
#include "face_preprocess.h"
#include "lbph.h"
#include "facedb.h"
#include "face_app.h"
#include <stdio.h>

#define FRAME_TIMEOUT_MS   400U    /* restart capture if no frame in this time */

#define LCD_BAND_DISPLAY_HEIGHT 16U
static uint16_t s_display_band[OV7670_FRAME_WIDTH * 2U * LCD_BAND_DISPLAY_HEIGHT];
static uint8_t  s_gray[OV7670_FRAME_WIDTH * OV7670_FRAME_HEIGHT] __attribute__((section("ccmram")));
static FaceDetect_Config    s_detect_cfg;
static FaceDetect_Result    s_det;
static FacePreprocess_Result s_pre;
static uint16_t s_hist[LBPH_HIST_SIZE];

static uint32_t s_last_start_ms = 0U;
static uint32_t s_last_start_cyc = 0U;

static void PerfInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ------------------------------------------------------------------ */
/* IWDG watchdog: auto-reset if the main loop hangs (~8 s @32 kHz LSI) */
static void IWDG_Init(void)
{
    __HAL_RCC_LSI_ENABLE();
    while ((RCC->CSR & RCC_CSR_LSIRDY) == 0U)
    {
    }
    IWDG->KR = 0x5555U;      /* unlock */
    IWDG->PR = 4U;           /* prescaler /64 */
    IWDG->RLR = 4095U;       /* ~8.2 s typical */
    IWDG->KR = 0xCCCCU;      /* start watchdog */
}

static void IWDG_Refresh(void)
{
    IWDG->KR = 0xAAAAU;
}

/* ------------------------------------------------------------------ */
static void Camera_Recover(const char *why)
{
    printf("[FACE] camera recover: %s\r\n", why);
    (void)OV7670_Stop();
    hdcmi.ErrorCode = HAL_DCMI_ERROR_NONE;
    delay_ms(5U);
    OV7670_ClearFrameReady();
    if (OV7670_Start() == HAL_OK)
    {
        s_last_start_ms = HAL_GetTick();
        s_last_start_cyc = DWT->CYCCNT;
    }
}

static void Camera_ShowFrame(void)
{
    uint16_t band;
    uint16_t src_y;

    if (g_display_scale == 0U)
    {
        return;
    }

    if (g_display_scale == 1U)
    {
        /* Native 160x120 display: one quarter of the 2x pixel traffic. */
        for (band = 0U; band < OV7670_FRAME_HEIGHT / 8U; band++)
        {
            for (src_y = band * 8U; src_y < (band + 1U) * 8U; src_y++)
            {
                uint16_t x;
                uint16_t row = (uint16_t)(src_y - band * 8U);

                for (x = 0U; x < OV7670_FRAME_WIDTH; x++)
                {
                    s_display_band[(uint32_t)row * OV7670_FRAME_WIDTH + x] =
                        OV7670_GetPixel(x, src_y);
                }
            }

            LCD_ShowImage(0U, (uint16_t)(band * 8U),
                          OV7670_FRAME_WIDTH, 8U, s_display_band);
        }
        return;
    }

    /* 2x scaled display to 320x240. */
    for (band = 0U; band < OV7670_FRAME_HEIGHT / 8U; band++)
    {
        uint16_t band_h = 0U;

        for (src_y = band * 8U; src_y < (band + 1U) * 8U; src_y++)
        {
            uint16_t x;
            uint16_t row0 = (uint16_t)((src_y - band * 8U) * 2U);
            uint16_t row1 = (uint16_t)(row0 + 1U);

            for (x = 0U; x < OV7670_FRAME_WIDTH; x++)
            {
                uint16_t pixel = OV7670_GetPixel(x, src_y);
                s_display_band[(uint32_t)row0 * OV7670_FRAME_WIDTH * 2U + 2U * x] = pixel;
                s_display_band[(uint32_t)row0 * OV7670_FRAME_WIDTH * 2U + 2U * x + 1U] = pixel;
                s_display_band[(uint32_t)row1 * OV7670_FRAME_WIDTH * 2U + 2U * x] = pixel;
                s_display_band[(uint32_t)row1 * OV7670_FRAME_WIDTH * 2U + 2U * x + 1U] = pixel;
            }

            band_h += 2U;
        }

        LCD_ShowImage(0U, (uint16_t)(band * LCD_BAND_DISPLAY_HEIGHT),
                      OV7670_FRAME_WIDTH * 2U, band_h, s_display_band);
    }
}

static void DrawStatusBar(void)
{
    char buf[48];

    snprintf(buf, sizeof(buf), "%s FPS:%lu DB:%u",
             FaceApp_ModeText(), (unsigned long)g_face.fps,
             (unsigned)g_face.db_count);
    LCD_ShowString(0U, 0U, buf, WHITE, BLACK, Font_7x10);
}

static void DrawOverlay(void)
{
    uint8_t sc = (g_display_scale == 1U) ? 1U : 2U;

    if (g_display_scale == 0U)
    {
        return;
    }

    DrawStatusBar();

    if (s_det.found)
    {
        uint16_t bx = (uint16_t)(s_det.x * sc);
        uint16_t by = (uint16_t)(s_det.y * sc);
        uint16_t bw = (uint16_t)(s_det.w * sc);
        uint16_t bh = (uint16_t)(s_det.h * sc);
        uint16_t ty;
        char buf[40];

        if (bx + bw > 320U) bw = (uint16_t)(320U - bx);
        if (by + bh > 240U) bh = (uint16_t)(240U - by);
        if (bw < 2U || bh < 2U) bw = 2U;
        if (bh < 2U) bh = 2U;

        LCD_DrawRectangle(bx, by, bw, bh, 0U, GREEN);

        ty = (by >= 12U) ? (uint16_t)(by - 12U) : 0U;

        if (g_face.enrolling)
        {
            snprintf(buf, sizeof(buf), "ADD%u %s %u/%u",
                     (unsigned)g_face.enroll_id, g_face.enroll_name,
                     (unsigned)g_face.enroll_count,
                     (unsigned)FACE_APP_ENROLL_SAMPLES);
            LCD_ShowString(bx, ty, buf, YELLOW, BLACK, Font_7x10);
        }
        else if (!s_pre.ok)
        {
            if (s_pre.reason == FACE_PP_ERR_FAR)
            {
                LCD_ShowString(bx, ty, "FAR", CYAN, BLACK, Font_7x10);
            }
            else if (s_pre.reason == FACE_PP_ERR_BLUR)
            {
                LCD_ShowString(bx, ty, "BLUR", CYAN, BLACK, Font_7x10);
            }
            else if (s_pre.reason == FACE_PP_ERR_LIGHT)
            {
                LCD_ShowString(bx, ty, "LIGHT", CYAN, BLACK, Font_7x10);
            }
            else
            {
                LCD_ShowString(bx, ty, "WAIT", CYAN, BLACK, Font_7x10);
            }
        }
        else if (g_face.last_unknown)
        {
            LCD_ShowString(bx, ty, "UNKNOWN", YELLOW, BLACK, Font_7x10);
        }
        else if (g_face.last_id != 0U)
        {
            snprintf(buf, sizeof(buf), "%s mad:%lu",
                     g_face.last_name, (unsigned long)g_face.last_mad);
            LCD_ShowString(bx, ty, buf, GREEN, BLACK, Font_7x10);
        }
    }
    else
    {
        LCD_ShowString(0U, 14U, "NO FACE", RED, BLACK, Font_7x10);
    }
}

static void ShowFatal(void)
{
    LCD_Clear(RED);
    while (1)
    {
        IWDG_Refresh();
    }
}

int main(void)
{
    HAL_Init();

    if (sys_stm32_clock_init(336, 8, 2, 7) != 0U)
    {
        SystemCoreClockUpdate();
        delay_init((uint16_t)(SystemCoreClock / 1000000U));
        LCD_Init();
        LCD_Clear(RED);
        while (1)
        {
        }
    }

    delay_init(168);
    LCD_Init();
    usart_init(115200U);
    IWDG_Init();

    FaceApp_Init();

    if (OV7670_Init() != OV7670_STATUS_OK)
    {
        ShowFatal();
    }

    FaceDetect_DefaultConfig(&s_detect_cfg);
    PerfInit();

    if (OV7670_Start() != HAL_OK)
    {
        ShowFatal();
    }
    s_last_start_ms = HAL_GetTick();
    s_last_start_cyc = DWT->CYCCNT;

    printf("[FACE] pipeline started\r\n");

    while (1)
    {
        IWDG_Refresh();

        if (OV7670_IsFrameReady() != 0U)
        {
            OV7670_ToGray(s_gray);

            FaceDetect_Run(OV7670_GetFrameBuffer(), s_gray, &s_detect_cfg, &s_det);

            if (s_det.found)
            {
                FacePreprocess_Run(s_gray, &s_det, &s_pre);
                if (s_pre.ok)
                {
                    LBPH_Histogram(s_pre.face, s_hist);
                    FaceApp_OnFace(s_pre.face, s_hist, s_det.w, 1U, FACE_PP_OK);
                }
                else
                {
                    FaceApp_OnFace(NULL, NULL, s_det.w, 0U, s_pre.reason);
                }
            }

            Camera_ShowFrame();
            DrawOverlay();
            FaceApp_Tick();

            OV7670_ClearFrameReady();
            if (OV7670_Start() == HAL_OK)
            {
                s_last_start_ms = HAL_GetTick();
                s_last_start_cyc = DWT->CYCCNT;
            }
            else
            {
                Camera_Recover("start_fail");
            }
        }
        else
        {
            /* No frame: restart capture after timeout. DWT bypasses SysTick. */
            if (((HAL_GetTick() - s_last_start_ms) > FRAME_TIMEOUT_MS) ||
                ((DWT->CYCCNT - s_last_start_cyc) >
                 (FRAME_TIMEOUT_MS * (SystemCoreClock / 1000U))))
            {
                Camera_Recover("frame_timeout");
            }
        }

        FaceApp_ProcessCommand();

        /* Only a REAL overrun (DMA still has data) is an error. */
        if ((hdcmi.ErrorCode != HAL_DCMI_ERROR_NONE) &&
            (DMA2_Stream1->NDTR != 0U))
        {
            Camera_Recover("dcmi_error");
        }
    }
}
