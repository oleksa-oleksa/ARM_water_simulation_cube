/*----------------------------------------------------------------------------
 *      U S B  -  K e r n e l
 *----------------------------------------------------------------------------
 *      Name:    USBUSER.C
 *      Purpose: USB Custom User Module
 *      Version: V1.10
 *----------------------------------------------------------------------------
 *      This software is supplied "AS IS" without any warranties, express,
 *      implied or statutory, including but not limited to the implied
 *      warranties of fitness for purpose, satisfactory quality and
 *      noninfringement. Keil extends you a royalty-free right to reproduce
 *      and distribute executable files created using this software for use
 *      on Philips LPC2xxx microcontroller devices only. Nothing else gives
 *      you the right to use this software.
 *
 *      Copyright (c) 2005-2006 Keil Software.
 *---------------------------------------------------------------------------*/

#include "LPC23xX.H"                        /* LPC23xx definitions */
#include "type.h"
#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "usbuser.h"

#include "usbaudio.h"


/*
 *  USB Power Event Callback
 *   Called automatically on USB Power Event
 *    Parameter:       power: On(TRUE)/Off(FALSE)
 */

#if USB_POWER_EVENT
void USB_Power_Event (BOOL  power) {
  power;
}
#endif


/*
 *  USB Reset Event Callback
 *   Called automatically on USB Reset Event
 */

#if USB_RESET_EVENT
void USB_Reset_Event (void) {
  USB_ResetCore();
}
#endif


/*
 *  USB Suspend Event Callback
 *   Called automatically on USB Suspend Event
 */

#if USB_SUSPEND_EVENT
void USB_Suspend_Event (void) {
}
#endif


/*
 *  USB Resume Event Callback
 *   Called automatically on USB Resume Event
 */

#if USB_RESUME_EVENT
void USB_Resume_Event (void) {
}
#endif


/*
 *  USB Remote Wakeup Event Callback
 *   Called automatically on USB Remote Wakeup Event
 */

#if USB_WAKEUP_EVENT
void USB_WakeUp_Event (void) {
}
#endif


/*
 *  USB Start of Frame Event Callback
 *   Called automatically on USB Start of Frame Event
 */

#if USB_SOF_EVENT
void USB_SOF_Event (void) {
#if USB_DMA == 0
  if (USB_ReadEP(0x03, (BYTE *)&DataBuf[DataIn])) {
    /* Data Available */
    DataIn += P_S;                          /* Update Data In Index */
    DataIn &= B_S - 1;                      /* Adjust Data In Index */
    if (((DataIn - DataOut) & (B_S - 1)) == (B_S/2)) {
      DataRun = 1;                          /* Data Stream running */
    }
  } else {
    /* No Data */
    DataRun  = 0;                           /* Data Stream not running */
    DataOut  = DataIn;                      /* Initialize Data Indexes */
  }
#endif
}
#endif


/*
 *  USB Error Event Callback
 *   Called automatically on USB Error Event
 *    Parameter:       error: Error Code
 */

#if USB_ERROR_EVENT
void USB_Error_Event (DWORD error) {
  error;
}
#endif


/*
 *  USB Set Configuration Event Callback
 *   Called automatically on USB Set Configuration Request
 */

#if USB_CONFIGURE_EVENT
void USB_Configure_Event (void) {
}
#endif


/*
 *  USB Set Interface Event Callback
 *   Called automatically on USB Set Interface Request
 */

#if USB_INTERFACE_EVENT
void USB_Interface_Event (void) {
}
#endif


/*
 *  USB Set/Clear Feature Event Callback
 *   Called automatically on USB Set/Clear Feature Request
 */

#if USB_FEATURE_EVENT
void USB_Feature_Event (void) {
}
#endif


#define P_EP(n) ((USB_EP_EVENT & (1 << (n))) ? USB_EndPoint##n : NULL)

/* USB Endpoint Events Callback Pointers */
void (* const USB_P_EP[16]) (DWORD event) = {
  P_EP(0),
  P_EP(1),
  P_EP(2),
  P_EP(3),
  P_EP(4),
  P_EP(5),
  P_EP(6),
  P_EP(7),
  P_EP(8),
  P_EP(9),
  P_EP(10),
  P_EP(11),
  P_EP(12),
  P_EP(13),
  P_EP(14),
  P_EP(15),
};


