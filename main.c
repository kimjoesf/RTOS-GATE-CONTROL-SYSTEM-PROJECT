/*****************************************************************************
 * Smart Parking Garage Gate System - FreeRTOS Project
 * Target: TM4C123GH6PM (Keil uVision 5 Simulation Mode)
 *
 * Button Assignments:
 *   PE0 - Driver OPEN      (pull-down, active-high)
 *   PE1 - Driver CLOSE     (pull-down, active-high)
 *   PB0 - Security OPEN    (pull-down, active-high)
 *   PB1 - Security CLOSE   (pull-down, active-high)
 *   PD0 - Open Limit       (pull-down, active-high)
 *   PD1 - Closed Limit     (pull-down, active-high)
 *   PF4 - Obstacle         (on-board SW1, pull-up, active-low)
 *
 * LED Assignments (on-board Port F):
 *   PF3 - Green LED (gate OPENING)
 *   PF1 - Red LED   (gate CLOSING)
 *
 * FreeRTOS Tasks:
 *   Safety Task      - Priority 4 (Highest) - Obstacle detection & reversal
 *   Input Task       - Priority 3 (High)    - Button reading & debouncing
 *   Gate Control Task- Priority 2 (Medium)  - State machine logic
 *   LED Control Task - Priority 2 (Medium)  - LED output control
 *   Status Task      - Priority 1 (Low)     - Terminal state reporting
 *****************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/*============================================================================
 * HARDWARE REGISTER DEFINITIONS (TM4C123GH6PM)
 * Using #ifndef guards to prevent conflicts with CMSIS pack headers
 *============================================================================*/
/* System Control */
#ifndef SYSCTL_RCGCGPIO_R
#define SYSCTL_RCGCGPIO_R   (*((volatile uint32_t *)0x400FE608))
#endif
#ifndef SYSCTL_PRGPIO_R
#define SYSCTL_PRGPIO_R     (*((volatile uint32_t *)0x400FEA08))
#endif

/* GPIO Port F (LEDs + PF4 Obstacle) */
#ifndef GPIO_PORTF_DATA_R
#define GPIO_PORTF_DATA_R   (*((volatile uint32_t *)0x400253FC))
#endif
#ifndef GPIO_PORTF_DIR_R
#define GPIO_PORTF_DIR_R    (*((volatile uint32_t *)0x40025400))
#endif
#ifndef GPIO_PORTF_AFSEL_R
#define GPIO_PORTF_AFSEL_R  (*((volatile uint32_t *)0x40025420))
#endif
#ifndef GPIO_PORTF_PUR_R
#define GPIO_PORTF_PUR_R    (*((volatile uint32_t *)0x40025510))
#endif
#ifndef GPIO_PORTF_DEN_R
#define GPIO_PORTF_DEN_R    (*((volatile uint32_t *)0x4002551C))
#endif
#ifndef GPIO_PORTF_AMSEL_R
#define GPIO_PORTF_AMSEL_R  (*((volatile uint32_t *)0x40025528))
#endif
#ifndef GPIO_PORTF_PCTL_R
#define GPIO_PORTF_PCTL_R   (*((volatile uint32_t *)0x4002552C))
#endif
#ifndef GPIO_PORTF_LOCK_R
#define GPIO_PORTF_LOCK_R   (*((volatile uint32_t *)0x40025520))
#endif
#ifndef GPIO_PORTF_CR_R
#define GPIO_PORTF_CR_R     (*((volatile uint32_t *)0x40025524))
#endif

/* GPIO Port E (PE0 Driver OPEN, PE1 Driver CLOSE) */
#ifndef GPIO_PORTE_DATA_R
#define GPIO_PORTE_DATA_R   (*((volatile uint32_t *)0x400243FC))
#endif
#ifndef GPIO_PORTE_DIR_R
#define GPIO_PORTE_DIR_R    (*((volatile uint32_t *)0x40024400))
#endif
#ifndef GPIO_PORTE_AFSEL_R
#define GPIO_PORTE_AFSEL_R  (*((volatile uint32_t *)0x40024420))
#endif
#ifndef GPIO_PORTE_PDR_R
#define GPIO_PORTE_PDR_R    (*((volatile uint32_t *)0x40024514))
#endif
#ifndef GPIO_PORTE_DEN_R
#define GPIO_PORTE_DEN_R    (*((volatile uint32_t *)0x4002451C))
#endif
#ifndef GPIO_PORTE_AMSEL_R
#define GPIO_PORTE_AMSEL_R  (*((volatile uint32_t *)0x40024528))
#endif
#ifndef GPIO_PORTE_PCTL_R
#define GPIO_PORTE_PCTL_R   (*((volatile uint32_t *)0x4002452C))
#endif

