;	Glidix Loader
;
;	Copyright (c) 2014-2016, Madd Games.
;	All rights reserved.
;	
;	Redistribution and use in source and binary forms, with or without
;	modification, are permitted provided that the following conditions are met:
;	
;	* Redistributions of source code must retain the above copyright notice, this
;	  list of conditions and the following disclaimer.
;	
;	* Redistributions in binary form must reproduce the above copyright notice,
;	  this list of conditions and the following disclaimer in the documentation
;	  and/or other materials provided with the distribution.
;	
;	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
;	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

; gxfs.asm
; GXFS manipulation functions.

; Function read_sectors
; Reads sectors, given the starting LBA and sector count, and the
; buffer into which to read the sectors.
; Input:
;	EAX = starting LBA (relative to partition start)
;	CL = number of sectors to read
;	ES:DI = pointer to buffer
; Output:
;	None
; Trashes:
;	None
read_sectors:
	pusha
	push cx
	
	; first calculate (heads_per_cylinder * sectors_per_track)
	mov ecx, eax			; ECX = LBA
	xor eax, eax
	xor ebx, ebx
	mov al, [num_heads]
	mov bl, [num_sectors]
	mul ebx
	xor edx, edx
	mov ebx, eax			; EBX = heads_per_cylinder * sectors_per_track
	mov eax, ecx
	div ebx
	
	; now EAX = cylinder, EDX = "temp"
	; put the cylinder in ECX.
	mov ecx, eax
	mov eax, edx
	xor ebx, ebx
	mov bl, [num_sectors]
	div ebx
	
	; now:
	;	ECX = cylinder
	;	EAX = head
	;	EDX = sector
	inc edx
	xchg bx, bx
	mov bl, dl
	mov edx, ecx

	; we now have:
	; AL = head
	; BL = sector
	; DX = cylinder

	; in CX, again, we need the top 2 bits (8-9) of the cylinder, then 6 bits of
	; the sector.
	pop bp		; save sector count in bp
	mov ch, dl
	mov cl, dh
	shl cl, 6
	or cl, bl
	mov dh, al		; DH = head
	mov ax, bp
	mov ah, 0x02		; read sectors
	mov dl,	[boot_drive]

	; buffer (ES:BX)
	mov bx, di

	; read the sectors
	int 0x13
	popa
	ret
	
; Function read_disk
; Read a linear offset from the boot partition into a buffer.
; Input:
;	EAX = linear offset to data on partition
;	ECX = number of bytes to load
;	ES:DI = pointer to buffer
; Output:
;	None
; Trashes:
;	None
read_disk:
	pusha
	
	; store the end block in EDX
	mov edx, eax
	add edx, ecx
	shr edx, 9
	inc edx
	
	; set EBX to the offsetIntoBlock, and EAX to the start LBA
	mov ebx, eax
	shr eax, 9
	and ebx, 0x1FF
	
	; preserve the byte count, and calculate the number of sectors
	; to read.
	push ecx
	mov ecx, edx
	sub ecx, eax
	
	; read the sectors into the standard buffer at 0x60:0
	push es
	push di
	mov di, 0x60
	mov es, di
	xor di, di
	xor edx, edx
	mov dx, [part_start_lba]
	add eax, edx
	call read_sectors
	pop di
	pop es
	pop ecx
	
	; now we have:
	; EBX = offset into block
	; ECX = byte count to read
	; ES:DI = pointer to output buffer
	; copy from the block buffer to output buffer
	push ds
	mov ax, 0x60
	mov ds, ax
	mov si, bx
	rep movsb
	pop ds
	
	popa
	ret

; Function read_inode
; Reads an inode structure into memory from disk.
; Input:
;	EAX = inode number (starting from 1)
; Output:
;	Inode info at [inoBufferLoc].
; Trashes:
;	None
read_inode:
	pusha
	
	; find the section index and relative inode index
	dec eax
	xor edx, edx
	mov ebx, [num_sections]
	div ebx
	
	; get the position of this section's inode table
	mov si, 0x2410		; position of section table + 16 (offset to sdOffTabIno)
	shl ax, 32
	add si, ax
	mov ebx, [si]		; EBX = disk offset to inode table
	
	mov eax, edx
	xor edx, edx
	mov ecx, 359
	mul ecx
	add ebx, eax
	mov eax, ebx		; now EAX = offset to the inode we are looking for
	mov bx, ds		; and ECX is still 359
	mov es, bx
	mov di, [inoBufferLoc]
	call read_disk
	
	popa
	ret
