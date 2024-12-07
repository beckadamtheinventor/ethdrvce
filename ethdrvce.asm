; Headers
;-------------------------------------------------------------------------------
include '../include/library.inc'
include '../include/include_library.inc'
;-------------------------------------------------------------------------------

; Name
;------------------------------------------
library ETHDRVCE, 0
;------------------------------------------

; Dependencies
;-------------------------------------------------------------------------------
include_library '../usbdrvce/usbdrvce.asm'
;-------------------------------------------------------------------------------

; V0 Functions
; -----------------------------------------
export eth_USBHandleHubs
export eth_Open
export eth_SetRecvdCallback
export eth_Write
export eth_Close



;------------------------------------------
; Helper Functions
;------------------------------------------

__indcallhl:
	jp	(hl)
 
 __indcall:
	jp	(iy)

_nibble:
	call	ti._frameset0
	ld	hl, (ix + 6)
	ld.sis	de, -48
	ex	de, hl
	ld	iyl, e
	ld	iyh, d
	ex	de, hl
	add.sis	iy, de
	ld.sis	de, 10
	ex	de, hl
	ld	e, iyl
	ld	d, iyh
	ex	de, hl
	or	a, a
	sbc.sis	hl, de
	jp	c, BB0_7
	ld.sis	de, -65
	ld	hl, (ix + 6)
	ex	de, hl
	ld	iyl, e
	ld	iyh, d
	ex	de, hl
	add.sis	iy, de
	ld.sis	de, 6
	ex	de, hl
	ld	e, iyl
	ld	d, iyh
	ex	de, hl
	or	a, a
	sbc.sis	hl, de
	jr	nc, BB0_3
	ld	a, iyl
	jp	BB0_5
BB0_3:
	ld.sis	bc, -97
	ld	hl, (ix + 6)
	add.sis	hl, bc
	push	hl
	pop	bc
	or	a, a
	sbc.sis	hl, de
	jr	nc, BB0_6
	ld	a, c
BB0_5:
	add	a, 10
	ld	iyl, a
	jr	BB0_7
BB0_6:
	ld	iyl, -1
BB0_7:
	ld	a, iyl
	pop	ix
	ret

;--------------------------------
; Generic Interrupt RX Callback
;--------------------------------
_interrupt_receive_callback:
	ld	hl, -6
	call	ti._frameset
	ld	hl, (ix + 6)
	push	hl
	call	usb_GetEndpointData
	ld	(ix - 3), hl
	pop	hl
	ld	hl, (ix + 9)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	nz, BB1_9
	ld	hl, (ix + 12)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, BB1_9
	ld	iy, 0
BB1_3:
	ld	(ix - 6), iy
	lea	bc, iy
	ld	hl, _eth_int_buf
	add	hl, bc
	push	hl
	pop	de
	ld	a, (hl)
	cp	a, -95
	jp	nz, BB1_8
	push	de
	pop	iy
	ld	a, (iy + 1)
	or	a, a
	jp	nz, BB1_8
	push	de
	pop	iy
	ld	hl, (iy + 2)
	add.sis	hl, bc
	or	a, a
	sbc.sis	hl, bc
	ld	l, 1
	jr	nz, BB1_7
	ld	l, 0
BB1_7:
	ld	iy, (ix - 3)
	ld	a, (iy + 62)
	and	a, -2
	add	a, l
	ld	(iy + 62), a
BB1_8:
	push	de
	pop	iy
	ld	hl, (iy + 6)
	ld	bc, 0
	ld	c, l
	ld	b, h
	ld	de, 8
	ld	iy, (ix - 6)
	add	iy, de
	add	iy, bc
	lea	hl, iy
	ld	bc, (ix + 12)
	or	a, a
	sbc	hl, bc
	jp	c, BB1_3
BB1_9:
	ld	iy, (ix - 3)
	ld	hl, (iy + 16)
	ld	de, (iy + 28)
	ld	bc, 0
	push	bc
	push	de
	ld	de, 64
	push	de
	ld	de, _eth_int_buf
	push	de
	push	hl
	call	usb_ScheduleTransfer
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	or	a, a
	sbc	hl, hl
	ld	sp, ix
	pop	ix
	ret

_eth_tx_callback:
	call	ti._frameset0
	ld	iy, (ix + 15)
	ld	de, 0
	lea	bc, iy
	ld	iy, (iy + 6)
	lea	hl, iy
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB2_4
	ld	hl, (ix + 9)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB2_3
	ld	de, 5
BB2_3:
	push	bc
	push	de
	call	__indcall
	pop	hl
	pop	hl
	ld	de, 0
BB2_4:
	ex	de, hl
	pop	ix
	ret

;----------------------------
; ECM Driver Functions
;----------------------------
_ecm_receive_callback:
	ld	hl, -79
	call	ti._frameset
	ld	hl, (ix + 6)
	lea	de, ix - 76
	ld	(ix - 79), de
	push	hl
	call	usb_GetEndpointData
	push	hl
	pop	iy
	pop	hl
	ld	(ix - 76), 0
	ld	hl, (ix - 79)
	push	hl
	pop	de
	inc	de
	ld	bc, 75
	ldir
	ld	hl, (ix + 9)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	nz, BB3_3
	ld	hl, (ix + 12)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB3_3
	ld	de, _eth_rx_buf
	ld	(ix - 76), de
	ld	(ix - 73), hl
	ld	hl, (iy + 22)
	ld	de, (ix - 79)
	push	de
	ld	de, 0
	push	de
	ld	(ix - 79), iy
	call	__indcallhl
	ld	iy, (ix - 79)
	pop	hl
	pop	hl