/* GPIO Port B (PB0 Security OPEN, PB1 Security CLOSE) */
#ifndef GPIO_PORTB_DATA_R
#define GPIO_PORTB_DATA_R   (*((volatile uint32_t *)0x400053FC))
#endif
#ifndef GPIO_PORTB_DIR_R
#define GPIO_PORTB_DIR_R    (*((volatile uint32_t *)0x40005400))
#endif
#ifndef GPIO_PORTB_AFSEL_R
#define GPIO_PORTB_AFSEL_R  (*((volatile uint32_t *)0x40005420))
#endif
#ifndef GPIO_PORTB_PDR_R
#define GPIO_PORTB_PDR_R    (*((volatile uint32_t *)0x40005514))
#endif
#ifndef GPIO_PORTB_DEN_R
#define GPIO_PORTB_DEN_R    (*((volatile uint32_t *)0x4000551C))
#endif
#ifndef GPIO_PORTB_AMSEL_R
#define GPIO_PORTB_AMSEL_R  (*((volatile uint32_t *)0x40005528))
#endif
#ifndef GPIO_PORTB_PCTL_R
#define GPIO_PORTB_PCTL_R   (*((volatile uint32_t *)0x4000552C))
#endif

/* GPIO Port D (PD0 Open Limit, PD1 Closed Limit) */
#ifndef GPIO_PORTD_DATA_R
#define GPIO_PORTD_DATA_R   (*((volatile uint32_t *)0x400073FC))
#endif
#ifndef GPIO_PORTD_DIR_R
#define GPIO_PORTD_DIR_R    (*((volatile uint32_t *)0x40007400))
#endif
#ifndef GPIO_PORTD_AFSEL_R
#define GPIO_PORTD_AFSEL_R  (*((volatile uint32_t *)0x40007420))
#endif
#ifndef GPIO_PORTD_PDR_R
#define GPIO_PORTD_PDR_R    (*((volatile uint32_t *)0x40007514))
#endif
#ifndef GPIO_PORTD_DEN_R
#define GPIO_PORTD_DEN_R    (*((volatile uint32_t *)0x4000751C))
#endif
#ifndef GPIO_PORTD_AMSEL_R
#define GPIO_PORTD_AMSEL_R  (*((volatile uint32_t *)0x40007528))
#endif
#ifndef GPIO_PORTD_PCTL_R
#define GPIO_PORTD_PCTL_R   (*((volatile uint32_t *)0x4000752C))
#endif

/*============================================================================
 * PIN DEFINITIONS
 *============================================================================*/
/* LEDs on Port F */
#define LED_RED_PIN         (1U << 1)
#define LED_GREEN_PIN       (1U << 3)
#define LED_MASK            (LED_RED_PIN | LED_GREEN_PIN)

/* Buttons */
#define BTN_PE0_PIN         (1U << 0)   /* Driver OPEN */
#define BTN_PE1_PIN         (1U << 1)   /* Driver CLOSE */
#define BTN_PB0_PIN         (1U << 0)   /* Security OPEN */
#define BTN_PB1_PIN         (1U << 1)   /* Security CLOSE */
#define BTN_PD0_PIN         (1U << 0)   /* Open Limit */
#define BTN_PD1_PIN         (1U << 1)   /* Closed Limit */
#define BTN_PF4_PIN         (1U << 4)   /* Obstacle */
#define BTN_PF0_PIN         (1U << 0)   /* Manual Mode (SW2) */

/* Clock gate masks */
#define RCGCGPIO_PORTB   (1U << 1)
#define RCGCGPIO_PORTD   (1U << 3)
#define RCGCGPIO_PORTE   (1U << 4)
#define RCGCGPIO_PORTF   (1U << 5)
#define RCGCGPIO_ALL     (RCGCGPIO_PORTB | RCGCGPIO_PORTD | \
                          RCGCGPIO_PORTE | RCGCGPIO_PORTF)

