/*----------------------------------------------------------------------------
 *      U S B  -  K e r n e l
 *----------------------------------------------------------------------------
 *      Name:    MSCUSER.C
 *      Purpose: Mass Storage Class Custom User Module
 *      Version: V1.10
 *----------------------------------------------------------------------------
 *      This file is part of the uVision/ARM development tools.
 *      This software may only be used under the terms of a valid, current,
 *      end user licence from KEIL for a compatible version of KEIL software
 *      development tools. Nothing else gives you the right to use it.
 *
 *      Copyright (c) 2005-2007 Keil Software.
 *---------------------------------------------------------------------------*/

#include <LPC23XX.H>                        /* LPC214x definitions */

#include "type.h"

#include "usb.h"
#include "msc.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "mscuser.h"

#include "memory.h"


BYTE  Memory[MSC_MemorySize];  /* MSC RAM */

BOOL  MemOK;                   /* Memory OK */

DWORD Offset;                  /* R/W Offset */
DWORD Length;                  /* R/W Length */

BYTE  BulkStage;               /* Bulk Stage */

#if USB_DMA
#pragma arm section zidata = "USB_RAM"      // data located in USB RAM (neccessry for DMA)
                                            // see file DMA.sct
BYTE  BulkBuf[MSC_MAX_PACKET]; /* Bulk In/Out Buffer */
MSC_CBW CBW;                   /* Command Block Wrapper */
MSC_CSW CSW;                   /* Command Status Wrapper */
#pragma arm section zidata
#else
BYTE  BulkBuf[MSC_MAX_PACKET]; /* Bulk In/Out Buffer */
MSC_CBW CBW;                   /* Command Block Wrapper */
MSC_CSW CSW;                   /* Command Status Wrapper */
#endif
BYTE  BulkLen;                 /* Bulk In/Out Length */



/*
 *  MSC Mass Storage Reset Request Callback
 *   Called automatically on Mass Storage Reset Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

BOOL MSC_Reset (void) {

  FIO2CLR = (LED_RD | LED_WR);  /* Turn Off R/W LED */
  BulkStage = MSC_BS_CBW;
  return (TRUE);
}


/*
 *  MSC Get Max LUN Request Callback
 *   Called automatically on Get Max LUN Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

BOOL MSC_GetMaxLUN (void) {

  EP0Buf[0] = 0;               /* No LUN associated with this device */
  return (TRUE);
}


/*
 *  MSC Memory Read Callback
 *   Called automatically on Memory Read Event
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_MemoryRead (void) {
  DWORD n;
#if USB_DMA
  DWORD i;
#endif

  if (Length > MSC_MAX_PACKET) {
    n = MSC_MAX_PACKET;
  } else {
    n = Length;
  }

  if ((Offset + n) > MSC_MemorySize) {
    n = MSC_MemorySize - Offset;
    BulkStage = MSC_BS_DATA_IN_LAST_STALL;
  }

#if USB_DMA
  if (USB_DMA_IsUsed (MSC_EP_IN)) {                     // Check if EP is used for DMA
    for (i = 0; i < n; i++)                             // copy data to USB DMA RAM
      BulkBuf[i] = Memory[Offset+i];                    // would be better if data is already in USB DMA RAM
  
    USB_WriteEP(MSC_EP_IN, BulkBuf, n);
  } else
    USB_WriteEP(MSC_EP_IN, &Memory[Offset], n);
#else
  USB_WriteEP(MSC_EP_IN, &Memory[Offset], n);
#endif
  Offset += n;
  Length -= n;

  CSW.dDataResidue -= n;

  if (Length == 0) {
    BulkStage = MSC_BS_DATA_IN_LAST;
  }

  if (BulkStage != MSC_BS_DATA_IN) {
    FIO2CLR = LED_RD;            /* Turn Off Read LED */
    CSW.bStatus = CSW_CMD_PASSED;
  }
}


/*
 *  MSC Memory Write Callback
 *   Called automatically on Memory Write Event
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_MemoryWrite (void) {
  DWORD n;

  if ((Offset + BulkLen) > MSC_MemorySize) {
    BulkLen = MSC_MemorySize - Offset;
    BulkStage = MSC_BS_CSW;
    USB_SetStallEP(MSC_EP_OUT);
  }

  for (n = 0; n < BulkLen; n++) {
    Memory[Offset + n] = BulkBuf[n];
  }

  Offset += BulkLen;
  Length -= BulkLen;

  CSW.dDataResidue -= BulkLen;

  if ((Length == 0) || (BulkStage == MSC_BS_CSW)) {
    FIO2CLR = LED_WR;            /* Turn Off Write LED */
    CSW.bStatus = CSW_CMD_PASSED;
    MSC_SetCSW();
  }
}


