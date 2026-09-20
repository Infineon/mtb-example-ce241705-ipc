/******************************************************************************
* File Name:   ns_ipc.c
*
* Description: IPC configuration and ISR for the CM33 Non-Secure core.
*              Sends IPC notify messages to CM33-S on channel 0 and
*              handles both notify and release interrupt events on
*              IPC Interrupt 1 (CM33_NS_IPC_INTR_NUM).
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

#include "cy_pdl.h"
#include <stdio.h>

/*******************************************************************************
* Macros
*******************************************************************************/
/* CM33-NS sends on CH0 (owned by CM33-S), receives release events on Intr 1 */

/* IPC channel 1 — local NS channel (used for interrupt source only) */
#define CM33_NS_IPC_CH_NUM                  (CY_IPC_CHAN_USER)          /* IPC channel index 1 */

/* IPC Interrupt 1 — fired by CM33-S on CH0 release */
#define CM33_NS_IPC_INTR_NUM                (CY_IPC_INTR_USER)          /* IPC interrupt index 1 */
#define CM33_NS_IPC_INTR_MASK               (CY_IPC_INTR_MASK(CM33_NS_IPC_INTR_NUM))  /* BIT(1) */

/* MUX line that connects IPC Interrupt 1 to the CM33-NS NVIC */
#define CM33_NS_IPC_INTR_MUX                (CY_IPC_INTR_MUX(CM33_NS_IPC_INTR_NUM))

/* NVIC priority for the CM33-NS IPC interrupt handler */
#define IPC_PRIORITY                        (1u)

/*******************************************************************************
* Global Variables
*******************************************************************************/

/* CM33-NS IPC interrupt config: IPC Intr 1 MUX → NVIC */
const cy_stc_sysint_t ipcTestIntConfig =
{
          .intrSrc = (IRQn_Type)CM33_NS_IPC_INTR_MUX,
          .intrPriority = IPC_PRIORITY,
};

/* Event flags set inside the ISR and polled by the application loop */
volatile bool notify_event, rel_event;

/* Message payload sent to CM33-S via IPC channel 0 */
uint32_t writeMesg;

/*******************************************************************************
* Function Name: ipc_isr
********************************************************************************
* Summary:
* ISR for IPC Interrupt 1 on the non-secure core.  Handles two events:
*   - notify: CM33-S sent a message on channel 1.
*   - Release: CM33-S finished processing and freed channel 0.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/

void ipc_isr(void)
{
     uint32_t shadowIntr;
     IPC_STRUCT_Type *ipcPtr;
     IPC_INTR_STRUCT_Type *ipcIntrPtr;

     /* IPC Interrupt 1 base address */
     ipcIntrPtr = Cy_IPC_Drv_GetIntrBaseAddr(CM33_NS_IPC_INTR_NUM);

     /* Read masked interrupt status */
     shadowIntr = Cy_IPC_Drv_GetInterruptStatusMasked(ipcIntrPtr);

     /* Acquire (notify) path — CM33-S sent a message on CH1 */
     if (0UL != Cy_IPC_Drv_ExtractAcquireMask(shadowIntr))
     {
          printf("Recv. notify event \r\n");

          ipcPtr = Cy_IPC_Drv_GetIpcBaseAddress(CM33_NS_IPC_CH_NUM);

          /* Clear acquire bits to prevent re-trigger */
          Cy_IPC_Drv_ClearInterrupt(ipcIntrPtr, CY_IPC_NO_NOTIFICATION,
                    Cy_IPC_Drv_ExtractAcquireMask(shadowIntr));

          if (Cy_IPC_Drv_IsLockAcquired(ipcPtr))
          {
               /* Release CH1 and notify CM33-S */
               (void)Cy_IPC_Drv_LockRelease(ipcPtr, CM33_S_IPC_INTR_MASK);
          }
          notify_event = true;
     }

     /* Release path — CM33-S freed CH0, OK to send next message */
     if (0UL != Cy_IPC_Drv_ExtractReleaseMask(shadowIntr))
     {
          /* Clear the release interrupt bits. */
          Cy_IPC_Drv_ClearInterrupt(ipcIntrPtr,
                    Cy_IPC_Drv_ExtractReleaseMask(shadowIntr),
                    CY_IPC_NO_NOTIFICATION);

          printf("Received release event \r\n");

          rel_event = true;
     }

     /* Flush pending interrupt status */
     (void)Cy_IPC_Drv_GetInterruptStatus(ipcIntrPtr);
}

/*******************************************************************************
* Function Name: send_msg
********************************************************************************
* Summary:
* Acquires IPC channel 0, writes a message pointer, and fires a notify
* interrupt to CM33-S.  Prints success/failure to the debug UART.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/

void send_msg(void)
{
     cy_en_ipcdrv_status_t status;

     printf("IPC[%d] Write Data\r\n", CM33_S_IPC_CH_NUM);

     /* Acquire CH0, write msg ptr, notify CM33-S via IPC Interrupt 0 */
     status = Cy_IPC_Drv_SendMsgPtr(Cy_IPC_Drv_GetIpcBaseAddress(CM33_S_IPC_CH_NUM),
               CM33_S_IPC_INTR_MASK,   /* notify target: IPC Interrupt 0 (CM33-S) */
               &writeMesg);            /* pointer to message data */
     if (status == CY_IPC_DRV_SUCCESS)
     {
          printf("[CM33-NS] send success \r\n");
     }
     else
     {
          /* CH0 busy — wait for rel_event before retrying */
          printf("[CM33-NS] send failed \r\n");
     }
}

/*******************************************************************************
* Function Name: usecase_cm33ns_cm33s
********************************************************************************
* Summary:
* Wrapper that prints a header and calls send_msg() to demonstrate
* CM33-NS → CM33-S IPC communication.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/
void usecase_cm33ns_cm33s(void)
{
     printf("\n\n\n[cm33-ns] Send message to cm33-s \r\n");

     send_msg();

}

/*******************************************************************************
* Function Name: config_current_core
********************************************************************************
* Summary:
* Registers ipc_isr on IPC Interrupt 1, enables acquire/release masks
* for IPC channel 0, and resets the event flags and message payload.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/
void config_current_core(void)
{
     /* Register ISR for IPC Interrupt 1 and enable in NVIC */
     Cy_SysInt_Init(&ipcTestIntConfig, ipc_isr);
     NVIC_EnableIRQ((IRQn_Type)CM33_NS_IPC_INTR_MUX);

     /* Unmask acquire + release events from CH0 */
     Cy_IPC_Drv_SetInterruptMask(Cy_IPC_Drv_GetIntrBaseAddr(CM33_NS_IPC_INTR_NUM),
               CM33_S_IPC_CH_MASK,   /* release mask */
               CM33_S_IPC_CH_MASK);  /* acquire mask */

     /* Reset flags and message payload */
     rel_event    = false;
     notify_event = false;
     writeMesg    = 1;
}

/*******************************************************************************
* Function Name: test_multi_core_ipc
********************************************************************************
* Summary:
* One-time setup via config_current_core(), then infinite loop that
* sends an IPC message to CM33-S every 1 second.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/
void test_multi_core_ipc(void)
{
     config_current_core();
     while(1){
          usecase_cm33ns_cm33s();
          Cy_SysLib_Delay(1000);
     }
}

/* [] END OF FILE */