/*============================================================================
 * GATE STATE DEFINITIONS
 *============================================================================*/
typedef enum {
    STATE_IDLE_CLOSED = 0,
    STATE_IDLE_OPEN,
    STATE_OPENING,
    STATE_CLOSING,
    STATE_STOPPED_MIDWAY,
    STATE_REVERSING
} GateState_t;

/*============================================================================
 * BUTTON EVENT DEFINITIONS
 *============================================================================*/
typedef enum {
    EVT_NONE = 0,
    EVT_DRIVER_OPEN,
    EVT_DRIVER_CLOSE,
    EVT_SECURITY_OPEN,
    EVT_SECURITY_CLOSE,
    EVT_MANUAL_RELEASE
} ButtonEvent_t;

/*============================================================================
 * CONFIGURATION
 *============================================================================*/
#define DEBOUNCE_MS             30
#define REVERSE_DURATION_MS     500
#define INPUT_POLL_MS           30
#define STATUS_PRINT_MS         500
#define EVENT_QUEUE_SIZE        16

/* Task stack sizes (in words) */
#define TASK_STACK_SIZE         256

/* Task priorities */
#define PRIORITY_SAFETY         4
#define PRIORITY_INPUT          3
#define PRIORITY_GATE_CTRL      2
#define PRIORITY_LED_CTRL       2
#define PRIORITY_STATUS         1

/*============================================================================
 * GLOBAL VARIABLES (Protected by Mutex)
 *============================================================================*/
static volatile GateState_t gateState = STATE_IDLE_CLOSED;
static volatile uint32_t securityActive = 0;  /* 1 if current movement was initiated by security */

/*============================================================================
 * RTOS HANDLES
 *============================================================================*/
static QueueHandle_t      xButtonEventQueue   = NULL;
static SemaphoreHandle_t  xOpenLimitSemaphore = NULL;
static SemaphoreHandle_t  xClosedLimitSemaphore = NULL;
static SemaphoreHandle_t  xObstacleSemaphore  = NULL;
static SemaphoreHandle_t  xGateStateMutex     = NULL;

/* Task handles */
static TaskHandle_t xInputTaskHandle    = NULL;
static TaskHandle_t xGateCtrlTaskHandle = NULL;
static TaskHandle_t xLedCtrlTaskHandle  = NULL;
static TaskHandle_t xSafetyTaskHandle   = NULL;
static TaskHandle_t xStatusTaskHandle   = NULL;

/*============================================================================
 * GPIO INITIALIZATION
 *============================================================================*/
