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

#ifndef OHCI_H_
#define OHCI_H_

#include <glidix/common.h>
#include <glidix/usb.h>

/**
 * Bits of the hcControl register.
 */
#define	HC_CTRL_CLE					(1 << 4)
#define	HC_CTRL_IR					(1 << 8)
#define	HC_CTRL_HCFS(x)					(((x) >> 6) & 3)

/**
 * Bits of the hcCommandStatus register.
 */
#define	HC_CMD_HCR					(1 << 0)
#define	HC_CMD_OCR					(1 << 3)

/**
 * USB states.
 */
#define	HC_USB_RESET					0
#define	HC_USB_RESUME					1
#define	HC_USB_OPERATIONAL				2
#define	HC_USB_SUSPEND					3

/**
 * OHCI interrupt bits (for HcInterruptStatus and HcInterruptEnable).
 * HC_INT_MIE is for HcInterruptEnable only.
 */
#define	HC_INT_SO					(1 << 0)
#define	HC_INT_WDH					(1 << 1)
#define	HC_INT_SF					(1 << 2)
#define	HC_INT_RD					(1 << 3)
#define	HC_INT_UE					(1 << 4)
#define	HC_INT_FNO					(1 << 5)
#define	HC_INT_RHSC					(1 << 6)
#define	HC_INT_OC					(1 << 30)
#define	HC_INT_MIE					(1 << 31)

/**
 * Port status register bits.
 */
#define	HC_PORT_CCS					(1 << 0)
#define	HC_PORT_PRS					(1 << 4)

/**
 * Endpoint descriptor flags.
 */
#define	HC_ED_FA(fa)					fa
#define	HC_ED_EN(en)					((en) << 6)
#define	HC_ED_OUT					(1 << 11)
#define	HC_ED_IN					(1 << 12)
#define	HC_ED_FULLSPEED					0
#define	HC_ED_LOWSPEED					(1 << 13)
#define	HC_ED_SKIP					(1 << 14)
#define	HC_ED_ISOCHRONOUS				(1 << 15)
#define	HC_ED_MPS(mps)					((mps) << 16)

/**
 * General Transfer Descriptor flags.
 */
#define	HC_TD_R						(1 << 8)
#define	HC_TD_DP_SETUP					0
#define	HC_TD_DP_OUT					(1 << 9)
#define	HC_TD_DP_IN					(1 << 10)

/**
 * OHCI Operational Registers.
 */
typedef struct
{
	volatile uint32_t				hcRevision;
	volatile uint32_t				hcControl;
	volatile uint32_t				hcCommandStatus;
	volatile uint32_t				hcInterruptStatus;
	volatile uint32_t				hcInterruptEnable;
	volatile uint32_t				hcInterruptDisable;
	volatile uint32_t				hcHCCA;
	volatile uint32_t				hcPeriodCurrentED;
	volatile uint32_t				hcControlHeadED;
	volatile uint32_t				hcControlCurrentED;
	volatile uint32_t				hcBulkHeadED;
	volatile uint32_t				hcBulkCurrentED;
	volatile uint32_t				hcDoneHead;
	volatile uint32_t				hcFmInterval;
	volatile uint32_t				hcFmRemaining;
	volatile uint32_t				hcFmNumber;
	volatile uint32_t				hcPeriodicStart;
	volatile uint32_t				hcLSThreshold;
	volatile uint32_t				hcRhDescriptorA;
	volatile uint32_t				hcRhDescriptorB;
	volatile uint32_t				hcRhStatus;
	volatile uint32_t				hcRhPortStatus[15];
} OHCIRegs;

/**
 * Host Controller Communications Area.
 */
typedef struct
{
	volatile uint32_t				hccaInterruptTable[32];
	volatile uint32_t				hccaFrameNumber;		/* only bottom 16 bits used */
	volatile uint32_t				hccaDoneHead;
	volatile uint32_t				hccaResv[29];
} HCCA;

/**
 * Represents an OHCI port.
 */
typedef struct
{
	uint32_t					index;
	USBDevice*					usbdev;				/* NULL = no device connected */
} OHCIPort;

/**
 * OHCI Endpoint Descriptor (ED)
 */
typedef struct
{
	volatile uint32_t				edFlags;
	volatile uint32_t				edTailP;
	volatile uint32_t				edHeadPCH;
	volatile uint32_t				edNext;
} OHCIEndpointDescriptor;

/**
 * OHCI General Transfer Descriptor (TD)
 */
typedef struct
{
	volatile uint32_t				tdFlags;
	volatile uint32_t				tdBuffer;
	volatile uint32_t				tdNext;
	volatile uint32_t				tdBufferEnd;	
} OHCIGeneralTD;

/**
 * Format of the Communications Page.
 */
typedef struct
{
	HCCA						hcca;
	OHCIEndpointDescriptor				initED;
	OHCIGeneralTD					initTD;
	USBRequest					initReq;
} OHCICommPage;

#endif