BB3_3:
	ld	hl, (iy + 13)
	ld	de, (iy + 19)
	ld	bc, 0
	push	bc
	push	de
	ld	de, 1518
	push	de
	ld	de, _eth_rx_buf
	push	de
	push	hl
	call	usb_ScheduleTransfer
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	or	a, a
	sbc	hl, hl
	ld	sp, ix
	pop	ix
	ret

_ecm_bulk_transmit:
	ld	hl, -3
	call	ti._frameset
	ld	iy, (ix + 9)
	ld	de, (iy + 3)
	ld	bc, 1519
	push	de
	pop	hl
	or	a, a
	sbc	hl, bc
	jr	nc, BB4_2
	ld	bc, (ix + 6)
	ld	(ix - 3), de
	lea	hl, iy
	push	bc
	pop	iy
	ld	de, (iy + 10)
	ld	bc, (hl)
	push	hl
	ld	hl, _eth_tx_callback
	push	hl
	ld	hl, (ix - 3)
	push	hl
	push	bc
	push	de
	call	usb_ScheduleTransfer
	ld	bc, 1519
	ld	de, (ix - 3)
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
BB4_2:
	ex	de, hl
	or	a, a
	sbc	hl, bc
	sbc	a, a
	pop	hl
	pop	ix
	ret


;----------------------------------
; NCM Driver Functions
;----------------------------------

_ncm_control_setup:
	ld	hl, -41
	call	ti._frameset
	ld	iy, (ix + 6)
	ld	bc, 0
	ld.sis	hl, 0
	lea	de, ix - 3
	ld	(ix - 38), de
	ld	de, 32929
	ld	(ix - 11), de
	ld	(ix - 8), bc
	ld.sis	de, 28
	ld	(ix - 5), e
	ld	(ix - 4), d
	ld	(ix - 19), 33
	ld	(ix - 18), -122
	ld	(ix - 17), l
	ld	(ix - 16), h
	ld	(ix - 15), l
	ld	(ix - 14), h
	bit	4, (iy + 61)
	jr	z, BB5_2
	ld.sis	de, 8
	jr	BB5_3
BB5_2:
	ld.sis	de, 4
BB5_3:
	ld	(ix - 13), e
	ld	(ix - 12), d
	ld	de, 82721
	ld	(ix - 27), de
	ld	(ix - 24), bc
	ld	(ix - 21), l
	ld	(ix - 20), h
	ld	de, 2048
	ld	(ix - 35), de
	ld	de, 1024
	ld	(ix - 32), de
	ld	(ix - 29), l
	ld	(ix - 28), h
	ld	hl, (iy)
	push	bc
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	de, (ix - 38)
	push	de
	ld	de, 3
	push	de
	ld	iy, (ix + 6)
	pea	iy + 31
	pea	ix - 11
	push	hl
	call	usb_ControlTransfer
	ld	(ix - 41), hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	ld	hl, (ix + 6)
	ld	hl, (hl)
	ld	de, 0
	push	de
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	de, (ix - 38)
	push	de
	ld	de, 3
	push	de
	pea	ix - 35
	pea	ix - 19
	push	hl
	call	usb_ControlTransfer
	ld	iy, (ix + 6)
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	ld	bc, (ix - 41)
	call	ti._ior
	ld	a, (iy + 61)
	and	a, 1
	bit	0, a
	jr	z, BB5_5
	ld	(ix - 41), hl
	ld	hl, (iy)
	ld	de, 0
	push	de
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	de, (ix - 38)
	push	de
	ld	de, 3
	push	de
	ld	de, 0
	push	de
	pea	ix - 27
	push	hl
	call	usb_ControlTransfer
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	ld	bc, (ix - 41)
	call	ti._ior
BB5_5:
	ld	sp, ix
	pop	ix
	ret

_ncm_receive_callback:
	ld	hl, -94
	call	ti._frameset
	ld	hl, (ix + 6)
	lea	de, ix - 76
	ld	(ix - 79), de
	push	hl
	call	usb_GetEndpointData
	ld	(ix - 82), hl
	pop	hl
	ld	iy, (ix - 79)
	lea	hl, iy + 6
	ld	(ix - 70), 0
	push	hl
	pop	de
	inc	de
	ld	bc, 69
	ldir
	ld	a, (_eth_rx_buf+3)
	ld	hl, (ix + 9)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	nz, BB6_10
	ld	hl, (ix + 12)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, BB6_10
	ld	bc, 5063502
	ld	d, 72
	ld	hl, (_eth_rx_buf)
	ld	e, a
	ld	a, d
	call	ti._lcmpu
	jp	nz, BB6_10
	ld	bc, _eth_rx_buf
	ld	hl, _eth_rx_buf+10
	ld	a, 48
	ld	hl, (hl)
	ld	de, 0
	ld	e, l
	ld	d, h
BB6_4:
	push	bc
	pop	iy
	add	iy, de
	ld	hl, (iy)
	ld	e, (iy + 3)
	ld	bc, 5063502
	call	ti._lcmpu
	jp	nz, BB6_10
	lea	hl, iy + 8
	ld	(ix - 88), hl
	ld	hl, (iy + 8)
	ld	de, 0
	push	de
	pop	bc
	ld	c, l
	ld	b, h
	ld	(ix - 91), iy
	ld	hl, (iy + 10)
	ld	e, l
	ld	d, h
	ld.sis	hl, 1
	ld	(ix - 85), hl
