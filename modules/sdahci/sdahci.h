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

#ifndef __glidix_sdahci_h
#define __glidix_sdahci_h

#include <glidix/util/common.h>
#include <glidix/hw/pci.h>
#include <glidix/hw/dma.h>
#include <glidix/storage/storage.h>

#define	AHCI_SIG_ATA	0x00000101
#define	AHCI_SIG_ATAPI	0xEB140101
#define	AHCI_SIG_SEMB	0xC33C0101
#define	AHCI_SIG_PM	0x96690101

#define ATA_CMD_READ_PIO				0x20
#define ATA_CMD_READ_PIO_EXT				0x24
#define ATA_CMD_READ_DMA				0xC8
#define ATA_CMD_READ_DMA_EXT				0x25
#define ATA_CMD_WRITE_PIO				0x30
#define ATA_CMD_WRITE_PIO_EXT				0x34
#define ATA_CMD_WRITE_DMA				0xCA
#define ATA_CMD_WRITE_DMA_EXT				0x35
#define ATA_CMD_CACHE_FLUSH				0xE7
#define ATA_CMD_CACHE_FLUSH_EXT				0xEA
#define ATA_CMD_PACKET					0xA0
#define ATA_CMD_IDENTIFY_PACKET				0xA1
#define ATA_CMD_IDENTIFY				0xEC

#define ATA_IDENT_DEVICETYPE				0
#define ATA_IDENT_CYLINDERS				2
#define ATA_IDENT_HEADS					6
#define ATA_IDENT_SECTORS				12
#define ATA_IDENT_SERIAL				20
#define ATA_IDENT_MODEL					54
#define ATA_IDENT_CAPABILITIES				98
#define ATA_IDENT_FIELDVALID				106
#define ATA_IDENT_MAX_LBA				120
#define ATA_IDENT_COMMANDSETS				164
#define ATA_IDENT_MAX_LBA_EXT				200

#define	ATAPI_CMD_READ					0xA8
#define	ATAPI_CMD_EJECT					0x1B
#define	ATAPI_CMD_READ_CAPACITY				0x25

#define	BOHC_BOS					(1 << 0)
#define	BOHC_OOS					(1 << 1)
#define	BOHC_SOOE					(1 << 2)
#define	BOHC_OOC					(1 << 3)
#define	BOHC_BB						(1 << 4)

#define	SSTS_DET_NONE					0x00
#define	SSTS_DET_NOPHY					0x01
#define	SSTS_DET_OK					0x03
#define	SSTS_DET_DISABLED				0x04

#define	SSTS_IPM_NONE					0x00
#define	SSTS_IPM_ACTIVE					0x01
#define	SSTS_IPM_PARTIAL				0x02
#define	SSTS_IPM_SLUMBER				0x06
#define	SSTS_IPM_DEVSLEEP				0x08

#define	CMD_ST						(1 << 0)
#define	CMD_FRE						(1 << 4)
#define	CMD_FR						(1 << 14)
#define	CMD_CR						(1 << 15)

#define	STS_BSY						(1 << 7)
#define	STS_DRQ						(1 << 3)
#define	STS_ERR						(1 << 0)

