/******************************************************************************
* File Name:   main.c
*
* Description: CM33 Non-Secure application entry point.
*              Initialises the debug UART via retarget-io and starts
*              the IPC message loop that sends to CM33-S every second.
*
* Related Document: See README.md
*
*******************************************************************************
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cybsp.h"
#include "cycfg.h"
#include <stdio.h>
#include "cy_syspm_pdcm.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
*******************************************************************************/

/*******************************************************************************
* Global Variables
*******************************************************************************/

/* For the Retarget -IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    DEBUG_UART_context;           /** UART context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj;           /** Debug UART HAL object  */
cy_stc_scb_uart_context_t UART_context;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void test_multi_core_ipc(void);

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* Non-secure entry point launched by the secure application.
* Initialises BSP, debug UART (retarget-io), prints a banner,
* then enters the IPC test loop (test_multi_core_ipc).
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/

int main(void)
{
     cy_rslt_t result = CY_RSLT_SUCCESS;
     /* Initialize the device and board peripherals */
     result = cybsp_init();

     /* Enable global interrupts */
     __enable_irq();

     /* Initialize retarget-io to use the debug UART port */
     result = (cy_rslt_t)Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);

     /* UART init failed. Stop program execution */
     if (result != CY_RSLT_SUCCESS)
     {
          CY_ASSERT(0);
     }

     Cy_SCB_UART_Enable(DEBUG_UART_HW);

     /* Setup the HAL UART */
     result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config, &DEBUG_UART_context, NULL);

     /* HAL UART init failed. Stop program execution */
     if (result != CY_RSLT_SUCCESS)
     {
          CY_ASSERT(0);
     }

     result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

     /* HAL retarget_io init failed. Stop program execution */
     if (result != CY_RSLT_SUCCESS)
     {
          CY_ASSERT(0);
     }

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: Single Core IPC\r\n");
    printf("************************************************************\r\n\n");

     /* Start IPC send loop — sends a message to CM33-S every 1 s */
     test_multi_core_ipc();

}
/* [] END OF FILE */