static void GPIO_Init(void)
{
    /* Enable clocks for ports B, D, E, F and wait until ready */
    SYSCTL_RCGCGPIO_R |= RCGCGPIO_ALL;
    while ((SYSCTL_PRGPIO_R & RCGCGPIO_ALL) != RCGCGPIO_ALL) { }

    /* --- Port F: Unlock PF0 (NMI pin requires unlock sequence) --- */
    GPIO_PORTF_LOCK_R  = 0x4C4F434BU;     /* Unlock Port F */
    GPIO_PORTF_CR_R    |= BTN_PF0_PIN;    /* Allow changes to PF0 */

    /* --- Port F: LEDs (PF1, PF3 outputs), PF4 Obstacle, PF0 Manual Mode --- */
    GPIO_PORTF_AMSEL_R &= ~(BTN_PF0_PIN | BTN_PF4_PIN | LED_MASK);
    GPIO_PORTF_PCTL_R  &= ~0x000FFFF0U;
    GPIO_PORTF_PCTL_R  &= ~0x0000000FU;   /* Clear PCTL for PF0 too */
    GPIO_PORTF_AFSEL_R &= ~(BTN_PF0_PIN | BTN_PF4_PIN | LED_MASK);
    GPIO_PORTF_DIR_R   |=  LED_MASK;      /* PF1, PF3 outputs */
    GPIO_PORTF_DIR_R   &= ~(BTN_PF4_PIN | BTN_PF0_PIN); /* PF4, PF0 inputs */
    GPIO_PORTF_PUR_R   |=  (BTN_PF4_PIN | BTN_PF0_PIN); /* Pull-up on PF4, PF0 */
    GPIO_PORTF_DEN_R   |=  BTN_PF0_PIN | BTN_PF4_PIN | LED_MASK;
    GPIO_PORTF_DATA_R  &= ~LED_MASK;      /* LEDs off */

    GPIO_PORTF_LOCK_R  = 0;               /* Re-lock Port F */

    /* --- Port E: PE0 Driver OPEN, PE1 Driver CLOSE (pull-down, active-high) --- */
    GPIO_PORTE_AMSEL_R &= ~(BTN_PE0_PIN | BTN_PE1_PIN);
    GPIO_PORTE_PCTL_R  &= ~0x000000FFU;
    GPIO_PORTE_AFSEL_R &= ~(BTN_PE0_PIN | BTN_PE1_PIN);
    GPIO_PORTE_DIR_R   &= ~(BTN_PE0_PIN | BTN_PE1_PIN);
    GPIO_PORTE_PDR_R   |=  (BTN_PE0_PIN | BTN_PE1_PIN);
    GPIO_PORTE_DEN_R   |=  (BTN_PE0_PIN | BTN_PE1_PIN);

    /* --- Port B: PB0 Security OPEN, PB1 Security CLOSE (pull-down, active-high) --- */
    GPIO_PORTB_AMSEL_R &= ~(BTN_PB0_PIN | BTN_PB1_PIN);
    GPIO_PORTB_PCTL_R  &= ~0x000000FFU;
    GPIO_PORTB_AFSEL_R &= ~(BTN_PB0_PIN | BTN_PB1_PIN);
    GPIO_PORTB_DIR_R   &= ~(BTN_PB0_PIN | BTN_PB1_PIN);
    GPIO_PORTB_PDR_R   |=  (BTN_PB0_PIN | BTN_PB1_PIN);
    GPIO_PORTB_DEN_R   |=  (BTN_PB0_PIN | BTN_PB1_PIN);

    /* --- Port D: PD0 Open Limit, PD1 Closed Limit (pull-down, active-high) --- */
    GPIO_PORTD_AMSEL_R &= ~(BTN_PD0_PIN | BTN_PD1_PIN);
    GPIO_PORTD_PCTL_R  &= ~0x000000FFU;
    GPIO_PORTD_AFSEL_R &= ~(BTN_PD0_PIN | BTN_PD1_PIN);
    GPIO_PORTD_DIR_R   &= ~(BTN_PD0_PIN | BTN_PD1_PIN);
    GPIO_PORTD_PDR_R   |=  (BTN_PD0_PIN | BTN_PD1_PIN);
    GPIO_PORTD_DEN_R   |=  (BTN_PD0_PIN | BTN_PD1_PIN);
}

/*============================================================================
 * BUTTON READING HELPERS
 *============================================================================*/
static inline uint32_t Read_DriverOpen(void)    { return (GPIO_PORTE_DATA_R & BTN_PE0_PIN) != 0; }
static inline uint32_t Read_DriverClose(void)   { return (GPIO_PORTE_DATA_R & BTN_PE1_PIN) != 0; }
static inline uint32_t Read_SecurityOpen(void)  { return (GPIO_PORTB_DATA_R & BTN_PB0_PIN) != 0; }
static inline uint32_t Read_SecurityClose(void) { return (GPIO_PORTB_DATA_R & BTN_PB1_PIN) != 0; }
static inline uint32_t Read_OpenLimit(void)     { return (GPIO_PORTD_DATA_R & BTN_PD0_PIN) != 0; }
static inline uint32_t Read_ClosedLimit(void)   { return (GPIO_PORTD_DATA_R & BTN_PD1_PIN) != 0; }
static inline uint32_t Read_Obstacle(void)      { return (GPIO_PORTF_DATA_R & BTN_PF4_PIN) == 0; }
static inline uint32_t Read_ManualMode(void)    { return (GPIO_PORTF_DATA_R & BTN_PF0_PIN) == 0; }

/*============================================================================
 * LED CONTROL HELPERS
 *============================================================================*/
static void LED_SetGreen(uint32_t on)
{
    if (on) {
        GPIO_PORTF_DATA_R |= LED_GREEN_PIN;
    } else {
        GPIO_PORTF_DATA_R &= ~LED_GREEN_PIN;
    }
}

static void LED_SetRed(uint32_t on)
{
    if (on) {
        GPIO_PORTF_DATA_R |= LED_RED_PIN;
    } else {
        GPIO_PORTF_DATA_R &= ~LED_RED_PIN;
    }
}

