/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "tnkernel/tn.h"
#include "mem_map.h"
#include "version.h"

#pragma location = ".version"
__root struct version_t const __version = {VER_MAJOR, VER_MINOR, VER_SVN_REV, VER_FIX_REV };

// Reserve space for the system stack (for system startup and interrupts)
#define SYS_STK_SZ  192
#pragma location = ".noinit"
#pragma data_alignment=8
static unsigned int __sys_stk[SYS_STK_SZ];

// A type that describes the entries of the vector table
typedef void (*int_handler_t)();

// The name "__vector_table" has special meaning for C-SPY:
// it is where the SP start value is found, and the NVIC vector
// table register (VTOR) is initialized to this address if != 0
#pragma location = ".intvec" //vtable
//#pragma location = ".vtable"
__root int_handler_t const __vector_table[] =
{
    // The initial stack pointer
    (int_handler_t)&__sys_stk[SYS_STK_SZ - 1],

    tn_reset_int_handler,       // The reset handler
    tn_nmi_int_handler,         // The NMI handler
    tn_fault_int_handler,       // The hard fault handler
    tn_default_int_handler,     // The MPU fault handler
    tn_default_int_handler,     // The bus fault handler
    tn_default_int_handler,     // The usage fault handler
    0,                          // Reserved
    0,                          // Reserved
    0,                          // Reserved
    0,                          // Reserved
    tn_default_int_handler,     // SVCall handler
    tn_default_int_handler,     // Debug monitor handler
    0,                          // Reserved
    tn_pend_sv_handler,         // The PendSV handler
    tn_sys_tick_int_handler,    // The SysTick handler
    hw_portA_int_handler,       // GPIO Port A
    tn_default_int_handler,     // GPIO Port B
    hw_portC_int_handler,       // GPIO Port C
    tn_default_int_handler,     // GPIO Port D
    tn_default_int_handler,     // GPIO Port E
    hw_uart0_int_handler,       // UART0 Rx and Tx
    hw_uart1_int_handler,       // UART1 Rx and Tx  number  - 22
    tn_default_int_handler,     // SSI0 Rx and Tx
    tn_default_int_handler,     // I2C0 Master and Slave
    tn_default_int_handler,     // PWM Fault
    tn_default_int_handler,     // PWM Generator 0
    tn_default_int_handler,     // PWM Generator 1
    tn_default_int_handler,     // PWM Generator 2
    tn_default_int_handler,     // Quadrature Encoder 0
    hw_adc_seq0_int_handler,     // ADC0 Sequence 0
    hw_adc_seq1_int_handler,    // ADC0 Sequence 1
    tn_default_int_handler,     // ADC0 Sequence 2
    tn_default_int_handler,     // ADC0 Sequence 3
    hw_watchdog_int_handler,    // Watchdog timer
    hw_timer0a_int_handler,     // Timer 0 subtimer A
    tn_default_int_handler,     // Timer 0 subtimer B
    hw_timer1a_int_handler,     // Timer 1 subtimer A
    tn_default_int_handler,     // Timer 1 subtimer B
    tn_default_int_handler,     // Timer 2 subtimer A
    tn_default_int_handler,     // Timer 2 subtimer B
    tn_default_int_handler,     // Analog Comparator 0
    tn_default_int_handler,     // Analog Comparator 1
    tn_default_int_handler,     // Analog Comparator 2
    tn_default_int_handler,     // System Control (PLL, OSC, BO)
    tn_default_int_handler,     // FLASH Control
    tn_default_int_handler,     // GPIO Port F
    hw_portG_int_handler,       // GPIO Port G
    hw_portH_int_handler,     // GPIO Port H
    hw_uart2_int_handler,     // UART2 Rx and Tx
    tn_default_int_handler,     // SSI1 Rx and Tx
    tn_default_int_handler,     // Timer 3 subtimer A
    tn_default_int_handler,     // Timer 3 subtimer B
    tn_default_int_handler,     // I2C1 Master and Slave
    tn_default_int_handler,     // Quadrature Encoder 1
    tn_default_int_handler,     // CAN0
    tn_default_int_handler,     // CAN1
    tn_default_int_handler,     // CAN2
    hw_ethernet_int_handler,    // Ethernet
    tn_default_int_handler,     // Hibernate
    hw_USB0DeviceIntHandler,     // USB0 hw_USB0DeviceIntHandler,
    tn_default_int_handler,     // PWM Generator 3
    tn_default_int_handler,     // uDMA Software Transfer
    tn_default_int_handler,     // uDMA Error
    tn_default_int_handler,     // ADC1 Sequence 0
    tn_default_int_handler,     // ADC1 Sequence 1
    tn_default_int_handler,     // ADC1 Sequence 2
    tn_default_int_handler,     // ADC1 Sequence 3
    tn_default_int_handler,     // I2S0
    tn_default_int_handler,     // External Bus Interface 0
    hw_portJ_int_handler,     // GPIO Port J
    0                           // Reserved   number  - 71
};