typedef enum
{
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

typedef struct
{
	uint64_t			clb;
	uint64_t			fb;
	uint32_t			is;
	uint32_t			ie;
	uint32_t			cmd;
	uint32_t			rsv0;
	uint32_t			tfd;
	uint32_t			sig;
	uint32_t			ssts;
	uint32_t			sctl;
	uint32_t			serr;
	uint32_t			sact;
	uint32_t			ci;
	uint32_t			sntf;
	uint32_t			fbs;
	uint32_t			rsv1[11];
	uint32_t			vendor[4];
} AHCIPort;

typedef struct
{
	uint32_t			cap;
	uint32_t			ghc;
	uint32_t			is;
	uint32_t			pi;
	uint32_t			vs;
	uint32_t			ccc_ctl;
	uint32_t			ccc_pts;
	uint32_t			em_loc;
	uint32_t			em_ctl;
	uint32_t			cap2;
	uint32_t			bohc;
	uint8_t				rsv[0xA0-0x2C];
	uint8_t				vendor[0x100-0xA0];
	AHCIPort			ports[32];
} AHCIMemoryRegs;

typedef struct
{
	uint8_t				cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
	uint8_t				a:1;		// ATAPI
	uint8_t				w:1;		// Write, 1: H2D, 0: D2H
	uint8_t				p:1;		// Prefetchable
 
	uint8_t				r:1;		// Reset
	uint8_t				b:1;		// BIST
	uint8_t				c:1;		// Clear busy upon R_OK
	uint8_t				rsv0:1;		// Reserved
	uint8_t				pmp:4;		// Port multiplier port
 
	uint16_t			prdtl;		// Physical region descriptor table length in entries
 
	// DW1
	volatile uint32_t		prdbc;		// Physical region descriptor uint8_t count transferred
 
	// DW2, 3
	uint64_t			ctba;		// Command table descriptor base address
 
	// DW4 - 7
	uint32_t			rsv1[4];	// Reserved
} AHCICommandHeader;

typedef struct AHCIController_
{
	struct AHCIController_*		next;
	PCIDevice*			pcidev;
	volatile AHCIMemoryRegs*	regs;
	struct ATADevice_*		ataDevices[32];
	int				numAtaDevices;
} AHCIController;

typedef struct ATADevice_	/* or ATAPI */
{
	AHCIController*			ctrl;
	int				portno;
	volatile AHCIPort*		port;
	StorageDevice*			sd;
	DMABuffer			dmabuf;
	Mutex				lock;
} ATADevice;

typedef struct tagHBA_PRDT_ENTRY
{
	uint64_t			dba;		// Data base address
	uint32_t			rsv0;		// Reserved
 
	// DW3
	uint32_t			dbc:22;		// byte count, 4M max
	uint32_t			rsv1:9;		// Reserved
	uint32_t			i:1;		// Interrupt on completion
} AHCI_PRDT;

typedef struct tagHBA_CMD_TBL
{
	// 0x00
	uint8_t				cfis[64];	// Command FIS
 
	// 0x40
	uint8_t				acmd[16];	// ATAPI command, 12 or 16 bytess
 
	// 0x50
	uint8_t				rsv[48];	// Reserved
 
	// 0x80
	AHCI_PRDT			prdt[9];
} AHCICommandTable;

typedef struct tagFIS_REG_H2D
{
	// DWORD 0
	uint8_t				fis_type;	// FIS_TYPE_REG_H2D
 
	uint8_t				pmport:4;	// Port multiplier
	uint8_t				rsv0:3;		// Reserved
	uint8_t				c:1;		// 1: Command, 0: Control
 
	uint8_t				command;	// Command register
	uint8_t				featurel;	// Feature register, 7:0
 
	// DWORD 1
	uint8_t				lba0;		// LBA low register, 7:0
	uint8_t				lba1;		// LBA mid register, 15:8
	uint8_t				lba2;		// LBA high register, 23:16
	uint8_t				device;		// Device register
 
	// DWORD 2
	uint8_t				lba3;		// LBA register, 31:24
	uint8_t				lba4;		// LBA register, 39:32
	uint8_t				lba5;		// LBA register, 47:40
	uint8_t				featureh;	// Feature register, 15:8
 
	// DWORD 3
	uint8_t				countl;		// Count register, 7:0
	uint8_t				counth;		// Count register, 15:8
	uint8_t				icc;		// Isochronous command completion
	uint8_t				control;	// Control register
 
	// DWORD 4
	uint8_t				rsv1[4];	// Reserved
} FIS_REG_H2D;

/**
 * "Operations area", all DMA structures are allocated within this.
 */
typedef struct
{
	/**
	 * Command list.
	 */
	AHCICommandHeader		cmdlist[32];
	
	/**
	 * FIS Area
	 * TODO: what even is the point of it?
	 */
	char				fisArea[256];
	
	/**
	 * Command table.
	 */
	AHCICommandTable		cmdtab;
	
	/**
	 * Identify area.
	 */
	char				id[1024];
} AHCIOpArea;

/**
 * Stop the commmand engine on a port.
 */
void ahciStopCmd(volatile AHCIPort *port);

/**
 * Start the command engine on a port.
 */
void ahciStartCmd(volatile AHCIPort *port);

#endif