/*============================================================================
 * SAFE STATE ACCESS (Mutex protected)
 *============================================================================*/
static GateState_t GetGateState(void)
{
    GateState_t s;
    xSemaphoreTake(xGateStateMutex, portMAX_DELAY);
    s = gateState;
    xSemaphoreGive(xGateStateMutex);
    return s;
}

static void SetGateState(GateState_t newState)
{
    xSemaphoreTake(xGateStateMutex, portMAX_DELAY);
    gateState = newState;
    xSemaphoreGive(xGateStateMutex);
}

/*============================================================================
 * STATE NAME HELPER (for printing)
 *============================================================================*/
static const char* GetStateName(GateState_t s)
{
    switch (s) {
        case STATE_IDLE_CLOSED:   return "IDLE_CLOSED";
        case STATE_IDLE_OPEN:     return "IDLE_OPEN";
        case STATE_OPENING:       return "OPENING";
        case STATE_CLOSING:       return "CLOSING";
        case STATE_STOPPED_MIDWAY:return "STOPPED_MIDWAY";
        case STATE_REVERSING:     return "REVERSING";
        default:                  return "UNKNOWN";
    }
}

/*============================================================================
 * TASK: INPUT TASK (Priority: High)
 * Reads all buttons and detects rising edges (toggle press).
 * Tracks PF0 (SW2) for manual mode: when released, sends MANUAL_RELEASE.
 *============================================================================*/
static void vInputTask(void *pvParameters)
{
    /* Previous button states for edge detection */
    uint32_t prevDriverOpen    = 0;
    uint32_t prevDriverClose   = 0;
    uint32_t prevSecurityOpen  = 0;
    uint32_t prevSecurityClose = 0;
    uint32_t prevOpenLimit     = 0;
    uint32_t prevClosedLimit   = 0;
    uint32_t prevObstacle      = 0;
    uint32_t prevManualMode    = 0;

    (void)pvParameters;

    for (;;) {
        /* Read current button states */
        uint32_t curDriverOpen    = Read_DriverOpen();
        uint32_t curDriverClose   = Read_DriverClose();
        uint32_t curSecurityOpen  = Read_SecurityOpen();
        uint32_t curSecurityClose = Read_SecurityClose();
        uint32_t curOpenLimit     = Read_OpenLimit();
        uint32_t curClosedLimit   = Read_ClosedLimit();
        uint32_t curObstacle      = Read_Obstacle();
        uint32_t curManualMode    = Read_ManualMode();

        ButtonEvent_t evt;

        /* Detect rising edge (0->1) for each button = one toggle press */

        /* --- Driver OPEN (PE0) --- */
        if (curDriverOpen && !prevDriverOpen) {
            evt = EVT_DRIVER_OPEN;
            xQueueSend(xButtonEventQueue, &evt, 0);
        }

        /* --- Driver CLOSE (PE1) --- */
        if (curDriverClose && !prevDriverClose) {
            evt = EVT_DRIVER_CLOSE;
            xQueueSend(xButtonEventQueue, &evt, 0);
        }

        /* --- Security OPEN (PB0) --- */
        if (curSecurityOpen && !prevSecurityOpen) {
            evt = EVT_SECURITY_OPEN;
            xQueueSend(xButtonEventQueue, &evt, 0);
        }

        /* --- Security CLOSE (PB1) --- */
        if (curSecurityClose && !prevSecurityClose) {
            evt = EVT_SECURITY_CLOSE;
            xQueueSend(xButtonEventQueue, &evt, 0);
        }

        /* --- Manual Mode (PF0/SW2) - detect RELEASE (was pressed, now released) --- */
        if (!curManualMode && prevManualMode) {
            evt = EVT_MANUAL_RELEASE;
            xQueueSend(xButtonEventQueue, &evt, 0);
        }

        /* --- Open Limit (PD0) - Signal via semaphore --- */
        if (curOpenLimit && !prevOpenLimit) {
            xSemaphoreGive(xOpenLimitSemaphore);
        }

        /* --- Closed Limit (PD1) - Signal via semaphore --- */
        if (curClosedLimit && !prevClosedLimit) {
            xSemaphoreGive(xClosedLimitSemaphore);
        }

        /* --- Obstacle (PF4) - Signal via semaphore --- */
        if (curObstacle && !prevObstacle) {
            xSemaphoreGive(xObstacleSemaphore);
        }

        /* Update previous states */
        prevDriverOpen    = curDriverOpen;
        prevDriverClose   = curDriverClose;
        prevSecurityOpen  = curSecurityOpen;
        prevSecurityClose = curSecurityClose;
        prevOpenLimit     = curOpenLimit;
        prevClosedLimit   = curClosedLimit;
        prevObstacle      = curObstacle;
        prevManualMode    = curManualMode;

        vTaskDelay(pdMS_TO_TICKS(INPUT_POLL_MS));
    }
}