/*
 *  MSC Memory Verify Callback
 *   Called automatically on Memory Verify Event
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_MemoryVerify (void) {
  DWORD n;

  if ((Offset + BulkLen) > MSC_MemorySize) {
    BulkLen = MSC_MemorySize - Offset;
    BulkStage = MSC_BS_CSW;
    USB_SetStallEP(MSC_EP_OUT);
  }

  for (n = 0; n < BulkLen; n++) {
    if (Memory[Offset + n] != BulkBuf[n]) {
      MemOK = FALSE;
      break;
    }
  }

  Offset += BulkLen;
  Length -= BulkLen;

  CSW.dDataResidue -= BulkLen;

  if ((Length == 0) || (BulkStage == MSC_BS_CSW)) {
    CSW.bStatus = (MemOK) ? CSW_CMD_PASSED : CSW_CMD_FAILED;
    MSC_SetCSW();
  }
}


/*
 *  MSC SCSI Read/Write Setup Callback
 *    Parameters:      None (global variables)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

BOOL MSC_RWSetup (void) {
  DWORD n;

  /* Logical Block Address of First Block */
  n = (CBW.CB[2] << 24) |
      (CBW.CB[3] << 16) |
      (CBW.CB[4] <<  8) |
      (CBW.CB[5] <<  0);

  Offset = n * MSC_BlockSize;

  /* Number of Blocks to transfer */
  n = (CBW.CB[7] <<  8) |
      (CBW.CB[8] <<  0);

  Length = n * MSC_BlockSize;

  if (CBW.dDataLength != Length) {
    USB_SetStallEP(MSC_EP_IN);
    USB_SetStallEP(MSC_EP_OUT);
    CSW.bStatus = CSW_PHASE_ERROR;
    MSC_SetCSW();
    return (FALSE);
  }

  return (TRUE);
}


/*
 *  Check Data IN Format
 *    Parameters:      None (global variables)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

BOOL DataInFormat (void) {

  if (CBW.dDataLength == 0) {
    CSW.bStatus = CSW_PHASE_ERROR;
    MSC_SetCSW();
    return (FALSE);
  }
  if ((CBW.bmFlags & 0x80) == 0) {
    USB_SetStallEP(MSC_EP_OUT);
    CSW.bStatus = CSW_PHASE_ERROR;
    MSC_SetCSW();
    return (FALSE);
  }
  return (TRUE);
}


/*
 *  Perform Data IN Transfer
 *    Parameters:      None (global variables)
 *    Return Value:    TRUE - Success, FALSE - Error
 */

void DataInTransfer (void) {

  if (BulkLen > CBW.dDataLength) {
    BulkLen = CBW.dDataLength;
  }

  USB_WriteEP(MSC_EP_IN, BulkBuf, BulkLen);
  BulkStage = MSC_BS_DATA_IN_LAST;

  CSW.dDataResidue -= BulkLen;
  CSW.bStatus = CSW_CMD_PASSED;
}


/*
 *  MSC SCSI Test Unit Ready Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_TestUnitReady (void) {

  if (CBW.dDataLength != 0) {
    if ((CBW.bmFlags & 0x80) != 0) {
      USB_SetStallEP(MSC_EP_IN);
    } else {
      USB_SetStallEP(MSC_EP_OUT);
    }
  }

  CSW.bStatus = CSW_CMD_PASSED;
  MSC_SetCSW();
}


/*
 *  MSC SCSI Request Sense Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_RequestSense (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x70;          /* Response Code */
  BulkBuf[ 1] = 0x00;
  BulkBuf[ 2] = 0x02;          /* Sense Key */
  BulkBuf[ 3] = 0x00;
  BulkBuf[ 4] = 0x00;
  BulkBuf[ 5] = 0x00;
  BulkBuf[ 6] = 0x00;
  BulkBuf[ 7] = 0x0A;          /* Additional Length */
  BulkBuf[ 8] = 0x00;
  BulkBuf[ 9] = 0x00;
  BulkBuf[10] = 0x00;
  BulkBuf[11] = 0x00;
  BulkBuf[12] = 0x30;          /* ASC */
  BulkBuf[13] = 0x01;          /* ASCQ */
  BulkBuf[14] = 0x00;
  BulkBuf[15] = 0x00;
  BulkBuf[16] = 0x00;
  BulkBuf[17] = 0x00;

  BulkLen = 18;
  DataInTransfer();
}


