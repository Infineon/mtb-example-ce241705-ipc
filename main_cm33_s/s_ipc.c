/******************************************************************************
* File Name:   s_ipc.c
*
* Description: IPC configuration and ISR for the CM33 Secure core.
*              Sets up IPC channel 0 (CM33_S_IPC_CH_NUM) to receive
*              notify messages from the CM33 Non-Secure core and sends
*              a release event back after processing.
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
#include "cy_pdl.h"
#include "cy_ms_ctl.h"
#include "cy_ipc_drv.h"
#include "cy_sysint.h"
#include "cy_ipc_sema.h"
#include "cy_ipc_pipe.h"

/*******************************************************************************
* Macros
*******************************************************************************/

/* IPC Interrupt 1 — release-notify target for CM33-NS */
#define CM33_NS_IPC_INTR_NUM               (CY_IPC_INTR_USER)          /* IPC interrupt index 1 */
#define CM33_NS_IPC_INTR_MASK              (CY_IPC_INTR_MASK(CM33_NS_IPC_INTR_NUM))  /* BIT(1) */

/* NVIC priority for the CM33-S IPC interrupt handler */
#define CM33_S_IPC_INTR_PRIORITY           (1U)

/*******************************************************************************
* Global Variables
*******************************************************************************/

/* CM33-S IPC interrupt config: IPC Intr 0 MUX → NVIC */
const cy_stc_sysint_t ipcIntConfig =
{
          .intrSrc = (IRQn_Type)CM33_S_IPC_INTR_MUX,
          .intrPriority = CM33_S_IPC_INTR_PRIORITY
};

/* Placeholder message data (unused in receive-only secure path) */
uint32_t writeMesg;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void ipc_cm33_s_isr(void);
int config_ipc(void);

/*******************************************************************************
* Function Name: ipc_cm33_s_isr
********************************************************************************
* Summary:
* ISR for IPC Interrupt 0 on the secure core.  Triggered when CM33-NS
* acquires (notifies) IPC channel 0.  Toggles the user LED and releases
* the channel back to CM33-NS.
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/

void ipc_cm33_s_isr(void)
{
     uint32_t shadowIntr;
     IPC_STRUCT_Type *ipcPtr;
     IPC_INTR_STRUCT_Type *ipcIntrPtr;
     uint32_t notifyMask;

     /* IPC Interrupt 0 base address */
     ipcIntrPtr = Cy_IPC_Drv_GetIntrBaseAddr(CM33_S_IPC_INTR_NUM);

     /* Read masked interrupt status */
     shadowIntr = Cy_IPC_Drv_GetInterruptStatusMasked(ipcIntrPtr);

     /* Extract acquire (notify) bits */
     notifyMask = Cy_IPC_Drv_ExtractAcquireMask(shadowIntr);

     /* CM33-NS sent a message on IPC channel 0 */
     if (0UL != notifyMask)
     {
          /* Clear acquire bits to prevent re-entry */
          Cy_IPC_Drv_ClearInterrupt(ipcIntrPtr, CY_IPC_NO_NOTIFICATION, notifyMask);

          /* Get IPC channel 0 handle */
          ipcPtr = Cy_IPC_Drv_GetIpcBaseAddress(CM33_S_IPC_CH_NUM);

          /* Verify lock — confirms valid data from CM33-NS */
          if (Cy_IPC_Drv_IsLockAcquired(ipcPtr))
          {
               /* Toggle LED to indicate IPC message received */
               Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
          }

          /* Release CH0 and notify CM33-NS via IPC Interrupt 1 */
          Cy_IPC_Drv_LockRelease(ipcPtr, CY_IPC_INTR_MASK(CM33_NS_IPC_INTR_NUM));
     }
}

/*******************************************************************************
* Function Name: config_ipc
********************************************************************************
* Summary:
* Registers the secure IPC ISR on IPC Interrupt 0 and enables
* acquire/release event masks for IPC channel 0.  Called once
* from main() before any NS code runs.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int config_ipc(void)
{
     cy_en_sysint_status_t intrStatus = CY_SYSINT_SUCCESS;

     /* Register ISR for IPC Interrupt 0 */
     intrStatus = Cy_SysInt_Init(&ipcIntConfig, ipc_cm33_s_isr);

     /* Enable global interrupts */
     __enable_irq();

     /* Enable IPC Interrupt 0 in NVIC */
     NVIC_EnableIRQ((IRQn_Type)CM33_S_IPC_INTR_MUX);

     /* Unmask acquire + release events from CH0 */
     Cy_IPC_Drv_SetInterruptMask(Cy_IPC_Drv_GetIntrBaseAddr(CM33_S_IPC_INTR_NUM),
               CM33_S_IPC_CH_MASK,   /* release mask */
               CM33_S_IPC_CH_MASK);  /* acquire mask */

     return (int)intrStatus;
}
/* [] END OF FILE */