/*============================================================================
 * TASK: SAFETY TASK (Priority: Highest)
 * Monitors obstacle signal; enforces safety behavior during closing
 *============================================================================*/
static void vSafetyTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        /* Block until obstacle semaphore is given */
        if (xSemaphoreTake(xObstacleSemaphore, portMAX_DELAY) == pdTRUE) {
            GateState_t currentState = GetGateState();

            /* Only respond to obstacle during CLOSING */
            if (currentState == STATE_CLOSING) {
                /* Stop immediately, then reverse */
                SetGateState(STATE_REVERSING);
                printf("!!!!!!!!!!!!!!! Obstacle detected !!!!!!!!!!!!!!!\n");

                /* Reverse for 500ms */
                vTaskDelay(pdMS_TO_TICKS(REVERSE_DURATION_MS));

                /* Stop completely - go to STOPPED_MIDWAY */
                SetGateState(STATE_STOPPED_MIDWAY);
                securityActive = 0;
                printf(" Reverse complete. Gate STOPPED_MIDWAY\n");
            }
        }
    }
}

/*============================================================================
 * TASK: GATE CONTROL TASK (Priority: Medium)
 * Dual Mode Operation:
 *   AUTO MODE (PF0/SW2 not pressed):
 *     - First press of OPEN/CLOSE  -> starts movement (runs to limit)
 *     - Second press of same button -> stops movement (STOPPED_MIDWAY)
 *     - Pressing opposite button   -> changes direction
 *   MANUAL MODE (PF0/SW2 pressed):
 *     - Press OPEN/CLOSE -> starts movement
 *     - Pressing same button again -> IGNORED (no toggle stop)
 *     - Gate stops ONLY when PF0 is released (EVT_MANUAL_RELEASE)
 *   Common:
 *     - Security buttons override driver buttons (priority)
 *     - Limit switch reached -> gate idle at limit
 *============================================================================*/
