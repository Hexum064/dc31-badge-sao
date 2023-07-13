
/*
 * ws2812b.s
 *
 * Created: 2023-07-11 10:24:10
 *  Author: Branden
 */ 

 //A note to remember, XL = r26, XH = r27, YL = r28, YH = r29, ZL = r30, ZH = r31

.section .text
.global test_asm, output_pixels

output_pixels:
//r25,24 = port address
//r22 = pin
//r21,20 = array address
//r18 = array size
//r17 = shift counter
//r16 = current byte
	
	;r16 and 17 are the only registers that we will need outside of what was passed in as args, so save those
	push r16			
	push r17
//First load the address of the array into the indexing registers
	MOVW X, r20			;store the array address in X
	MOVW Y, r24			;store the PORT address in Y

output_byte:
	ldi r17, 8			;load 8 into r17 for the shift counter
	ld r16, X+			;load the address at X into r16 and inc the address
	
output_bit:
	std Y+5, r22		;store the PIN bm into the OUTSET reg
	nop					;delay time for the min on time. approx 400nS
	nop
	nop
	nop
	nop
	nop

	sbrs r16, 0			;skip the next line that sets the output low if the first bit is set
	std Y+6, r22		;set the output low using the address of the OUTCLR reg and the pin
	nop					;delay a bit more to either extend the on time to approx 800ns or pad the off time
	nop
	nop
	nop
	nop
	nop
	nop

	std Y+6, r22		;now set the output low 
	nop					;delay a bit longer for the min off time
	nop
	nop
	nop

	lsr r16				;shift r16 right to move to the next bit
	dec r17				;decrement the shift count
	brne output_bit		;if we are not at 0, then output the next bit

	dec r18				;decrement the array size (number of bytes)
	brne output_byte	;if we are not at 0 bytes, start to output the next byte	
	
	std Y+6, r22		;we are done so clear the output
	pop r17				;restore r16 and 17
	pop r16
ret

test_asm:
	push r16
	push r17

	ldi	r16, 0x02		;rgb led out pin = pin 1
	ldi r17, (24 * 3)
	
	
loop_start:	
	sts 0x0405, r16		;set the pin hi first
	nop
	nop
	nop
	nop
	nop
	nop
	sbrs r17, 0
	sts 0x0406, r16		;clear the pin after a period for high or low
	nop
	nop
	nop
	nop
	sbrc r17, 0
	sts 0x0406, r16		;clear the pin after a period for high or low
	nop
	nop
	nop


	dec r17
	brne loop_start

	pop r17
	pop r16
	ret