BB6_6:
	ld	hl, _eth_rx_buf
	add	hl, bc
	ld	(ix - 76), hl
	ld	(ix - 73), de
	ld	iy, (ix - 82)
	ld	hl, (iy + 22)
	ld	de, (ix - 79)
	push	de
	ld	de, 0
	push	de
	call	__indcallhl
	pop	hl
	pop	hl
	ld	de, 0
	ld	hl, (ix - 85)
	ld	e, l
	ld	d, h
	push	de
	pop	hl
	ld	c, 2
	call	ti._ishl
	push	hl
	pop	bc
	ld	hl, (ix - 88)
	add	hl, bc
	ld	hl, (hl)
	add.sis	hl, bc
	or	a, a
	sbc.sis	hl, bc
	jp	z, BB6_8
	ld	bc, 0
	ld	c, l
	ld	b, h
	ld	(ix - 94), bc
	ex	de, hl
	ld	c, 2
	call	ti._ishl
	push	hl
	pop	de
	ld	iy, (ix - 88)
	add	iy, de
	ld	hl, (iy + 2)
	ld	de, 0
	ld	e, l
	ld	d, h
	ld	bc, (ix - 85)
	inc.sis	bc
	ld	(ix - 85), bc
	ld	bc, (ix - 94)
	add.sis	hl, bc
	or	a, a
	sbc.sis	hl, bc
	jp	nz, BB6_6
BB6_8:
	ld	iy, (ix - 91)
	ld	hl, (iy + 6)
	ld	de, 0
	ld	e, l
	ld	d, h
	add.sis	hl, bc
	or	a, a
	sbc.sis	hl, bc
	ld	bc, _eth_rx_buf
	ld	a, 48
	jp	nz, BB6_4
	ld	iy, (ix - 82)
	ld	hl, (iy + 13)
	ld	(ix - 79), hl
	ld	de, (iy + 19)
	ld	hl, 0
	push	hl
	push	de
	ld	de, 2048
	push	de
	push	bc
	ld	hl, (ix - 79)
	push	hl
	call	usb_ScheduleTransfer
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
BB6_10:
	or	a, a
	sbc	hl, hl
	ld	sp, ix
	pop	ix
	ret
 
 _ncm_bulk_transmit:
	ld	hl, -6
	call	ti._frameset
	ld	hl, (ix + 6)
	xor	a, a
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, BB7_4
	ld	hl, (ix + 9)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, BB7_4
	ld	hl, (ix + 9)
	push	hl
	pop	iy
	lea	hl, iy + 12
	ld	(ix - 3), hl
	ld	(iy + 12), 0
	push	hl
	pop	iy
	inc	iy
	ld	bc, 63
	lea	de, iy
	ldir
	ld	iy, (ix + 6)
	ld	bc, (iy + 43)
	ld	iyl, c
	ld	iyh, b
	ld.sis	de, 12
	add.sis	iy, de
	ld	l, e
	ld	h, d
	call	ti._sremu
	ld	e, l
	ld	d, h
	ex	de, hl
	ld	e, iyl
	ld	d, iyh
	ex	de, hl
	or	a, a
	sbc.sis	hl, de
	ld	(ix - 6), hl
	ld	hl, 5063502
	ld	iy, (ix + 9)
	ld	(iy + 12), hl
	ld	(iy + 15), 72
	ld.sis	hl, 12
	ld	(iy + 16), l
	ld	(iy + 17), h
	lea	hl, iy
	ld	iy, (ix + 6)
	ld	de, (iy + 59)
	ld	c, e
	ld	b, d
	inc.sis	bc
	ld	iy, (ix + 6)
	ld	(iy + 59), c
	ld	(iy + 60), b
	push	hl
	pop	iy
	ld	(iy + 18), e
	ld	(iy + 19), d
	ld	de, (iy + 3)
	ld	l, e
	ld	h, d
	ld.sis	bc, 64
	add.sis	hl, bc
	ld	(iy + 20), l
	ld	(iy + 21), h
	ld	hl, (ix - 6)
	ld	(iy + 22), l
	ld	(iy + 23), h
	ld	bc, 0
	ld	c, l
	ld	b, h
	ld	hl, (ix - 3)
	push	hl
	pop	iy
	add	iy, bc
	ld	bc, 5063502
	ld	(iy), bc
	ld	(iy + 3), 48
	ld.sis	bc, 16
	ld	(iy + 4), c
	ld	(iy + 5), b
	ld.sis	bc, 0
	ld	(iy + 6), c
	ld	(iy + 7), b
	ld.sis	bc, 64
	ld	(iy + 8), c
	ld	(iy + 9), b
	ld	(iy + 10), e
	ld	(iy + 11), d
	ld.sis	de, 0
	ld	(iy + 12), e
	ld	(iy + 13), d
	ld	(iy + 14), e
	ld	(iy + 15), d
	ld	iy, (ix + 6)
	ld	bc, (iy + 10)
	ld	de, 0
	push	de
	push	de
	ld	de, 64
	push	de
	push	hl
	push	bc
	call	usb_ScheduleTransfer
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	ld	iy, (ix + 6)
	ld	hl, (iy + 10)
	ld	(ix - 3), hl
	ld	iy, (ix + 9)
	ld	de, (iy)
	ld	bc, (iy + 3)
	push	iy
	ld	hl, _eth_tx_callback
	push	hl
	push	bc
	push	de
	ld	hl, (ix - 3)
	push	hl
	call	usb_ScheduleTransfer
	ld	de, (ix + 6)
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	ld	iy, (ix + 9)
	ld	a, (iy + 3)
	and	a, 63
	or	a, a
	ld	a, 0
	jr	nz, BB7_4
	push	de
	pop	iy
	ld	bc, (iy + 10)
	ld	hl, (ix + 9)
	ld	de, (hl)
	ld	hl, 0
	push	hl
	push	hl
	push	hl
	push	de
	push	bc
	call	usb_ScheduleTransfer
	xor	a, a
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
BB7_4:
	ld	sp, ix
	pop	ix
	ret
 
 
 eth_USBHandleHubs:
	ld	hl, -274
	call	ti._frameset
	ld	hl, (ix + 6)
	push	hl
	call	usb_GetDeviceFlags
	pop	de
	ld	a, l
	bit	3, a
	ld	a, 0
	jp	z, BB8_5
	ld	bc, -265
	lea	iy, ix
	add	iy, bc
	ld	bc, -271
	lea	hl, ix
	add	hl, bc
	ld	(hl), iy
	lea	hl, iy + 3
	ld	bc, -268
	lea	iy, ix
	add	iy, bc
	ld	(iy), hl
	or	a, a
	sbc	hl, hl
	push	hl
	ld	hl, (ix + 6)
	push	hl
	call	usb_GetConfigurationDescriptorTotalLength
	push	hl
	pop	de
	ld	bc, -274
	lea	iy, ix
	add	iy, bc
	ld	(iy), de
	pop	hl
	pop	hl
	ld	bc, -271
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	push	de
	ld	bc, -268
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	or	a, a
	sbc	hl, hl
	push	hl
	ld	hl, 2
	push	hl
	ld	hl, (ix + 6)
	push	hl
	call	usb_GetDescriptor
	ld	de, -274
	lea	hl, ix
	add	hl, de
	ld	bc, (hl)
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	ld	de, -271
	lea	hl, ix
	add	hl, de
	ld	iy, (hl)
	ld	de, (iy)
	push	bc
	pop	hl
	or	a, a
	sbc	hl, de
	jr	z, BB8_3
	ld	a, 0
	jr	BB8_4
