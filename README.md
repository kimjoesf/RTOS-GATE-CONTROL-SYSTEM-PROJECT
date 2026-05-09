# Smart Parking Garage Gate System - Complete Project Manual

## Table of Contents
1. [Project Overview](#1-project-overview)
2. [Hardware Platform](#2-hardware-platform)
3. [System Architecture](#3-system-architecture)
4. [Pin Assignments & Wiring](#4-pin-assignments--wiring)
5. [Operating Modes](#5-operating-modes)
6. [Gate State Machine](#6-gate-state-machine)
7. [FreeRTOS Tasks Explained](#7-freertos-tasks-explained)
8. [Inter-Task Communication](#8-inter-task-communication)
9. [Safety System](#9-safety-system)
10. [LED Indicators](#10-led-indicators)
11. [Software Setup & Build](#11-software-setup--build)
12. [Simulation Guide (Keil uVision 5)](#12-simulation-guide-keil-uvision-5)
13. [Testing Procedures](#13-testing-procedures)
14. [Code Structure Reference](#14-code-structure-reference)
15. [Troubleshooting](#15-troubleshooting)

---

## 1. Project Overview

### What Is This Project?
This is an embedded systems project that simulates a **smart parking garage gate** controlled by a real-time operating system (FreeRTOS). The system manages the opening and closing of a parking garage barrier gate, similar to what you see at shopping mall parking entrances.

### What Does It Do?
- Controls a gate that can open, close, stop, and reverse
- Accepts commands from two sources: a **driver** (car driver panel) and a **security** (guard panel)
- **Security commands have priority** over driver commands
- Detects obstacles during gate closing and performs emergency reversal
- Provides visual feedback through LEDs (green = opening, red = closing)
- Reports system state through a debug terminal

### Why FreeRTOS?
FreeRTOS is a Real-Time Operating System that allows multiple tasks to run "simultaneously" on a single microcontroller. This project uses 5 concurrent tasks to handle inputs, safety, gate control, LED output, and status reporting — all running independently with proper synchronization.

---

## 2. Hardware Platform

### Microcontroller: TM4C123GH6PM (Tiva-C LaunchPad)
- **Manufacturer**: Texas Instruments
- **Architecture**: ARM Cortex-M4F (32-bit)
- **Clock Speed**: 16 MHz (internal oscillator, simulation mode)
- **GPIO Ports Used**: B, D, E, F
- **Development Environment**: Keil uVision 5 (Simulation Mode)

### Why Simulation Mode?
The project is designed to run entirely in the Keil uVision 5 simulator, meaning you do NOT need physical hardware. The simulator provides:
- Virtual GPIO pins you can toggle
- LED visualization through the TExaS LaunchPadDLL panel
- Switch inputs through the TExaS ADC panel
- Debug terminal output (printf via ITM)

---

## 3. System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    HARDWARE LAYER                            │
│  Buttons: PE0, PE1, PB0, PB1, PD0, PD1, PF4               │
│  LEDs:    PF1 (Red), PF3 (Green)                           │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                    FreeRTOS KERNEL                           │
│  Scheduler | SysTick | Memory Management (Heap_4)           │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                   APPLICATION TASKS                          │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌────────────┐               │
│  │  INPUT   │  │  SAFETY  │  │    GATE    │               │
│  │  TASK    │  │  TASK    │  │  CONTROL   │               │
│  │ (Prio 3) │  │ (Prio 4) │  │  (Prio 2)  │               │
│  └────┬─────┘  └────┬─────┘  └─────┬──────┘               │
│       │              │              │                       │
│       │    Queue     │  Semaphores  │                       │
│       ├──────────────┼──────────────┤                       │
│       │              │              │                       │
│  ┌────▼─────┐  ┌────▼─────┐                               │
│  │   LED    │  │  STATUS  │                               │
│  │  CONTROL │  │   TASK   │                               │
│  │ (Prio 2) │  │ (Prio 1) │                               │
│  └──────────┘  └──────────┘                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Pin Assignments & Wiring

### Input Buttons

| Pin  | Function         | Type        | Pull Resistor | Active State |
|------|-----------------|-------------|---------------|--------------|
| PE0  | Driver OPEN     | Push button | Pull-DOWN     | HIGH (1)     |
| PE1  | Driver CLOSE    | Push button | Pull-DOWN     | HIGH (1)     |
| PB0  | Security OPEN   | Push button | Pull-DOWN     | HIGH (1)     |
| PB1  | Security CLOSE  | Push button | Pull-DOWN     | HIGH (1)     |
| PD0  | Open Limit      | Limit switch| Pull-DOWN     | HIGH (1)     |
| PD1  | Closed Limit    | Limit switch| Pull-DOWN     | HIGH (1)     |
| PF4  | Obstacle (SW1)  | Push button | Pull-UP       | LOW (0)      |
| PF0  | Manual Mode (SW2)| Push button | Pull-UP       | LOW (0)      |

### Output LEDs

| Pin  | Color  | Meaning                          |
|------|--------|----------------------------------|
| PF1  | Red    | Gate is CLOSING                  |
| PF3  | Green  | Gate is OPENING or REVERSING     |

### Understanding Active-High vs Active-Low
- **Active-HIGH** (PE0, PE1, PB0, PB1, PD0, PD1): Button pressed = pin reads 1. Button released = pin reads 0.
- **Active-LOW** (PF4, PF0): Button pressed = pin reads 0. Button released = pin reads 1. (These are the on-board SW1 and SW2 on the LaunchPad, wired with internal pull-ups.)

### What Are Limit Switches?
In a real garage gate, physical limit switches are mounted at the fully-open and fully-closed positions. When the gate reaches either end, it physically presses the limit switch:
- **PD0 (Open Limit)**: Signals that the gate has reached the fully OPEN position
- **PD1 (Closed Limit)**: Signals that the gate has reached the fully CLOSED position

---

## 5. Operating Modes

The system supports **two operating modes** determined by PF0 (SW2):

### Auto Mode (PF0/SW2 NOT pressed - default)
This is the one-touch toggle mode:
1. **Press OPEN once** → Gate starts opening and continues until it reaches the open limit
2. **Press OPEN again** (while opening) → Gate STOPS midway
3. **Press CLOSE once** → Gate starts closing and continues until it reaches the closed limit
4. **Press CLOSE again** (while closing) → Gate STOPS midway

The gate runs automatically to the limit switch without needing to hold the button.

### Manual Mode (PF0/SW2 IS pressed - hold mode)
This simulates "holding" a button:
1. **Press SW2 first** (PF0 goes LOW = manual mode active)
2. **Press OPEN or CLOSE** → Gate starts moving
3. **Press the same button again** → NOTHING happens (gate keeps moving)
4. **Release SW2** (PF0 goes HIGH = manual mode released) → Gate STOPS immediately

In manual mode, the gate ONLY stops when:
- SW2 (PF0) is released
- A limit switch is reached
- An obstacle is detected (during closing)

### How PF0 Determines the Mode
| PF0 (SW2) State | Mode   | Second press of same button | Gate stops when...          |
|-----------------|--------|-----------------------------|-----------------------------||
| Released (HIGH) | Auto   | STOPS the gate              | Same button pressed again   |
| Pressed (LOW)   | Manual | IGNORED (gate keeps moving) | SW2 is released             |

### Direction Change (Both Modes)
- If the gate is **OPENING** and you press **CLOSE** → Gate immediately changes direction to CLOSING
- If the gate is **CLOSING** and you press **OPEN** → Gate immediately changes direction to OPENING

Direction change works the same in both Auto and Manual modes.

### Security Priority
The system has two control panels:
- **Driver Panel** (PE0, PE1): Used by the car driver at the entrance
- **Security Panel** (PB0, PB1): Used by the security guard

**Security commands ALWAYS override driver commands.** If both panels give conflicting commands, security wins. This applies in both Auto and Manual modes.

---

## 6. Gate State Machine

The gate operates as a Finite State Machine (FSM) with 6 states:

```
                    ┌───────────────┐
                    │  IDLE_CLOSED  │ ← Initial State
                    │  (Gate down)  │
                    └───────┬───────┘
                            │ OPEN pressed
                            ▼
                    ┌───────────────┐
            ┌──────│   OPENING     │──────┐
            │      │ (Green LED ON)│      │
            │      └───────┬───────┘      │
            │              │              │
   OPEN pressed     Open Limit      CLOSE pressed
   (2nd time)        reached         (direction change)
            │              │              │
            ▼              ▼              ▼
   ┌────────────────┐ ┌──────────┐ ┌──────────────┐
   │ STOPPED_MIDWAY │ │IDLE_OPEN │ │   CLOSING    │
   │ (Both LEDs OFF)│ │(Gate up) │ │(Red LED ON)  │
   └────────────────┘ └────┬─────┘ └──┬───────┬───┘
            ▲              │           │       │
            │         CLOSE pressed    │   Obstacle
            │              │           │   detected
            │              ▼           │       │
            │        ┌──────────────┐  │       ▼
            └────────│   CLOSING    │──┘  ┌──────────┐
            (CLOSE   │(Red LED ON)  │     │REVERSING │
            pressed  └──────┬───────┘     │(Green ON)│
            2nd time)       │             └────┬─────┘
                            │                  │
                       Closed Limit       After 500ms
                        reached                │
                            │                  ▼
                            ▼           ┌────────────────┐
                    ┌───────────────┐   │ STOPPED_MIDWAY │
                    │  IDLE_CLOSED  │   └────────────────┘
                    └───────────────┘
```

### State Descriptions

| State            | Meaning                                    | Green LED | Red LED |
|------------------|--------------------------------------------|-----------|---------|
| IDLE_CLOSED      | Gate fully closed, not moving               | OFF       | OFF     |
| IDLE_OPEN        | Gate fully open, not moving                 | OFF       | OFF     |
| OPENING          | Gate moving upward                          | ON        | OFF     |
| CLOSING          | Gate moving downward                        | OFF       | ON      |
| STOPPED_MIDWAY   | Gate stopped between open and closed        | OFF       | OFF     |
| REVERSING        | Emergency reversal (obstacle detected)      | ON        | OFF     |

---

## 7. FreeRTOS Tasks Explained

### Task 1: Safety Task (Priority 4 - HIGHEST)

**Purpose**: Monitors for obstacles and performs emergency gate reversal.

**How it works**:
1. Waits (blocked) until the obstacle semaphore is given
2. When triggered, checks if the gate is currently CLOSING
3. If closing: immediately sets state to REVERSING
4. Waits 500ms (simulates gate moving backward)
5. Sets state to STOPPED_MIDWAY

**Why highest priority?** Safety is critical — if an obstacle is detected (like a person or car under the gate), the system MUST respond immediately, preempting all other tasks.

### Task 2: Input Task (Priority 3 - HIGH)

**Purpose**: Reads all 7 buttons/switches every 30ms and detects state changes.

**How it works**:
1. Every 30ms, reads all GPIO pins
2. Compares current state with previous state
3. Detects **rising edges** (0→1 transitions = button press)
4. Sends button events to the queue (for OPEN/CLOSE buttons)
5. Gives semaphores (for limit switches and obstacle)

**Edge Detection**: The task only reacts to TRANSITIONS, not static states. This prevents duplicate events from a single button press.

### Task 3: Gate Control Task (Priority 2 - MEDIUM)

**Purpose**: Implements the state machine logic. Decides what the gate should do based on button events and limit signals.

**How it works**:
1. Checks limit semaphores (non-blocking) for end-of-travel
2. Reads button events from the queue (with 50ms timeout)
3. Applies toggle logic:
   - Same direction button pressed → STOP
   - Opposite direction button pressed → change direction
   - Button pressed from idle → start moving
4. Security commands are always honored

### Task 4: LED Control Task (Priority 2 - MEDIUM)

**Purpose**: Updates the physical LEDs based on the current gate state.

**How it works**:
1. Every 50ms, reads the current gate state
2. Sets LEDs according to the state table:
   - OPENING/REVERSING → Green ON, Red OFF
   - CLOSING → Green OFF, Red ON
   - All other states → Both OFF

### Task 5: Status Task (Priority 1 - LOWEST)

**Purpose**: Prints system state changes to the debug terminal (ITM).

**How it works**:
1. Every 500ms, checks if the state has changed
2. If changed, prints the new state and LED status
3. Only prints on changes (not continuously)

**Why lowest priority?** Printing is informational only — it should never interfere with actual gate control or safety operations.

---

## 8. Inter-Task Communication

### Queue: xButtonEventQueue
- **Type**: FreeRTOS Queue (FIFO buffer)
- **Size**: 16 events
- **Item Size**: sizeof(ButtonEvent_t) = 4 bytes
- **Producer**: Input Task (sends button press events)
- **Consumer**: Gate Control Task (reads and processes events)
- **Purpose**: Decouples button reading from gate logic

### Binary Semaphores

| Semaphore              | Given by    | Taken by          | Purpose                    |
|------------------------|-------------|-------------------|----------------------------|
| xOpenLimitSemaphore    | Input Task  | Gate Control Task | Signal: gate reached open  |
| xClosedLimitSemaphore  | Input Task  | Gate Control Task | Signal: gate reached close |
| xObstacleSemaphore     | Input Task  | Safety Task       | Signal: obstacle detected  |

**Why semaphores for limits/obstacle?** These are one-shot signals (events). A semaphore is lighter than a queue for simple "something happened" notifications.

### Mutex: xGateStateMutex
- **Type**: FreeRTOS Mutex (mutual exclusion)
- **Purpose**: Protects the shared `gateState` variable
- **Used by**: ALL tasks that read or write the gate state
- **Why needed?** Multiple tasks access `gateState`. Without a mutex, one task could read a partially-written value from another task (race condition).

---

## 9. Safety System

### Obstacle Detection Flow
```
1. PF4 (SW1) pressed → pin goes LOW (active-low)
2. Input Task detects falling edge → gives xObstacleSemaphore
3. Safety Task wakes up (highest priority, preempts everything)
4. Safety Task checks: Is gate CLOSING?
   - YES → Set state to REVERSING
          → Wait 500ms (gate reverses)
          → Set state to STOPPED_MIDWAY
   - NO  → Ignore (obstacle only matters while closing)
```

### Why Only During Closing?
A real garage gate only poses a crushing danger when it's moving downward (closing). If it's opening (moving up), an obstacle underneath is not in danger.

### What Happens After Reversal?
The gate stops in STOPPED_MIDWAY state. The operator must manually press OPEN or CLOSE again to resume operation. This is a safety design — automatic retry could endanger the obstacle again.

---

## 10. LED Indicators

| System State     | Green (PF3) | Red (PF1) | Visual Meaning              |
|------------------|-------------|-----------|------------------------------|
| IDLE_CLOSED      | OFF         | OFF       | Gate stationary (closed)     |
| IDLE_OPEN        | OFF         | OFF       | Gate stationary (open)       |
| OPENING          | ON          | OFF       | Gate moving up               |
| CLOSING          | OFF         | ON        | Gate moving down             |
| STOPPED_MIDWAY   | OFF         | OFF       | Gate stationary (middle)     |
| REVERSING        | ON          | OFF       | Emergency reversal (going up)|

---

## 11. Software Setup & Build

### Prerequisites
- **Keil uVision 5** (MDK-ARM)
- **ARM::CMSIS** pack (v6.x)
- **ARM::CMSIS-FreeRTOS** pack (v11.2.0)
- **ARM::CMSIS-Compiler** pack (v2.1.0)
- **Keil::TM4C_DFP** pack (v1.1.0)

### Project Configuration
| Setting              | Value                          |
|----------------------|--------------------------------|
| Target Device        | TM4C123GH6PM                  |
| Compiler             | ARM Compiler 6 (armclang)      |
| C Library            | MicroLIB (enabled)             |
| Simulation Mode      | Yes (uSim = 1)                 |
| FreeRTOS Heap        | 16384 bytes (Heap_4)           |
| Startup Stack        | 1024 bytes (0x400)             |
| Clock Setup          | Disabled (CLOCK_SETUP = 0)     |
| SystemCoreClock      | 16 MHz (set in main)           |
| Event Recorder       | Disabled                       |
| STDOUT               | ITM (printf via debug viewer)  |

### Key Configuration Files
| File                              | Purpose                          |
|-----------------------------------|----------------------------------|
| `main.c`                          | All application code             |
| `RTE/RTOS/FreeRTOSConfig.h`      | FreeRTOS kernel configuration    |
| `RTE/Device/.../system_TM4C123.c` | System initialization (clock)   |
| `RTE/Device/.../startup_TM4C123.s`| Startup code & vector table     |

### Build Steps
1. Open `embedded_project.uvprojx` in Keil uVision 5
2. Go to **Project → Rebuild all target files**
3. Verify **0 Errors** in Build Output
4. Warnings about unused variables are acceptable

---

## 12. Simulation Guide (Keil uVision 5)

### Starting the Simulation
1. Press **Ctrl+F5** (or Debug → Start/Stop Debug Session)
2. Press **F5** (Run) to start execution
3. Open the **Debug (printf) Viewer**: View → Serial Windows → Debug (printf) Viewer
4. The TExaS LaunchPadDLL and TExaS ADC panels should appear automatically

### Simulation Panels

#### TExaS LaunchPadDLL Panel (Port F Hardware)
- **SW1** = PF4 (Obstacle button) — click to toggle
- **SW2** = PF0 (Manual Mode modifier) — click to toggle
- **LED on PF1** = Red LED (visible in panel)
- **LED on PF3** = Green LED (visible in panel)

#### TExaS ADC Panel
- **Switch inputs PD1-0** = Limit switches (PD0 = Open Limit, PD1 = Closed Limit)
- Click the PD0/PD1 buttons at the bottom to toggle them

#### Buttons NOT on Panels (PE0, PE1, PB0, PB1)
For buttons not directly visible on the TExaS panels, use one of these methods:

**Method 1: Memory Window**
1. View → Memory Windows → Memory 1
2. Enter address `0x400243FC` (Port E DATA register)
3. Modify the byte value:
   - Set bit 0 = PE0 (Driver OPEN): write `0x01`
   - Set bit 1 = PE1 (Driver CLOSE): write `0x02`
   - Clear to release: write `0x00`

**Method 2: Peripherals Window**
1. Peripherals → System Control → GPIO Port E
2. Toggle the DATA bits directly

**Method 3: Command Window**
```
PORTE_DATA = 0x01    // Press Driver OPEN (PE0)
PORTE_DATA = 0x00    // Release
PORTB_DATA = 0x01    // Press Security OPEN (PB0)
PORTB_DATA = 0x00    // Release
```

### Reading Terminal Output
The printf output appears in **Debug (printf) Viewer** window. You'll see messages like:
```
=== Smart Parking Garage Gate System ===
Initial State: IDLE_CLOSED
Green LED: OFF | Red LED: OFF

[GATE] OPEN pressed -> OPENING

--- State Changed ---
State: OPENING
Green LED: ON  | Red LED: OFF
---------------------
```

---

## 13. Testing Procedures

### Test 1: Single Touch (One-Touch Auto Mode)
**Goal**: Verify gate moves to limit with a single button press.

| Step | Action                    | Expected Result                              |
|------|---------------------------|----------------------------------------------|
| 1    | Press PE0 (toggle HIGH)   | Gate starts OPENING, Green LED ON            |
| 2    | Release PE0 (toggle LOW)  | Gate CONTINUES opening (auto mode)           |
| 3    | Press PD0 (Open Limit)    | Gate stops → IDLE_OPEN, Both LEDs OFF        |
| 4    | Press PE1 (toggle HIGH)   | Gate starts CLOSING, Red LED ON              |
| 5    | Release PE1 (toggle LOW)  | Gate CONTINUES closing (auto mode)           |
| 6    | Press PD1 (Closed Limit)  | Gate stops → IDLE_CLOSED, Both LEDs OFF      |

**How to simulate in TExaS panel**:
- "Press" = Click once (pin goes HIGH)
- "Release" = Click again (pin goes LOW)
- For single-touch: click once, click again quickly (< 500ms between clicks)

### Test 2: Manual Mode (Hold Control using SW2/PF0)
**Goal**: Verify gate moves only while SW2 is held, and stops when SW2 is released.

| Step | Action                              | Expected Result                                   |
|------|-------------------------------------|---------------------------------------------------|
| 1    | Click SW2 (PF0 goes LOW)           | Manual mode ACTIVE (no visible change yet)        |
| 2    | Press PE0 (Driver OPEN)             | Gate starts OPENING, Green LED ON                 |
| 3    | Press PE0 again (while SW2 held)    | NOTHING happens (gate keeps opening - no toggle)  |
| 4    | Click SW2 again (PF0 goes HIGH)    | Gate STOPS → STOPPED_MIDWAY, LEDs OFF             |

**Key difference from Auto Mode**: In step 3, pressing the same button again does NOT stop the gate because SW2 (manual mode) is still active. The gate only stops when SW2 is released in step 4.

**How to simulate in TExaS LaunchPad panel**:
1. Click SW2 on the panel (activates manual mode)
2. Toggle the desired action button (PE0/PE1/PB0/PB1)
3. Try clicking the same action button again → gate should NOT stop
4. Click SW2 again to release manual mode → gate stops

### Test 3: Stop Midway (Toggle Stop)
**Goal**: Verify pressing the same button again stops the gate.

| Step | Action                    | Expected Result                              |
|------|---------------------------|----------------------------------------------|
| 1    | Press PE0                 | Gate starts OPENING                          |
| 2    | Press PE0 again           | Gate STOPS → STOPPED_MIDWAY, LEDs OFF        |
| 3    | Press PE1                 | Gate starts CLOSING from midway              |
| 4    | Press PE1 again           | Gate STOPS → STOPPED_MIDWAY                  |

### Test 4: Direction Change
**Goal**: Verify pressing opposite button changes gate direction.

| Step | Action                    | Expected Result                              |
|------|---------------------------|----------------------------------------------|
| 1    | Press PE0                 | Gate starts OPENING (Green ON)               |
| 2    | Press PE1                 | Gate changes to CLOSING (Red ON, Green OFF)  |
| 3    | Press PE0                 | Gate changes to OPENING (Green ON, Red OFF)  |

### Test 5: Obstacle Safety
**Goal**: Verify obstacle detection during closing triggers emergency reversal.

| Step | Action                    | Expected Result                              |
|------|---------------------------|----------------------------------------------|
| 1    | Start gate CLOSING        | Red LED ON                                   |
| 2    | Press SW1 (PF4 obstacle)  | Gate REVERSES (Green ON), prints warning     |
| 3    | Wait 500ms                | Gate → STOPPED_MIDWAY, Both LEDs OFF         |
| 4    | Press SW1 during OPENING  | NOTHING happens (obstacle ignored)           |

### Test 6: Security Priority
**Goal**: Verify security panel overrides driver panel.

| Step | Action                      | Expected Result                            |
|------|-----------------------------|--------------------------------------------|
| 1    | Press PE1 (Driver CLOSE)    | Gate starts CLOSING                        |
| 2    | Press PB0 (Security OPEN)   | Gate changes to OPENING (security wins)    |
| 3    | Press PE0 (Driver OPEN)     | Gate starts OPENING                        |
| 4    | Press PB1 (Security CLOSE)  | Gate changes to CLOSING (security wins)    |

### Test 7: Ignore Invalid Commands
**Goal**: Verify system ignores meaningless commands.

| Step | Action                           | Expected Result                       |
|------|----------------------------------|---------------------------------------|
| 1    | System in IDLE_OPEN              | Gate fully open                       |
| 2    | Press PE0 (OPEN)                 | IGNORED (already open), prints msg    |
| 3    | System in IDLE_CLOSED            | Gate fully closed                     |
| 4    | Press PE1 (CLOSE)                | IGNORED (already closed), prints msg  |

---

## 14. Code Structure Reference

### File: main.c (Single-File Architecture)

```
Lines 1-24      : File header & documentation
Lines 26-31     : #include directives (FreeRTOS headers)
Lines 33-135    : Hardware register definitions (#define with addresses)
Lines 137-160   : Pin and clock gate mask definitions
Lines 162-183   : Type definitions (GateState_t, ButtonEvent_t enums)
Lines 185-202   : Configuration constants (timing, priorities, stack)
Lines 204-223   : Global variables & RTOS handles
Lines 225-267   : GPIO_Init() - Hardware initialization
Lines 269-278   : Button reading helper functions (inline)
Lines 280-298   : LED control helper functions
Lines 300-318   : GetGateState() / SetGateState() - Mutex-protected access
Lines 320-334   : GetStateName() - State-to-string conversion
Lines 336-418   : vInputTask() - Button reading & edge detection
Lines 420-448   : vSafetyTask() - Obstacle detection & reversal
Lines 450-575   : vGateControlTask() - State machine logic
Lines 577-619   : vLedControlTask() - LED output control
Lines 621-665   : vStatusTask() - Terminal status reporting
Lines 667-676   : vApplicationStackOverflowHook() - Error handler
Lines 678-721   : main() - System initialization & RTOS startup
```

### Key Data Flow
```
Physical Buttons → Input Task → Queue/Semaphores → Gate Control Task → State Change
                                                                            │
                                                          ┌─────────────────┼────────────┐
                                                          ▼                 ▼            ▼
                                                    LED Control       Status Task    Safety Task
                                                    (updates LEDs)   (prints state) (if obstacle)
```

---

## 15. Troubleshooting

### Problem: Code doesn't reach main()
**Cause**: PLL initialization loops are very slow in the simulator.
**Fix**: Ensure `CLOCK_SETUP 0` in `system_TM4C123.c` (line 36).

### Problem: Need to press Run multiple times
**Cause**: Standard C library uses semihosting (SVC calls stop simulator).
**Fix**: Enable MicroLIB in project settings (Target → Use MicroLIB checkbox, or `<useUlib>1</useUlib>` in .uvprojx).

### Problem: No printf output visible
**Cause**: Debug (printf) Viewer not open.
**Fix**: In debug mode, go to View → Serial Windows → Debug (printf) Viewer.

### Problem: Event Recorder assert (BKPT)
**Cause**: Event Recorder initialization fails.
**Fix**: Set `configEVR_INITIALIZE 0` in FreeRTOSConfig.h.

### Problem: Buttons don't respond
**Cause**: GPIO clock not enabled or wrong pin polarity.
**Fix**: Verify RCGCGPIO enables ports B, D, E, F. Check active-high vs active-low logic.

### Problem: Gate doesn't stop at limits
**Cause**: Limit switch edge not detected (already HIGH when system starts).
**Fix**: Make sure limit switch starts LOW, then toggle HIGH when gate should stop.

### Problem: Build errors about redefined macros
**Cause**: Register macros conflict with CMSIS pack headers.
**Fix**: All register defines use `#ifndef` guards to prevent conflicts.

---

## Appendix A: Register Address Map

| Register              | Address      | Purpose                        |
|-----------------------|--------------|--------------------------------|
| SYSCTL_RCGCGPIO_R    | 0x400FE608   | GPIO clock enable              |
| SYSCTL_PRGPIO_R      | 0x400FEA08   | GPIO peripheral ready          |
| GPIO_PORTF_DATA_R    | 0x400253FC   | Port F data (all bits)         |
| GPIO_PORTE_DATA_R    | 0x400243FC   | Port E data (all bits)         |
| GPIO_PORTB_DATA_R    | 0x400053FC   | Port B data (all bits)         |
| GPIO_PORTD_DATA_R    | 0x400073FC   | Port D data (all bits)         |

## Appendix B: FreeRTOS Configuration Summary

| Parameter                     | Value   | Meaning                           |
|-------------------------------|---------|-----------------------------------|
| configTICK_RATE_HZ            | 1000    | 1ms tick resolution               |
| configTOTAL_HEAP_SIZE         | 16384   | 16KB heap for RTOS objects        |
| configMINIMAL_STACK_SIZE      | 128     | Minimum stack per task (words)    |
| configMAX_PRIORITIES          | 56      | Maximum priority levels           |
| configCHECK_FOR_STACK_OVERFLOW| 2       | Stack overflow detection enabled  |
| configUSE_MUTEXES             | 1       | Mutex support enabled             |
| configUSE_COUNTING_SEMAPHORES | 1       | Semaphore support enabled         |
| configUSE_TIMERS              | 1       | Software timer support            |

## Appendix C: Task Stack & Priority Summary

| Task Name      | Stack (words) | Priority | Blocked On                  |
|----------------|---------------|----------|-----------------------------|
| Safety         | 256           | 4        | xObstacleSemaphore          |
| Input          | 256           | 3        | vTaskDelay (30ms)           |
| GateCtrl       | 256           | 2        | xQueueReceive (50ms)        |
| LedCtrl        | 256           | 2        | vTaskDelay (50ms)           |
| Status         | 256           | 1        | vTaskDelay (500ms)          |

---

*End of Manual*