static void vGateControlTask(void *pvParameters)
{
    ButtonEvent_t evt;
    GateState_t   currentState;

    (void)pvParameters;

    for (;;) {
        /* Check for limit semaphores (non-blocking) */
        if (xSemaphoreTake(xOpenLimitSemaphore, 0) == pdTRUE) {
            currentState = GetGateState();
            if (currentState == STATE_OPENING || currentState == STATE_REVERSING) {
                SetGateState(STATE_IDLE_OPEN);
                securityActive = 0;
                printf("[GATE] Open limit reached -> IDLE_OPEN\n");
            }
        }

        if (xSemaphoreTake(xClosedLimitSemaphore, 0) == pdTRUE) {
            currentState = GetGateState();
            if (currentState == STATE_CLOSING) {
                SetGateState(STATE_IDLE_CLOSED);
                securityActive = 0;
                printf("[GATE] Closed limit reached -> IDLE_CLOSED\n");
            }
        }

        /* Process button events from queue (short timeout to keep checking limits) */
        if (xQueueReceive(xButtonEventQueue, &evt, pdMS_TO_TICKS(50)) == pdTRUE) {
            currentState = GetGateState();

            /* --- MANUAL RELEASE: PF0 released -> stop gate if moving --- */
            if (evt == EVT_MANUAL_RELEASE) {
                if (currentState == STATE_OPENING || currentState == STATE_CLOSING) {
                    SetGateState(STATE_STOPPED_MIDWAY);
                    securityActive = 0;
                    printf("[GATE] Manual mode released -> STOPPED_MIDWAY\n");
                }
                continue;
            }

            /* Security priority: ignore driver commands if security is actively
               commanding OR if current movement was security-initiated */
            if (evt == EVT_DRIVER_OPEN || evt == EVT_DRIVER_CLOSE) {
                if (Read_SecurityClose() || Read_SecurityOpen() || securityActive) {
                    printf("[GATE] Driver command ignored (Security has priority)\n");
                    continue;
                }
            }

            /* Determine if this is an OPEN or CLOSE command */
            uint32_t isOpenCmd  = (evt == EVT_DRIVER_OPEN  || evt == EVT_SECURITY_OPEN);
            uint32_t isCloseCmd = (evt == EVT_DRIVER_CLOSE || evt == EVT_SECURITY_CLOSE);

            /* Check if manual mode is currently active */
            uint32_t manualActive = Read_ManualMode();

            /* --- OPEN command --- */
            if (isOpenCmd) {
                switch (currentState) {
                    case STATE_OPENING:
                        if (!manualActive) {
                            /* AUTO MODE: second press = STOP */
                            SetGateState(STATE_STOPPED_MIDWAY);
                            securityActive = 0;
                            printf("[GATE][AUTO] OPEN pressed again -> STOPPED_MIDWAY\n");
                        }
                        /* MANUAL MODE: ignore (gate keeps moving) */
                        break;

                    case STATE_CLOSING:
                        /* Change direction to OPENING (both modes) */
                        SetGateState(STATE_OPENING);
                        if (evt == EVT_SECURITY_OPEN) securityActive = 1;
                        printf("[GATE] OPEN pressed -> direction change -> OPENING\n");
                        break;

                    case STATE_IDLE_CLOSED:
                    case STATE_STOPPED_MIDWAY:
                        /* Gate is stopped: start OPENING (both modes) */
                        SetGateState(STATE_OPENING);
                        securityActive = (evt == EVT_SECURITY_OPEN) ? 1 : 0;
                        if (manualActive) {
                            printf("[GATE][MANUAL] OPEN pressed -> OPENING (hold SW2)\n");
                        } else {
                            printf("[GATE][AUTO] OPEN pressed -> OPENING\n");
                        }
                        break;

                    case STATE_IDLE_OPEN:
                        /* Already fully open: ignore */
                        printf("[GATE] OPEN pressed but already IDLE_OPEN\n");
                        break;

                    case STATE_REVERSING:
                        /* During safety reversal: ignore user input */
                        break;

                    default:
                        break;
                }
            }

            /* --- CLOSE command --- */
            if (isCloseCmd) {
                switch (currentState) {
                    case STATE_CLOSING:
                        if (!manualActive) {
                            /* AUTO MODE: second press = STOP */
                            SetGateState(STATE_STOPPED_MIDWAY);
                            securityActive = 0;
                            printf("[GATE][AUTO] CLOSE pressed again -> STOPPED_MIDWAY\n");
                        }
                        /* MANUAL MODE: ignore (gate keeps moving) */
                        break;

                    case STATE_OPENING:
                        /* Change direction to CLOSING (both modes) */
                        SetGateState(STATE_CLOSING);
                        if (evt == EVT_SECURITY_CLOSE) securityActive = 1;
                        printf("[GATE] CLOSE pressed -> direction change -> CLOSING\n");
                        break;

                    case STATE_IDLE_OPEN:
                    case STATE_STOPPED_MIDWAY:
                        /* Gate is stopped: start CLOSING (both modes) */
                        SetGateState(STATE_CLOSING);
                        securityActive = (evt == EVT_SECURITY_CLOSE) ? 1 : 0;
                        if (manualActive) {
                            printf("[GATE][MANUAL] CLOSE pressed -> CLOSING (hold SW2)\n");
                        } else {
                            printf("[GATE][AUTO] CLOSE pressed -> CLOSING\n");
                        }
                        break;

                    case STATE_IDLE_CLOSED:
                        /* Already fully closed: ignore */
                        printf("[GATE] CLOSE pressed but already IDLE_CLOSED\n");
                        break;

                    case STATE_REVERSING:
                        /* During safety reversal: ignore user input */
                        break;

                    default:
                        break;
                }
            }
        }
    }
}

/*============================================================================
 * TASK: LED CONTROL TASK (Priority: Medium)
 * Controls Red/Green LEDs based on gate movement direction
 *============================================================================*/