BB8_3:
	ld	a, -1
BB8_4:
	ld	(ix - 3), de
	ld	de, -271
	lea	hl, ix
	add	hl, de
	ld	(hl), a
	push	bc
	pop	hl
	ld	de, (ix - 3)
	or	a, a
	sbc	hl, de
	ld	hl, (ix + 6)
	push	bc
	ld	bc, -268
	lea	iy, ix
	push	af
	add	iy, bc
	pop	af
	ld	de, (iy)
	push	de
	push	hl
	call	z, usb_SetConfiguration
	pop	hl
	pop	hl
	pop	hl
	ld	bc, -271
	lea	hl, ix
	add	hl, bc
	ld	a, (hl)
BB8_5:
	ld	sp, ix
	pop	ix
	ret

eth_Open:
	ld	hl, -612
	call	ti._frameset
	ld	hl, (ix + 6)
	ld	(hl), 0
	push	hl
	pop	iy
	inc	iy
	ld	bc, 62
	lea	de, iy
	push	hl
	pop	iy
	ldir
	lea	hl, iy
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB9_3
	ld	de, (ix + 9)
	push	de
	pop	hl
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB9_3
	ld	hl, (ix + 12)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	nz, BB9_5
BB9_3:
	ld	hl, 1
BB9_4:
	ld	sp, ix
	pop	ix
	ret
BB9_5:
	ld	bc, -276
	lea	iy, ix
	add	iy, bc
	lea	hl, ix - 9
	push	ix
	ld	bc, -546
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	bc, -549
	lea	hl, ix
	add	hl, bc
	ld	(hl), iy
	lea	hl, iy + 11
	ld	bc, -543
	lea	iy, ix
	add	iy, bc
	ld	(iy), hl
	push	de
	call	usb_RefDevice
	pop	hl
	ld	bc, -546
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	ld	hl, 18
	push	hl
	ld	bc, -543
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	or	a, a
	sbc	hl, hl
	push	hl
	inc	hl
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_GetDescriptor
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	nz, BB9_7
	ld	hl, (ix - 9)
	ld	de, 18
	or	a, a
	sbc	hl, de
	jr	z, BB9_8
BB9_7:
	ld	hl, 2
	jp	BB9_4
BB9_8:
	ld	bc, -549
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	ld	a, (iy + 15)
	or	a, a
	jp	nz, BB9_69
	ld	bc, -540
	lea	hl, ix
	add	hl, bc
	push	hl
	pop	de
	ld	hl, _ecm_receive_callback
	push	ix
	ld	bc, -571
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	hl, _ncm_receive_callback
	push	ix
	ld	bc, -556
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	hl, _ecm_bulk_transmit
	push	ix
	ld	bc, -568
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	hl, _ncm_bulk_transmit
	push	ix
	ld	bc, -559
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	hl, 2048
	push	ix
	ld	bc, -583
	add	ix, bc
	ld	(ix), hl
	pop	ix
	lea	hl, iy
	push	ix
	ld	bc, -565
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	bc, -577
	lea	hl, ix
	add	hl, bc
	ld	(hl), de
	lea	hl, iy
	push	de
	pop	iy
	lea	de, iy + 8
	push	hl
	pop	iy
	ld	c, (iy + 28)
	ld	iy, (ix + 6)
	lea	hl, iy + 4
	ld	(ix - 3), bc
	ld	bc, -574
	lea	iy, ix
	add	iy, bc
	ld	(iy), hl
	push	de
	pop	iy
	ld	bc, -580
	lea	hl, ix
	add	hl, bc
	ld	(hl), iy
	lea	hl, iy + 4
	ld	bc, -586
	lea	iy, ix
	add	iy, bc
	ld	(iy), hl
	ld	a, 0
	ld	e, a
	push	ix
	ld	bc, -562
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	bc, (ix - 3)
	ld	iyl, c
	ld	bc, (ix + 9)