/*
 *  MSC SCSI Inquiry Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_Inquiry (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x00;          /* Direct Access Device */
  BulkBuf[ 1] = 0x80;          /* RMB = 1: Removable Medium */
  BulkBuf[ 2] = 0x00;          /* Version: No conformance claim to standard */
  BulkBuf[ 3] = 0x01;

  BulkBuf[ 4] = 36-4;          /* Additional Length */
  BulkBuf[ 5] = 0x80;          /* SCCS = 1: Storage Controller Component */
  BulkBuf[ 6] = 0x00;
  BulkBuf[ 7] = 0x00;

  BulkBuf[ 8] = 'K';           /* Vendor Identification */
  BulkBuf[ 9] = 'e';
  BulkBuf[10] = 'i';
  BulkBuf[11] = 'l';
  BulkBuf[12] = ' ';
  BulkBuf[13] = ' ';
  BulkBuf[14] = ' ';
  BulkBuf[15] = ' ';

  BulkBuf[16] = 'L';           /* Product Identification */
  BulkBuf[17] = 'P';
  BulkBuf[18] = 'C';
  BulkBuf[19] = '2';
  BulkBuf[20] = '3';
  BulkBuf[21] = '6';
  BulkBuf[22] = '8';
  BulkBuf[23] = ' ';
  BulkBuf[24] = 'D';
  BulkBuf[25] = 'i';
  BulkBuf[26] = 's';
  BulkBuf[27] = 'k';
  BulkBuf[28] = ' ';
  BulkBuf[29] = ' ';
  BulkBuf[30] = ' ';
  BulkBuf[31] = ' ';

  BulkBuf[32] = '1';           /* Product Revision Level */
  BulkBuf[33] = '.';
  BulkBuf[34] = '0';
  BulkBuf[35] = ' ';

  BulkLen = 36;
  DataInTransfer();
}


/*
 *  MSC SCSI Mode Sense (6-Byte) Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ModeSense6 (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x03;
  BulkBuf[ 1] = 0x00;
  BulkBuf[ 2] = 0x00;
  BulkBuf[ 3] = 0x00;

  BulkLen = 4;
  DataInTransfer();
}


/*
 *  MSC SCSI Mode Sense (10-Byte) Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ModeSense10 (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x00;
  BulkBuf[ 1] = 0x06;
  BulkBuf[ 2] = 0x00;
  BulkBuf[ 3] = 0x00;
  BulkBuf[ 4] = 0x00;
  BulkBuf[ 5] = 0x00;
  BulkBuf[ 6] = 0x00;
  BulkBuf[ 7] = 0x00;

  BulkLen = 8;
  DataInTransfer();
}


/*
 *  MSC SCSI Read Capacity Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ReadCapacity (void) {

  if (!DataInFormat()) return;

  /* Last Logical Block */
  BulkBuf[ 0] = ((MSC_BlockCount - 1) >> 24) & 0xFF;
  BulkBuf[ 1] = ((MSC_BlockCount - 1) >> 16) & 0xFF;
  BulkBuf[ 2] = ((MSC_BlockCount - 1) >>  8) & 0xFF;
  BulkBuf[ 3] = ((MSC_BlockCount - 1) >>  0) & 0xFF;

  /* Block Length */
  BulkBuf[ 4] = (MSC_BlockSize >> 24) & 0xFF;
  BulkBuf[ 5] = (MSC_BlockSize >> 16) & 0xFF;
  BulkBuf[ 6] = (MSC_BlockSize >>  8) & 0xFF;
  BulkBuf[ 7] = (MSC_BlockSize >>  0) & 0xFF;

  BulkLen = 8;
  DataInTransfer();
}


/*
 *  MSC SCSI Read Format Capacity Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_ReadFormatCapacity (void) {

  if (!DataInFormat()) return;

  BulkBuf[ 0] = 0x00;
  BulkBuf[ 1] = 0x00;
  BulkBuf[ 2] = 0x00;
  BulkBuf[ 3] = 0x08;          /* Capacity List Length */

  /* Block Count */
  BulkBuf[ 4] = (MSC_BlockCount >> 24) & 0xFF;
  BulkBuf[ 5] = (MSC_BlockCount >> 16) & 0xFF;
  BulkBuf[ 6] = (MSC_BlockCount >>  8) & 0xFF;
  BulkBuf[ 7] = (MSC_BlockCount >>  0) & 0xFF;

  /* Block Length */
  BulkBuf[ 8] = 0x02;          /* Descriptor Code: Formatted Media */
  BulkBuf[ 9] = (MSC_BlockSize >> 16) & 0xFF;
  BulkBuf[10] = (MSC_BlockSize >>  8) & 0xFF;
  BulkBuf[11] = (MSC_BlockSize >>  0) & 0xFF;

  BulkLen = 12;
  DataInTransfer();
}


