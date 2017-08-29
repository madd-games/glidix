/*
	Glidix kernel

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef EHCI_H_
#define EHCI_H_

#include <glidix/common.h> 
#include <glidix/dma.h>
#include <glidix/semaphore.h>

#define	EHCI_LEGSUP_OWN_OS		(1 << 24)
#define	EHCI_LEGSUP_OWN_BIOS		(1 << 16)

#define	EHCI_HCS_PPC			(1 << 4)

#define	EHCI_USBCMD_ASYNC_ENABLE	(1 << 5)
#define	EHCI_USBCMD_HCRESET		(1 << 1)
#define	EHCI_USBCMD_RUN			(1 << 0)

#define	EHCI_USBSTS_ASYNC		(1 << 15)
#define	EHCI_USBSTS_PORTCH		(1 << 2)
#define	EHCI_USBSTS_USBINT		(1 << 0)

#define	EHCI_USBINTR_PORTCH		(1 << 2)
#define	EHCI_USBINTR_USBINT		(1 << 0)

#define	EHCI_PORT_OWNER			(1 << 13)
#define	EHCI_PORT_POWER			(1 << 12)
#define	EHCI_PORT_RESET			(1 << 8)
#define	EHCI_PORT_ENABLED		(1 << 2)
#define	EHCI_PORT_CONNCH		(1 << 1)
#define	EHCI_PORT_CONNECTED		(1 << 0)

#define	EHCI_HORPTR_QH			2

#define	EHCI_HORPTR_TERM		(1 << 0)

#define	EHCI_SPEED_FULL			0
#define	EHCI_SPEED_LOW			1
#define	EHCI_SPEED_HIGH			2

#define	EHCI_PID_OUT			0
#define	EHCI_PID_IN			1
#define	EHCI_PID_SETUP			2

#define	EHCI_TD_DT			(1 << 31)
#define	EHCI_TD_IOC			(1 << 15)
#define	EHCI_TD_ACTIVE			(1 << 7)

#define	EHCI_QH_CONTROL			(1 << 27)
#define	EHCI_QH_HEAD			(1 << 15)
#define	EHCI_QH_DTC			(1 << 14)

typedef struct
{
	volatile uint8_t		caplen;
	volatile uint8_t		resv0;
	volatile uint16_t		version;
	volatile uint32_t		hcsparams;
	volatile uint32_t		hccparams;
} EhciCaps;

typedef struct
{
	volatile uint32_t		usbcmd;
	volatile uint32_t		usbsts;
	volatile uint32_t		usbintr;
	volatile uint32_t		frindex;
	volatile uint32_t		segment;
	volatile uint32_t		periodicBase;
	volatile uint32_t		asyncListAddr;
	volatile uint32_t		resv[9];
	volatile uint32_t		configFlag;
	volatile uint32_t		ports[15];
} EhciRegs;

typedef struct
{
	volatile uint32_t		horptr;
	volatile uint32_t		altHorptr;
	volatile uint32_t		token;
	volatile uint32_t		bufs[5];
} EhciTD;

typedef struct
{
	volatile uint32_t		horptr;
	volatile uint32_t		endpointInfo;
	volatile uint32_t		endpointCaps;
	volatile uint32_t		addrTD;
	volatile EhciTD			overlay;
} EhciQH;

/**
 * Represents a "request buffer" which forms a queue of requests to execute.
 */
struct EhciQueue_;
typedef struct EhciRequestBuffer_
{
	/**
	 * Link to the next one.
	 */
	struct EhciRequestBuffer_*	next;
	
	/**
	 * The request.
	 */
	USBRequest*			urb;
	
	// --- FIELDS USED IN ACTIVE TRANSACTIONS ONLY ---
	
	/**
	 * The hardware queue that the TDs were placed on, if this is a currently
	 * active transaction.
	 */
	struct EhciQueue_*		hwq;
	
	/**
	 * Index of the next TD we are waiting for.
	 */
	int				tdNext;
	
	/**
	 * Number of TDs making up this transaction.
	 */
	int				tdCount;
	
	/**
	 * DMA buffer used for the transaction content.
	 */
	DMABuffer			dmabuf;
} EhciRequestBuffer;

/**
 * Driver representation of a queue.
 */
typedef struct EhciQueue_
{
	/**
	 * Links.
	 */
	struct EhciQueue_*		prev;
	struct EhciQueue_*		next;
	
	/**
	 * The DMA buffer containing the EhciQH.
	 */
	DMABuffer			dmabuf;
	
	/**
	 * The queue head in virtual memory. Directly followed by 126 transfer
	 * descriptors for use by this queue.
	 */
	EhciQH*				qh;
	
	/**
	 * Physical address of queue head.
	 */
	uint32_t			physQH;
} EhciQueue;

/**
 * Layout of the "set address" request area.
 */
typedef struct
{
	EhciTD				tdSetup;
	EhciTD				tdStatus;
	USBSetupPacket			setupData;
} EhciSetAddr;

#endif