static void vLedControlTask(void *pvParameters)
{
    GateState_t currentState;
    GateState_t prevState = STATE_IDLE_CLOSED;

    (void)pvParameters;

    for (;;) {
        currentState = GetGateState();

        switch (currentState) {
            case STATE_OPENING:
                LED_SetGreen(1);
                LED_SetRed(0);
                break;

            case STATE_CLOSING:
                LED_SetGreen(0);
                LED_SetRed(1);
                break;

            case STATE_REVERSING:
                LED_SetGreen(1);
                LED_SetRed(0);
                break;

            case STATE_IDLE_OPEN:
            case STATE_IDLE_CLOSED:
            case STATE_STOPPED_MIDWAY:
            default:
                LED_SetGreen(0);
                LED_SetRed(0);
                break;
        }

        prevState = currentState;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/*============================================================================
 * TASK: STATUS TASK (Priority: Low)
 * Displays current system state using printf (ITM debug output)
 *============================================================================*/
static void vStatusTask(void *pvParameters)
{
    GateState_t currentState;
    GateState_t prevState = STATE_IDLE_CLOSED;

    (void)pvParameters;

    printf("\n=== Smart Parking Garage Gate System ===\n");
    printf("Initial State: IDLE_CLOSED\n");
    printf("Green LED: OFF | Red LED: OFF\n\n");

    for (;;) {
        currentState = GetGateState();

        /* Only print when state changes */
        if (currentState != prevState) {
            printf("\n--- State Changed ---\n");
            printf("State: %s\n", GetStateName(currentState));

            switch (currentState) {
                case STATE_OPENING:
                    printf("Green LED: ON  | Red LED: OFF\n");
                    break;
                case STATE_CLOSING:
                    printf("Green LED: OFF | Red LED: ON\n");
                    break;
                case STATE_REVERSING:
                    printf("Green LED: ON  | Red LED: OFF\n");
                    break;
                default:
                    printf("Green LED: OFF | Red LED: OFF\n");
                    break;
            }
            printf("---------------------\n");

            prevState = currentState;
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_PRINT_MS));
    }
}

/*============================================================================
 * FreeRTOS HOOK FUNCTIONS
 *============================================================================*/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    printf("[ERROR] Stack overflow in task: %s\n", pcTaskName);
    for (;;) { }
}

/*============================================================================
 * MAIN FUNCTION
 *============================================================================*/
int main(void)
{
    /* Set SystemCoreClock to 16 MHz (default IOSC with no PLL setup) */
    extern uint32_t SystemCoreClock;
    SystemCoreClock = 16000000U;

    /* Initialize GPIO hardware */
    GPIO_Init();

    /* Create mutex for gate state protection */
    xGateStateMutex = xSemaphoreCreateMutex();

    /* Create binary semaphores for limit buttons and obstacle */
    xOpenLimitSemaphore   = xSemaphoreCreateBinary();
    xClosedLimitSemaphore = xSemaphoreCreateBinary();
    xObstacleSemaphore    = xSemaphoreCreateBinary();

    /* Create event queue for button events */
    xButtonEventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(ButtonEvent_t));

    /* Verify all RTOS objects created successfully */
    if (xGateStateMutex == NULL || xOpenLimitSemaphore == NULL ||
        xClosedLimitSemaphore == NULL || xObstacleSemaphore == NULL ||
        xButtonEventQueue == NULL) {
        printf("[ERROR] Failed to create RTOS objects!\n");
        for (;;) { }
    }

    /* Create tasks */
    xTaskCreate(vSafetyTask,      "Safety",   TASK_STACK_SIZE, NULL, PRIORITY_SAFETY,    &xSafetyTaskHandle);
    xTaskCreate(vInputTask,       "Input",    TASK_STACK_SIZE, NULL, PRIORITY_INPUT,     &xInputTaskHandle);
    xTaskCreate(vGateControlTask, "GateCtrl", TASK_STACK_SIZE, NULL, PRIORITY_GATE_CTRL, &xGateCtrlTaskHandle);
    xTaskCreate(vLedControlTask,  "LedCtrl",  TASK_STACK_SIZE, NULL, PRIORITY_LED_CTRL,  &xLedCtrlTaskHandle);
    xTaskCreate(vStatusTask,      "Status",   TASK_STACK_SIZE, NULL, PRIORITY_STATUS,    &xStatusTaskHandle);

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) { }
}