BB9_10:
	ld	a, e
	cp	a, iyl
	jp	z, BB9_7
	ld	(ix - 3), bc
	ld	bc, -553
	lea	hl, ix
	add	hl, bc
	push	af
	ld	a, iyl
	ld	(hl), a
	pop	af
	push	de
	ld	bc, (ix - 3)
	push	bc
	ld	bc, -552
	lea	iy, ix
	add	iy, bc
	ld	(iy), de
	call	usb_GetConfigurationDescriptorTotalLength
	push	hl
	pop	bc
	pop	hl
	pop	hl
	push	bc
	pop	hl
	ld	de, 257
	or	a, a
	sbc	hl, de
	jp	nc, BB9_66
	ld	de, -546
	lea	hl, ix
	add	hl, de
	ld	hl, (hl)
	push	hl
	push	bc
	ld	de, -543
	lea	hl, ix
	add	hl, de
	ld	hl, (hl)
	push	hl
	ld	de, -552
	lea	hl, ix
	add	hl, de
	ld	hl, (hl)
	push	hl
	ld	hl, 2
	push	hl
	ld	hl, (ix + 9)
	push	hl
	ld	de, -589
	lea	hl, ix
	add	hl, de
	ld	(hl), bc
	call	usb_GetDescriptor
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	nz, BB9_66
	ld	hl, (ix - 9)
	ld	bc, -589
	lea	iy, ix
	add	iy, bc
	ld	de, (iy)
	or	a, a
	sbc	hl, de
	ld	bc, -553
	lea	hl, ix
	push	af
	add	hl, bc
	pop	af
	push	af
	ld	a, (hl)
	ld	iyl, a
	pop	af
	jp	nz, BB9_67
	xor	a, a
	ld	bc, -597
	lea	hl, ix
	add	hl, bc
	ld	(hl), a
	push	ix
	ld	bc, -543
	add	ix, bc
	ld	hl, (ix)
	pop	ix
	push	ix
	ld	bc, -592
	add	ix, bc
	ld	(ix), hl
	pop	ix
	ld	de, 0
BB9_15:
	push	de
	pop	hl
	ld	(ix - 3), de
	push	ix
	ld	de, -589
	add	ix, de
	ld	bc, (ix)
	pop	ix
	or	a, a
	sbc	hl, bc
	ld	de, (ix - 3)
	jp	nc, BB9_67
	ld	b, a
	ld	(ix - 3), bc
	ld	bc, -595
	lea	hl, ix
	add	hl, bc
	ld	(hl), de
	ld	de, -592
	lea	iy, ix
	add	iy, de
	ld	iy, (iy)
	ld	a, (iy + 1)
	cp	a, 4
	ld	bc, (ix - 3)
	jp	nz, BB9_22
	ld	a, b
	bit	2, a
	jp	nz, BB9_26
	ld	bc, -592
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	ld	a, (iy + 5)
	cp	a, 2
	push	ix
	ld	bc, -595
	push	af
	add	ix, bc
	pop	af
	ld	hl, (ix)
	pop	ix
	jp	nz, BB9_33
	ld	a, (iy + 6)
	cp	a, 6
	jr	z, BB9_21
	cp	a, 13
	jp	nz, BB9_53
BB9_21:
	ld	iy, (ix + 6)
	ld	(iy + 3), a
	ld	bc, -589
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	ld	bc, -543
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_SetConfiguration
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	ld	a, 1
	ld	bc, -596
	lea	iy, ix
	push	af
	add	iy, bc
	pop	af
	ld	(iy), a
	push	ix
	inc	bc
	push	af
	add	ix, bc
	pop	af
	ld	iy, (ix)
	pop	ix
	jp	nz, BB9_7
	jp	BB9_63
BB9_22:
	cp	a, 5
	jp	nz, BB9_30
	ld	l, b
	bit	3, l
	jp	nz, BB9_35
	ld	a, l
	and	a, 1
	bit	0, a
	jp	nz, BB9_46
	ld	bc, -596
	lea	iy, ix
	add	iy, bc
	ld	(iy), l
	jp	BB9_58
BB9_26:
	ld	de, -592
	lea	hl, ix
	add	hl, de
	ld	iy, (hl)
	ld	a, (iy + 2)
	push	ix
	ld	de, -597
	add	ix, de
	ld	l, (ix)
	pop	ix
	cp	a, l
	push	ix
	ld	de, -595
	push	af
	add	ix, de
	pop	af
	ld	hl, (ix)
	pop	ix
	push	ix
	dec	de
	push	af
	add	ix, de
	pop	af
	ld	(ix), b
	pop	ix
	jp	nz, BB9_34
	ld	a, (iy + 4)
	cp	a, 2
	jp	nz, BB9_34
	ld	bc, -592
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	ld	a, (iy + 5)
	cp	a, 10
	jp	nz, BB9_58
	ld	bc, -589
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	ld	bc, -595
	lea	iy, ix
	add	iy, bc
	ld	iy, (iy)
	lea	de, iy
	or	a, a
	sbc	hl, de
	push	ix
	ld	bc, -602
	add	ix, bc
	ld	(ix), hl
	pop	ix
	push	ix
	ld	bc, -596
	add	ix, bc
	ld	a, (ix)
	pop	ix
	or	a, 8
	push	ix
	add	ix, bc
	ld	(ix), a
	pop	ix
	push	ix
	ld	bc, -592
	add	ix, bc
	ld	hl, (ix)
	pop	ix
	push	ix
	ld	bc, -605
	add	ix, bc
	ld	(ix), hl
	jp	BB9_62
