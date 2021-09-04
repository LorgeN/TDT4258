.equ LED_ADDRESS, 0xFF200000
.equ UART_ADDRESS, 0xFF201000
.equ PALI_PATTERN, 0b0000011111
.equ NPALI_PATTERN, 0b1111100000

.global _start

.section .text

_start:
	BL check_input
	B check_palindrome

check_input:
	LENGTH .req R0
	LETTER_ADR .req R1
	LETTER .req R2

	LDR LETTER_ADR, =input
	LDR LENGTH, =0

check_input_internal:
	LDRB LETTER, [LETTER_ADR], #1
	CMP LETTER, #0
	
	BXEQ LR
	
	ADD LENGTH, #1
	B check_input_internal
	
check_palindrome:
	PUSH {R4, R5, R6, R7}
	INDEX_LOW .req R4
	INDEX_HIGH .req R5
	LETTER_LOW .req R6
	LETTER_HIGH .req R7

	LDR INDEX_LOW, =input
	ADD INDEX_HIGH, INDEX_LOW, LENGTH

restart_check:
	CMP INDEX_LOW, INDEX_HIGH
	POPGE {R4, R5, R6, R7}
	BGE palindrome_found

	LDRB LETTER_LOW, [INDEX_LOW]
	CMP LETTER_LOW, #32

	ADDLE INDEX_LOW, #1
	BLE restart_check

	LDRB LETTER_HIGH, [INDEX_HIGH]
	CMP LETTER_HIGH, #32

	SUB INDEX_HIGH, #1

	BLE restart_check

	ADD INDEX_LOW, #1

	CMP LETTER_LOW, #97
	ADDLT LETTER_LOW, #32

	CMP LETTER_HIGH, #97
	ADDLT LETTER_HIGH, #32

	CMP LETTER_LOW, LETTER_HIGH
	POPNE {R4, R5, R6, R7}
	BNE palindrome_not_found

	B restart_check

write_string:
	UART .req R0

	LDR UART, =UART_ADDRESS

write_string_internal:
	LDRB LETTER, [LETTER_ADR], #1
	CMP LETTER, #0
	BXEQ LR 

	STR LETTER, [UART]
	B write_string_internal

display_led:
	LDR R0, =LED_ADDRESS
	STR R1, [R0]
	BX LR

palindrome_found:
	// Write 'Palindrome detected' to UART
	LDR LETTER_ADR, =pali
	BL write_string

	// Switch on only the 5 rightmost LEDs
	LDR R1, =PALI_PATTERN
	BL display_led
	B exit
	
palindrome_not_found:
	// Write 'Not a palindrome' to UART
	LDR LETTER_ADR, =npali
	BL write_string

	// Switch on only the 5 leftmost LEDs
	LDR R1, =NPALI_PATTERN
	BL display_led
	B exit
	
exit:
	// Branch here for exit
	B exit
	

.section .data
.align
	// This is the input you are supposed to check for a palindrome
	// You can modify the string during development, however you
	// are not allowed to change the label 'input'!
	input: .asciz "level"
	// input: .asciz "8448"
    // input: .asciz "KayAk"
    // input: .asciz "step on no pets"
    // input: .asciz "Never odd or even"

	// Values for the messages we want to display
	pali: .asciz "Palindrome detected"
	npali: .asciz "Not a palindrome"
.end