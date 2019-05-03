@ OSH-2019 Lab1
@ By Zitan Liu, PB17000232

.section .init
.global _start
_start:

@ "Load Construct", PC-relative or direct MOV when assembled
ldr r0, =0x3F200000

@ Physical addresses range from 0x3F000000 to 0x3FFFFFFF for peripherals. The
@ bus addresses for peripherals are set up to map onto the peripheral bus address range
@ starting at 0x7E000000. Thus a peripheral advertised here at bus address 0x7Ennnnnn is
@ available at physical address 0x3Fnnnnnn

@ Set as output
mov r1, #1
lsl r1, #27
str r1, [r0, #8]

loop:
@ Set GPSET0[28] (29th bit) up
@ ACT LED is at GPIO 29

mov r1, #1
lsl r1, #29
str r1, [r0, #28]

@ Delay for 2s
ldr r3, =1000000
bl delay_tmr

@ Set GPCLR0[28] up
mov r1, #1
lsl r1, #29
str r1, [r0, #40]

@ Delay for 1s
ldr r3, =1000000
bl delay_tmr

@ jump to main
b loop

@ Args passed in R3
@ Will modify R2

delay:
	ldr r1, =0xEE6B280
	do_1:
		 mov r2, r3
		 do_2:
			 subs r2, r2, #1
			 bne do_2

	    subs r1, r1, #1
	    bne do_1

	bx lr

@ The System Timer peripheral provides four 32-bit timer channels and a single 64-bit free running
@ counter. Each channel has an output compare register, which is compared against the 32 least
@ significant bits of the free running counter values. When the two values match, the system timer
@ peripheral generates a signal to indicate a match for the appropriate channel. The match signal is then
@ fed into the interrupt controller. The interrupt service routine then reads the output compare register
@ and adds the appropriate offset for the next timer tick. The free running counter is driven by the timer
@ clock and stopped whenever the processor is stopped in debug mode.
@ The Physical (hardware) base address for the system timers is 0x7E003000. 

@ Args passed in R3, millisec.
@ Will modify R2, R4

delay_tmr:
	ldr r2, =0x3F003000
	ldr r4, [r2, #4]
	add r4, r4, r3 @ Target Val
	delay_tmr_loop:
		ldr r5, [r2, #4]
		cmp r4, r5
		bcs delay_tmr_loop
	bx lr