BB9_30:
	cp	a, 36
	jp	nz, BB9_45
	ld	de, -592
	lea	hl, ix
	add	hl, de
	ld	iy, (hl)
	ld	a, (iy + 2)
	cp	a, 6
	jp	nz, BB9_47
	ld	de, -592
	lea	hl, ix
	add	hl, de
	ld	iy, (hl)
	ld	a, (iy + 4)
	ld	de, -597
	lea	iy, ix
	add	iy, de
	ld	(iy), a
	ld	a, b
	or	a, 4
	push	ix
	ld	bc, -596
	add	ix, bc
	ld	(ix), a
	pop	ix
	jp	BB9_61
BB9_33:
	xor	a, a
	ld	bc, -596
	lea	iy, ix
	add	iy, bc
	ld	(iy), a
BB9_34:
	push	hl
	pop	iy
	jp	BB9_63
BB9_35:
	ld	de, -592
	lea	hl, ix
	add	hl, de
	ld	iy, (hl)
	ld	c, (iy + 2)
	ld	a, c
	cp	a, 0
	call	pe, ti._setflag
	ld	l, -1
	jp	p, BB9_37
	ld	l, 0
BB9_37:
	bit	0, l
	ld	a, 64
	jr	nz, BB9_39
	ld	a, 32
BB9_39:
	or	a, b
	bit	0, l
	jr	nz, BB9_41
	ld	e, c
	ld	(ix - 3), bc
	ld	bc, -562
	lea	iy, ix
	add	iy, bc
	ld	(iy), de
	ld	bc, (ix - 3)
BB9_41:
	bit	0, l
	jr	nz, BB9_43
	ld	de, -598
	lea	hl, ix
	add	hl, de
	ld	c, (hl)
BB9_43:
	ld	de, -596
	lea	hl, ix
	add	hl, de
	ld	(hl), a
	and	a, 114
	cp	a, 114
	jp	z, BB9_70
	ld	de, -598
	lea	hl, ix
	add	hl, de
	ld	(hl), c
	jp	BB9_57
BB9_45:
	ld	de, -596
	lea	hl, ix
	add	hl, de
	ld	(hl), b
	jp	BB9_57
BB9_46:
	ld	bc, -592
	lea	iy, ix
	add	iy, bc
	ld	iy, (iy)
	ld	a, (iy + 2)
	ld	bc, -599
	lea	iy, ix
	add	iy, bc
	ld	(iy), a
	ld	a, l
	or	a, 16
	ld	bc, -596
	lea	hl, ix
	add	hl, bc
	ld	(hl), a
	jp	BB9_61
BB9_47:
	ld	de, -596
	lea	hl, ix
	add	hl, de
	ld	(hl), b
	cp	a, 15
	jp	nz, BB9_55
	ld	bc, -549
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	ld	hl, 33185
	ld	(iy + 3), hl
	or	a, a
	sbc	hl, hl
	ld	(iy + 6), hl
	ld.sis	hl, 6
	ld	(iy + 9), l
	ld	(iy + 10), h
	ld	bc, -592
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	ld	l, (iy + 3)
	ld	a, l
	or	a, a
	jp	z, BB9_59
	ld	bc, -565
	lea	iy, ix
	add	iy, bc
	ld	de, (iy)
	push	de
	ld	de, 256
	push	de
	push	ix
	ld	bc, -580
	add	ix, bc
	ld	de, (ix)
	pop	ix
	push	de
	ld	de, 0
	push	de
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_GetStringDescriptor
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	nz, BB9_59
	ld	de, -586
	lea	hl, ix
	add	hl, de
	ld	bc, (hl)
	or	a, a
	sbc	hl, hl
	ld	de, -608
	lea	iy, ix
	add	iy, de
	ld	(iy), hl
BB9_51:
	ld	de, -608
	lea	hl, ix
	add	hl, de
	ld	hl, (hl)
	ld	de, 6
	or	a, a
	sbc	hl, de
	jp	z, BB9_60
	push	bc
	pop	iy
	ld	bc, -611
	lea	hl, ix
	add	hl, bc
	ld	(hl), iy
	ld	hl, (iy - 2)
	push	hl
	call	_nibble
	pop	hl
	ld	b, 4
	call	ti._bshl
	ld	de, -612
	lea	hl, ix
	add	hl, de
	ld	(hl), a
	inc	de
	lea	iy, ix
	add	iy, de
	ld	hl, (iy)
	ld	hl, (hl)
	push	hl
	call	_nibble
	pop	hl
	ld	bc, -612
	lea	hl, ix
	add	hl, bc
	or	a, (hl)
	ld	bc, -574
	lea	iy, ix
	add	iy, bc
	ld	hl, (iy)
	push	ix
	ld	bc, -608
	add	ix, bc
	ld	de, (ix)
	pop	ix
	add	hl, de
	ld	(hl), a
	inc	de
	lea	hl, ix
	add	hl, bc
	ld	(hl), de
	push	ix
	ld	de, -611
	add	ix, de
	ld	iy, (ix)
	pop	ix
	lea	iy, iy + 4
	lea	bc, iy
	jp	BB9_51
BB9_53:
	xor	a, a
BB9_54:
	ld	bc, -596
	lea	hl, ix
	add	hl, bc
	ld	(hl), a
	jr	BB9_57
BB9_55:
	cp	a, 26
	jr	nz, BB9_58
	ld	bc, -592
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	ld	a, (iy + 5)
	ld	iy, (ix + 6)
	ld	(iy + 61), a
BB9_57:
	ld	bc, -595
	lea	iy, ix
	add	iy, bc
	ld	iy, (iy)
	jr	BB9_63
