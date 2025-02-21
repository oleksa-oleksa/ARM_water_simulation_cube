/*****************************************************************************
 *   irq.c: Interrupt handler C file for NXP LPC23xx/24xx Family Microprocessors
 *
 *   Copyright(C) 2006, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2006.07.13  ver 1.00    Prelimnary version, first Release
 *
******************************************************************************/
#include "LPC23xx.h"			/* LPC23XX/24xx Peripheral Registers */
#include "irq.h"
#if FIQ
#include "timer.h"
#endif

/******************************************************************************
** Function name:		FIQ_Handler
**
** Descriptions:		FIQ interrupt handler called in startup
** parameters:			 
**					
** Returned value:		
** 
******************************************************************************/
void FIQ_Handler( void ) __irq
{
#if FIQ
  if ( VICFIQStatus & (0x1<<4) && VICIntEnable & (0x1<<4) )
  {
	Timer0FIQHandler();	
  }
  if ( VICFIQStatus & (0x1<<5) && VICIntEnable & (0x1<<5) )
  {
	Timer1FIQHandler();	
  }
  return;
#endif 
}

/* Initialize the interrupt controller */
/******************************************************************************
** Function name:		init_VIC
**
** Descriptions:		Initialize VIC interrupt controller.
** parameters:			None
** Returned value:		None
** 
******************************************************************************/
void init_VIC(void) 
{
    uint32_t i = 0;
    uint32_t *vect_addr, *vect_prio;
   	
    /* initialize VIC*/
    VICIntEnClr = 0xffffffff;
    VICVectAddr = 0;
    VICIntSelect = 0;

    /* set all the vector and vector control register to 0 */
    for ( i = 0; i < VIC_SIZE; i++ )
    {
		vect_addr = (uint32_t *)(VIC_BASE_ADDR + VECT_ADDR_INDEX + i*4);
		vect_prio = (uint32_t *)(VIC_BASE_ADDR + VECT_PRIO_INDEX + i*4);
		*vect_addr = 0x0;	
		*vect_prio = 0xF;
    }
    return;
}

/******************************************************************************
** Function name:		install_irq
**
** Descriptions:		Install interrupt handler
** parameters:			Interrupt number, interrupt handler address, 
**						interrupt priority
** Returned value:		1 or 0, return 0 if IntNum is out of range
** 
******************************************************************************/
uint32_t install_irq( uint32_t IntNumber, void *HandlerAddr, uint32_t Priority )
{
    uint32_t *vect_addr;
    uint32_t *vect_prio;
      
    VICIntEnClr = 1 << IntNumber;	/* Disable Interrupt */
    if ( IntNumber >= VIC_SIZE )
    {
		return ( 0 );
    }
    else
    {
		/* find first un-assigned VIC address for the handler */
		vect_addr = (uint32_t *)(VIC_BASE_ADDR + VECT_ADDR_INDEX + IntNumber*4);
		vect_prio = (uint32_t *)(VIC_BASE_ADDR + VECT_PRIO_INDEX + IntNumber*4);
		*vect_addr = (uint32_t)HandlerAddr;	/* set interrupt vector */
		*vect_prio = Priority;
		VICIntEnable = 1 << IntNumber;	/* Enable Interrupt */
		return( 1 );
    }
}

/******************************************************************************
**                            End Of File
******************************************************************************/
