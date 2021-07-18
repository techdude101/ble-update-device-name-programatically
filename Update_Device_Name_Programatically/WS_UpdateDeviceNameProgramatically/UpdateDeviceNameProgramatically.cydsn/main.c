/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include <project.h>

/***************************************
*        API Constants
***************************************/
#define LED_ON                                      (0u)
#define LED_OFF                                     (1u)

#define LOOP_DELAY                                  (1u)  /* How often would you like to update the ADV payload */

/* ADV payload dta structure */   
#define advPayload                                  (cyBle_discoveryModeInfo.advData->advData) 

/***************************************
*        Function Prototypes
***************************************/
void InitializeSystem(void);
void StackEventHandler(uint32 event, void* eventParam);
void EnterLowPowerMode(void);
void DynamicADVPayloadUpdate(void);

/***************************************
*        Global variables
***************************************/
uint16 count = 0;

int main (void)
{
    InitializeSystem();
    
    LED_R_Write(LED_ON);
    CyDelay(500);
    LED_R_Write(LED_OFF);
    
    for(;;)
    {
        /* Single API call to service all the BLE stack events. Must be
         * called at least once in a BLE connection interval */
        CyBle_ProcessEvents();
        
        LED_R_Write(LED_OFF);
        
        /* Configure the system in lowest possible power modes during and between BLE ADV events */
        EnterLowPowerMode();
        
        LED_R_Write(LED_ON);
        
        DynamicADVPayloadUpdate();
    }
}


/*******************************************************************************
* Function Name: InitializeSystem
********************************************************************************
*
* Summary:
*  This routine initializes all the componnets and firmware state.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void InitializeSystem(void)
{
    CYBLE_API_RESULT_T apiResult;

    CyGlobalIntEnable;

    apiResult = CyBle_Start(StackEventHandler); /* Init the BLE stack and register an applicaiton callback */

    if(apiResult != CYBLE_ERROR_OK)
    {
        /* BLE stack initialization failed, check your configuration */
        CYASSERT(0);
    }
    
    /* Set XTAL divider to 3MHz mode */
    CySysClkWriteEcoDiv(CY_SYS_CLK_ECO_DIV8); 
    
    /* ILO is no longer required, shut it down */
    CySysClkIloStop();
}

/*******************************************************************************
* Function Name: EnterLowPowerMode
********************************************************************************
*
* Summary:
*  This configures the BLESS and system in low power mode whenever possible.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void EnterLowPowerMode(void)
{
    CYBLE_BLESS_STATE_T blessState;
    uint8 intrStatus;
    
    /* Configure BLESS in Deep-Sleep mode */
    CyBle_EnterLPM(CYBLE_BLESS_DEEPSLEEP);
    
    /* Prevent interrupts while entering system low power modes */
    intrStatus = CyEnterCriticalSection();
    
    /* Get the current state of BLESS block */
    blessState = CyBle_GetBleSsState();
    
    /* If BLESS is in Deep-Sleep mode or the XTAL oscillator is turning on, 
     * then PSoC 4 BLE can enter Deep-Sleep mode (1.3uA current consumption) */
    if(blessState == CYBLE_BLESS_STATE_ECO_ON || 
        blessState == CYBLE_BLESS_STATE_DEEPSLEEP)
    {
        CySysPmDeepSleep();
    }
    else if(blessState != CYBLE_BLESS_STATE_EVENT_CLOSE)
    {
        /* If BLESS is active, then configure PSoC 4 BLE system in 
         * Sleep mode (~1.6mA current consumption) */
        CySysClkWriteHfclkDirect(CY_SYS_CLK_HFCLK_ECO);
        CySysClkImoStop();
        CySysPmSleep();
        CySysClkImoStart();
        CySysClkWriteHfclkDirect(CY_SYS_CLK_HFCLK_IMO);
    }
    else
    {
        /* Keep trying to enter either Sleep or Deep-Sleep mode */    
    }
    
    CyExitCriticalSection(intrStatus);
}


/*******************************************************************************
* Function Name: StackEventHandler
********************************************************************************
*
* Summary:
*  This is an event callback function to receive events from the BLE Component.
*
* Parameters:
*  uint8 event:       Event from the CYBLE component
*  void* eventParams: A structure instance for corresponding event type. The
*                     list of event structure is described in the component
*                     datasheet.
*
* Return:
*  None
*
*******************************************************************************/
void StackEventHandler(uint32 event, void *eventParam)
{
    switch(event)
    {
        /* Mandatory events to be handled by Find Me Target design */
        case CYBLE_EVT_STACK_ON:
        case CYBLE_EVT_GAP_DEVICE_DISCONNECTED:
            /* Start BLE advertisement for 30 seconds and update link
             * status on LEDs */
            CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_SLOW);
            //LED_R_Write(LED_ON);
          break;

        case CYBLE_EVT_GAPP_ADVERTISEMENT_START_STOP:
            if(CyBle_GetState() == CYBLE_STATE_DISCONNECTED)
            {
                /* Advertisement event timed out, go to low power
                 * mode (Hibernate mode) and wait for an external
                 * user event to wake up the device again */
                //LED_R_Write(LED_OFF);
            }
            break;

        default:
        break;
    }
}

/*******************************************************************************
* Function Name: DynamicADVPayloadUpdate
********************************************************************************
*
* Summary:
*  This routine dynamically updates the BLE advertisement packet
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void DynamicADVPayloadUpdate(void)
{
    if(CyBle_GetBleSsState() == CYBLE_BLESS_STATE_EVENT_CLOSE)
    {
        count++; /* Loop counter */
        if (count > 9) {
            count = 0;
        }
        
        /* Once the system enters Sleep/Deepsleep mode during advertisement, the source of wake-up is the next  
         * advertisement interval which has a wakeup interval of 1 advertisement (ADV) interval (100ms). 
         * LOOP_DELAY * ADV interval is the interval after which ADV data is updated in this firmware.*/
        
        advPayload[9] = (uint8_t)count + '0';
        
        CyBle_GapUpdateAdvData(cyBle_discoveryModeInfo.advData, cyBle_discoveryModeInfo.scanRspData);
    }
}

/* [] END OF FILE */