BB9_58:
	ld	bc, -595
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	jr	BB9_63
BB9_59:
	or	a, a
	sbc	hl, hl
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	bc, -565
	lea	iy, ix
	add	iy, bc
	ld	de, (iy)
	push	de
	ld	de, 3
	push	de
	push	ix
	ld	bc, -574
	add	ix, bc
	ld	de, (ix)
	pop	ix
	push	de
	push	ix
	ld	bc, -549
	add	ix, bc
	ld	iy, (ix)
	pop	ix
	pea	iy + 3
	push	hl
	call	usb_ControlTransfer
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	nz, BB9_64
BB9_60:
	ld	bc, -596
	lea	hl, ix
	add	hl, bc
	ld	a, (hl)
	or	a, 2
	lea	iy, ix
	add	iy, bc
	ld	(iy), a
BB9_61:
	push	ix
	ld	bc, -595
	add	ix, bc
	ld	iy, (ix)
BB9_62:
	pop	ix
BB9_63:
	ld	de, 0
	ld	bc, -592
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	ld	e, (hl)
	add	iy, de
	add	hl, de
	push	ix
	add	ix, bc
	ld	(ix), hl
	pop	ix
	lea	de, iy
	ld	bc, -553
	lea	hl, ix
	add	hl, bc
	push	af
	ld	a, (hl)
	ld	iyl, a
	pop	af
	push	ix
	ld	bc, -596
	add	ix, bc
	ld	a, (ix)
	pop	ix
	jp	BB9_15
BB9_64:
	ld	bc, -577
	lea	hl, ix
	add	hl, bc
	ld	iy, (hl)
	ld	hl, 33313
	ld	(iy), hl
	or	a, a
	sbc	hl, hl
	ld	(iy + 3), hl
	ld.sis	hl, 6
	ld	(iy + 6), l
	ld	(iy + 7), h
	call	_random
	ld	bc, -608
	lea	iy, ix
	add	iy, bc
	ld	(iy), hl
	call	_random
	ld	iy, (ix + 6)
	push	ix
	ld	bc, -608
	add	ix, bc
	ld	de, (ix)
	pop	ix
	ld	(iy + 4), de
	ld	(iy + 7), hl
	ld	a, e
	and	a, -4
	add	a, 2
	ld	(iy + 4), a
	or	a, a
	sbc	hl, hl
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	bc, -565
	lea	iy, ix
	add	iy, bc
	ld	de, (iy)
	push	de
	ld	de, 3
	push	de
	push	ix
	ld	bc, -574
	add	ix, bc
	ld	de, (ix)
	pop	ix
	push	de
	push	ix
	ld	bc, -577
	add	ix, bc
	ld	de, (ix)
	pop	ix
	push	de
	push	hl
	call	usb_ControlTransfer
	pop	de
	pop	de
	pop	de
	pop	de
	pop	de
	ld	bc, -596
	lea	iy, ix
	add	iy, bc
	ld	a, (iy)
	or	a, 2
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, BB9_54
	ld	bc, -596
	lea	hl, ix
	add	hl, bc
	ld	a, (hl)
	jp	BB9_54
BB9_66:
	ld	bc, (ix + 9)
	ld	de, -553
	lea	hl, ix
	add	hl, de
	push	af
	ld	a, (hl)
	ld	iyl, a
	pop	af
	ld	(ix - 3), bc
	push	ix
	ld	bc, -552
	add	ix, bc
	ld	de, (ix)
	pop	ix
	jr	BB9_68
BB9_67:
	ld	bc, (ix + 9)
	ld	(ix - 3), bc
	ld	bc, -552
	lea	hl, ix
	add	hl, bc
	ld	de, (hl)
BB9_68:
	ld	bc, (ix - 3)
	inc	e
	jp	BB9_10
BB9_69:
	ld	hl, 4
	jp	BB9_4
BB9_70:
	ld	de, -543
	lea	hl, ix
	add	hl, de
	ld	(hl), c
	ld	hl, (ix + 6)
	push	hl
	pop	iy
	ld	hl, (ix + 9)
	ld	(iy), hl
	ld	a, (iy + 3)
	cp	a, 13
	jr	nz, BB9_73
	ld	hl, (ix + 6)
	push	hl
	call	_ncm_control_setup
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	nz, BB9_7
	ld	iy, (ix + 6)
	ld	a, (iy + 3)
BB9_73:
	cp	a, 13
	ld	e, -1
	ld	c, 0
	ld	l, e
	jr	z, BB9_75
	ld	l, c
BB9_75:
	cp	a, 6
	jr	z, BB9_77
	ld	e, c
BB9_77:
	bit	0, e
	jr	nz, BB9_79
	ld	bc, 0
	ld	(ix - 3), de
	ld	de, -571
	lea	iy, ix
	add	iy, de
	ld	(iy), bc
	ld	de, (ix - 3)
BB9_79:
	bit	0, l
	jr	nz, BB9_81
	ld	(ix - 3), de
	ld	de, -571
	lea	iy, ix
	add	iy, de
	ld	bc, (iy)
	push	ix
	ld	de, -556
	add	ix, de
	ld	(ix), bc
	pop	ix
	ld	de, (ix - 3)
BB9_81:
	ld	iy, (ix + 6)
	ld	(ix - 3), de
	push	ix
	ld	de, -556
	add	ix, de
	ld	bc, (ix)
	pop	ix
	ld	(iy + 19), bc
	ld	de, (ix - 3)
	bit	0, e
	jr	nz, BB9_83
	ld	de, 0
	ld	bc, -568
	lea	iy, ix
	add	iy, bc
	ld	(iy), de