/*
 *  USB Endpoint 1 Event Callback
 *   Called automatically on USB Endpoint 1 Event
 *    Parameter:       event
 */

void USB_EndPoint1 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 2 Event Callback
 *   Called automatically on USB Endpoint 2 Event
 *    Parameter:       event
 */

void USB_EndPoint2 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 3 Event Callback
 *   Called automatically on USB Endpoint 3 Event
 *    Parameter:       event
 */

void USB_EndPoint3 (DWORD event) {
#if USB_DMA
  USB_DMA_DESCRIPTOR DD;

  if (event & USB_EVT_OUT_DMA_EOT) {
    /* End of Transfer */
    if (USB_DMA_BufAdr(0x03) != ((DWORD)DataBuf + 2*DataIn)) {
      /* Data Available */
      DataIn += P_C*P_S;                    /* Update Data In Index */
      DataIn &= B_S - 1;                    /* Adjust Data In Index */
      if (((DataIn - DataOut) & (B_S - 1)) == (B_S/2)) {
        DataRun = 1;                        /* Data Stream running */
      }
    } else {
      /* No Data */
      DataRun = 0;                          /* Data Stream not running */
      DataOut = DataIn;                     /* Initialize Data Indexes */
    }
  }
  if (event & (USB_EVT_OUT_DMA_EOT) | (USB_EVT_OUT_DMA_NDR)) {
    /* End of Transfer or New Descriptor Request */
    DD.BufAdr  = (DWORD)DataBuf + 2*DataIn; /* DMA Buffer Address */
    DD.BufLen  = P_C;                       /* DMA Packet Count */
    DD.MaxSize = 0;                         /* Must be 0 for Iso Transfer */
    DD.InfoAdr = (DWORD)InfoBuf;            /* Packet Info Buffer Address */
    DD.Cfg.Val = 0;                         /* Initial DMA Configuration */
    DD.Cfg.Type.IsoEP = 1;                  /* Iso Endpoint */
    USB_DMA_Setup (0x03, &DD);              /* Setup DMA */
    USB_DMA_Enable(0x03);                   /* Enable DMA */
  }
#else
  event = event;
#endif
}


/*
 *  USB Endpoint 4 Event Callback
 *   Called automatically on USB Endpoint 4 Event
 *    Parameter:       event
 */

void USB_EndPoint4 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 5 Event Callback
 *   Called automatically on USB Endpoint 5 Event
 *    Parameter:       event
 */

void USB_EndPoint5 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 6 Event Callback
 *   Called automatically on USB Endpoint 6 Event
 *    Parameter:       event
 */

void USB_EndPoint6 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 7 Event Callback
 *   Called automatically on USB Endpoint 7 Event
 *    Parameter:       event
 */

void USB_EndPoint7 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 8 Event Callback
 *   Called automatically on USB Endpoint 8 Event
 *    Parameter:       event
 */

void USB_EndPoint8 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 9 Event Callback
 *   Called automatically on USB Endpoint 9 Event
 *    Parameter:       event
 */

void USB_EndPoint9 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 10 Event Callback
 *   Called automatically on USB Endpoint 10 Event
 *    Parameter:       event
 */

void USB_EndPoint10 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 11 Event Callback
 *   Called automatically on USB Endpoint 11 Event
 *    Parameter:       event
 */

void USB_EndPoint11 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 12 Event Callback
 *   Called automatically on USB Endpoint 12 Event
 *    Parameter:       event
 */

void USB_EndPoint12 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 13 Event Callback
 *   Called automatically on USB Endpoint 13 Event
 *    Parameter:       event
 */

void USB_EndPoint13 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 14 Event Callback
 *   Called automatically on USB Endpoint 14 Event
 *    Parameter:       event
 */

void USB_EndPoint14 (DWORD event) {
  event = event;
}


/*
 *  USB Endpoint 15 Event Callback
 *   Called automatically on USB Endpoint 15 Event
 *    Parameter:       event
 */

void USB_EndPoint15 (DWORD event) {
  event = event;
}