/*
 *  MSC Get Command Block Wrapper Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_GetCBW (void) {
  DWORD n;

  for (n = 0; n < BulkLen; n++) {
    *((BYTE *)&CBW + n) = BulkBuf[n];
  }
  if ((BulkLen == sizeof(CBW)) && (CBW.dSignature == MSC_CBW_Signature)) {
    /* Valid CBW */
    CSW.dTag = CBW.dTag;
    CSW.dDataResidue = CBW.dDataLength;
    if ((CBW.bLUN != 0) || (CBW.bCBLength < 1) || CBW.bCBLength > 16) {
fail: CSW.bStatus = CSW_CMD_FAILED;
      MSC_SetCSW();
    } else {
      switch (CBW.CB[0]) {
        case SCSI_TEST_UNIT_READY:
          MSC_TestUnitReady();
          break;
        case SCSI_REQUEST_SENSE:
          MSC_RequestSense();
          break;
        case SCSI_FORMAT_UNIT:
          goto fail;
        case SCSI_INQUIRY:
          MSC_Inquiry();
          break;
        case SCSI_START_STOP_UNIT:
          goto fail;
        case SCSI_MEDIA_REMOVAL:
          goto fail;
        case SCSI_MODE_SELECT6:
          goto fail;
        case SCSI_MODE_SENSE6:
          MSC_ModeSense6();
          break;
        case SCSI_MODE_SELECT10:
          goto fail;
        case SCSI_MODE_SENSE10:
          MSC_ModeSense10();
          break;
        case SCSI_READ_FORMAT_CAPACITIES:
          MSC_ReadFormatCapacity();
          break;
        case SCSI_READ_CAPACITY:
          MSC_ReadCapacity();
          break;
        case SCSI_READ10:
          if (MSC_RWSetup()) {
            if ((CBW.bmFlags & 0x80) != 0) {
              FIO2SET = LED_RD;    /* Turn On Read LED */
              BulkStage = MSC_BS_DATA_IN;
              MSC_MemoryRead();
            } else {
              USB_SetStallEP(MSC_EP_OUT);
              CSW.bStatus = CSW_PHASE_ERROR;
              MSC_SetCSW();
            }
          }
          break;
        case SCSI_WRITE10:
          if (MSC_RWSetup()) {
            if ((CBW.bmFlags & 0x80) == 0) {
              FIO2SET = LED_WR;    /* Turn On Write LED */
              BulkStage = MSC_BS_DATA_OUT;
            } else {
              USB_SetStallEP(MSC_EP_IN);
              CSW.bStatus = CSW_PHASE_ERROR;
              MSC_SetCSW();
            }
          }
          break;
        case SCSI_VERIFY10:
          if (MSC_RWSetup()) {
            if ((CBW.bmFlags & 0x80) == 0) {
              BulkStage = MSC_BS_DATA_OUT;
              MemOK = TRUE;
            } else {
              USB_SetStallEP(MSC_EP_IN);
              CSW.bStatus = CSW_PHASE_ERROR;
              MSC_SetCSW();
            }
          }
          break;
        default:
          goto fail;
      }
    }
  } else {
    /* Invalid CBW */
    USB_SetStallEP(MSC_EP_IN);
    USB_SetStallEP(MSC_EP_OUT);
    BulkStage = MSC_BS_ERROR;
  }
}


/*
 *  MSC Set Command Status Wrapper Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_SetCSW (void) {

  CSW.dSignature = MSC_CSW_Signature;
  USB_WriteEP(MSC_EP_IN, (BYTE *)&CSW, sizeof(CSW));
  BulkStage = MSC_BS_CSW;
}


/*
 *  MSC Bulk In Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_BulkIn (void) {

  switch (BulkStage) {
    case MSC_BS_DATA_IN:
      switch (CBW.CB[0]) {
        case SCSI_READ10:
          MSC_MemoryRead();
          break;
      }
      break;
    case MSC_BS_DATA_IN_LAST:
      MSC_SetCSW();
      break;
    case MSC_BS_DATA_IN_LAST_STALL:
      USB_SetStallEP(MSC_EP_IN);
      MSC_SetCSW();
      break;
    case MSC_BS_CSW:
      BulkStage = MSC_BS_CBW;
      break;
  }
}


/*
 *  MSC Bulk Out Callback
 *    Parameters:      None (global variables)
 *    Return Value:    None
 */

void MSC_BulkOut (void) {

  BulkLen = USB_ReadEP(MSC_EP_OUT, BulkBuf);    // data is alwys copied to BulkBuf if DMA is used 
                                                // see USB_ReadEP()     in file usbhw.c
                                                // see USB_EndPoint2 () in file usbuser.c
  switch (BulkStage) {
    case MSC_BS_CBW:
      MSC_GetCBW();
      break;
    case MSC_BS_DATA_OUT:
      switch (CBW.CB[0]) {
        case SCSI_WRITE10:
          MSC_MemoryWrite();
          break;
        case SCSI_VERIFY10:
          MSC_MemoryVerify();
          break;
      }
      break;
    default:
      USB_SetStallEP(MSC_EP_OUT);
      CSW.bStatus = CSW_PHASE_ERROR;
      MSC_SetCSW();
      break;
  }
}