BB9_83:
	bit	0, l
	jr	nz, BB9_85
	ld	bc, -568
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	ld	bc, -559
	lea	iy, ix
	add	iy, bc
	ld	(iy), hl
BB9_85:
	ld	hl, (ix + 6)
	push	hl
	pop	iy
	ld	bc, -559
	lea	hl, ix
	add	hl, bc
	ld	de, (hl)
	ld	(iy + 25), de
	ld	hl, _interrupt_receive_callback
	ld	(iy + 28), hl
	ld	hl, (ix + 12)
	ld	(iy + 22), hl
	ex	de, hl
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, BB9_7
	ld	bc, -556
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, BB9_7
	ld	bc, -602
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	ld	bc, -605
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_SetInterface
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	ld	hl, 2
	jp	nz, BB9_4
	ld	bc, -562
	lea	hl, ix
	add	hl, bc
	ld	hl, (hl)
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	de, (ix + 6)
	push	de
	pop	iy
	ld	(iy + 13), hl
	ld	bc, -543
	lea	hl, ix
	add	hl, bc
	ld	l, (hl)
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	iy, (ix + 6)
	ld	(iy + 10), hl
	ld	bc, -599
	lea	hl, ix
	add	hl, bc
	ld	l, (hl)
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_GetDeviceEndpoint
	pop	de
	pop	de
	ld	iy, (ix + 6)
	ld	(iy + 16), hl
	ld	a, (iy + 3)
	cp	a, 13
	jr	nz, BB9_90
	ld	iy, (ix + 6)
	ld	hl, (iy + 10)
	ld	de, 0
	push	de
	push	hl
	call	usb_SetEndpointFlags
	pop	hl
	pop	hl
BB9_90:
	ld	hl, (ix + 6)
	push	hl
	pop	iy
	ld	hl, (iy + 13)
	push	iy
	push	hl
	call	usb_SetEndpointData
	pop	hl
	pop	hl
	ld	iy, (ix + 6)
	ld	hl, (iy + 10)
	push	iy
	push	hl
	call	usb_SetEndpointData
	pop	hl
	pop	hl
	ld	iy, (ix + 6)
	ld	hl, (iy + 16)
	push	iy
	push	hl
	call	usb_SetEndpointData
	pop	hl
	pop	hl
	ld	hl, (ix + 6)
	push	hl
	ld	hl, (ix + 9)
	push	hl
	call	usb_SetDeviceData
	pop	hl
	pop	hl
	ld	iy, (ix + 6)
	ld	hl, (iy + 16)
	ld	de, (iy + 28)
	ld	bc, 0
	push	bc
	push	de
	ld	de, 64
	push	de
	ld	de, _eth_int_buf
	push	de
	push	hl
	call	usb_ScheduleTransfer
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	ld	iy, (ix + 6)
	ld	hl, (iy + 13)
	ld	a, (iy + 3)
	cp	a, 13
	jr	z, BB9_92
	ld	de, 1518
	ld	bc, -583
	lea	iy, ix
	add	iy, bc
	ld	(iy), de
BB9_92:
	ld	iy, (ix + 6)
	ld	de, (iy + 19)
	ld	bc, 0
	push	bc
	push	de
	ld	bc, -583
	lea	iy, ix
	add	iy, bc
	ld	de, (iy)
	push	de
	ld	de, _eth_rx_buf
	push	de
	push	hl
	call	usb_ScheduleTransfer
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	or	a, a
	sbc	hl, hl
	jp	BB9_4

eth_SetRecvdCallback:
	call	ti._frameset0
	ld	hl, (ix + 9)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB10_2
	ld	iy, (ix + 6)
	ld	(iy + 22), hl
BB10_2:
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	nz, BB10_4
	ld	a, 0
	jr	BB10_5
BB10_4:
	ld	a, -1
BB10_5:
	pop	ix
	ret

eth_Write:
	call	__frameset0
	ld	iy, (ix + 6)
	ld	de, 1
	lea	hl, iy
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB11_6
	ld	bc, (ix + 9)
	push	bc
	pop	hl
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, BB11_6
	ld	hl, (iy + 25)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	nz, BB11_4
	ld	de, 2
	jr	BB11_6
BB11_4:
	push	bc
	push	iy
	call	__indcallhl
	pop	hl
	pop	hl
	bit	0, a
	ld	de, 0
	jr	nz, BB11_6
	ld	de, 5
BB11_6:
	ex	de, hl
	pop	ix
	ret

eth_Close:
	call	ti._frameset0
	ld	hl, (ix + 6)
	ld	hl, (hl)
	push	hl
	call	usb_ResetDevice
	pop	hl
	ld	hl, (ix + 6)
	ld	hl, (hl)
	push	hl
	call	_usb_UnrefDevice
	pop	hl
	ld	iy, (ix + 6)
	ld	(iy), 0
	lea	hl, iy
	inc	hl
	ld	bc, 62
	ex	de, hl
	lea	hl, iy
	ldir
	or	a, a
	sbc	hl, hl
	pop	ix
	ret


_eth_int_buf:
	rb	64

_eth_rx_buf:
	rb	2048


_random:
  push bc
  call .randoma
  ld l,a
  push hl
  dec sp
  pop af
  inc sp
  call .randoma
  ld l,a
  call .randoma
  ld h,a
  call .randoma
  ld e,a
  pop bc
  ret

; (r*9)^(r>>1)
.randoma:
  ld a,r
  ld c,a
  add a,a ; x2
  add a,a ; x4
  add a,a
  add a,c
  ld c,a
  ld a,r
  or a,a
  rrca
  xor a,c
  ret
