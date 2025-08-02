.global speck_encrypt

; r0-r15 - rezervovano na sifrovani - ukladame na zasobnik

#define cnt r18
#define tmp r19
#define zero r20
#define xl r26
#define xh r27
#define zl r30
#define zh r31
#define data_ptr_lo r24
#define data_ptr_hi r25

speck_encrypt: ; data ptr r25, r24 -> x
	; nejprve ulozime r0-r15 a nacteme do nich blok dat
	mov xh, data_ptr_hi
	mov xl, data_ptr_lo
	clr zl
	clr zh
save_regz_and_load_data: ; do registru r0-r7 nacteme data k sifrovani a ulozime na stack r8-r15
	ld	tmp, z  ; nacteme registr
	push tmp    ; ulozime hodnoty registru r0-r15
	sbrs zl, 3  ; 8-15 nenacitame z pameti
	ld  tmp, x+ ; data k sifrovani, x+ neprovedeme pro r8-r15
	st  z+, tmp ; ulozime data do registru - r8-r15 jsou libobolne
	cpi zl, 16  ; mam hotovo?
	brne save_regz_and_load_data
	; mame ulozeny registry r0-r7 na zasobniku a v registrech jsou data k sifrovani

	; nastavime round key -> Z
	ldi zl, lo8(round_keys)
	ldi zh, hi8(round_keys)

	;;clr currentRound;   the initial value is 0
	ldi cnt, 27;
	clr zero;           the initial value is 0
loop:
	; load k: [r15, r14, r13, r12], r12 is the lowest byte
	lpm r12, z+;
	lpm r13, z+;
	lpm r14, z+;
	lpm r15, z+;
	; x = S(8)( S(-8)(x) + y)
	add r5, r0; x1 = x1 + y0
	adc r6, r1; x2 = x2 + y1
	adc r7, r2; x3 = x3 + y2
	adc r4, r3; x0 = x0 + y3;
	; k = ( S(-8)(x) + y ) eor k
	eor r12, r5;
	eor r13, r6;
	eor r14, r7;
	eor r15, r4;
	; y = s(3)y
	lsl r0; loop 1
	rol r1;
	rol r2;
	rol r3;
	adc r0, zero;
	lsl r0; loop 2
	rol r1;
	rol r2;
	rol r3;
	adc r0, zero;
	lsl r0; loop 3
	rol r1;
	rol r2;
	rol r3;
	adc r0, zero;
	; y = S(3)(y) eor ( S(-8)(x) + y ) eor k
	eor r0, r12;
	eor r1, r13;
	eor r2, r14;
	eor r3, r15;
	; x = ( S(-8)(x) + y ) eor k
	movw r4, r12; r5:r4 = r13:r12
	movw r6, r14; r7:r6 = r15:r14
	; finished?
	;;inc currentRound;
	dec cnt
	;;cp currentRound, totalRound;
brne loop
	; v r0-r7 jsou data
	mov xh, data_ptr_hi
	mov xl, data_ptr_lo
	adiw xl, 8  ; budeme zapisovat do bufferu odzadu

	clr zh      ; pracujeme jen z registry - adresy 0:0 - 0:15
	; tady je trik - musime obnovit registry r15-r0 zpetne a soucasne
	; od registrem r7 zapisovat zpet do bufferu
	; k rozodnuti o zapisu vyuzivame 4. bit
	ldi zl, 16
load_regz_and_save_data:
	ld	tmp, -z ; nacteme registr r15-r0 - skutecne nas zajimaji jen r7-r0, ostatni hodnoty zahazujeme
	sbrs zl, 3  ; pokud je 4. bit nastaven - nezapisujeme do bufferu - jen obnova registru
	st  -x, tmp ; ulozime sifrovana data zpet do bufferu
	pop tmp		; puvodni hodnota registru
	st  z, tmp  ; ulozime do registru
	tst zl		; uz mame hotovo?
	brne load_regz_and_save_data

	ret

round_keys:
	.byte 0x45,0x53,0x50,0x43
	.byte 0xbe,0x7,0x17,0x94
	.byte 0x1b,0xa,0xdb,0x77
	.byte 0x61,0xcf,0xb1,0xb2
	.byte 0x96,0x49,0x7,0x37
	.byte 0x80,0xff,0x29,0x49
	.byte 0xb5,0x74,0x83,0xad
	.byte 0xc3,0x2d,0x6f,0xb3
	.byte 0x51,0x94,0x2a,0xf0
	.byte 0x5e,0xaa,0x5e,0xde
	.byte 0xa7,0xac,0x3c,0xd8
	.byte 0x9b,0xd3,0x7e,0x68
	.byte 0x49,0x1,0x5f,0xfa
	.byte 0xbc,0x96,0xf0,0x72
	.byte 0xb7,0xf5,0x2e,0x92
	.byte 0xe1,0x53,0xb9,0x14
	.byte 0x26,0x61,0x74,0xcd
	.byte 0x2,0x39,0x59,0x41
	.byte 0x3,0x30,0x8,0x65
	.byte 0x3a,0xaa,0x72,0xb1
	.byte 0xf4,0x3d,0x77,0x49
	.byte 0xa8,0x9e,0xa9,0x20
	.byte 0x40,0x74,0x20,0x47
	.byte 0xa5,0x26,0x88,0x68
	.byte 0x1a,0xa6,0x8b,0x2d
	.byte 0x57,0x1,0x80,0xb8
	.byte 0x4e,0xc1,0xe9,0x2d
