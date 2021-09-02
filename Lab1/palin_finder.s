.equ LED_ADDRESS, 0xFF200000
.equ UART_ADDRESS, 0xFF201000
.equ PALI_PATTERN, 0b0000011111
.equ NPALI_PATTERN, 0b1111100000

.global _start

.section .text

// R0: Addresses
// R1: Currently processing
// R2: Input length
// R3: Low index
// R4: High index
// R5: String input

_start:
	MOV R2, #0
	MOV R3, #0

	BL check_input
	B check_palindrome

check_input:
	LDR R5, =input
check_input_internal:
	LDRB R6, [R5], #1
	CMP R6, #0
	
	BXEQ LR
	
	ADD R2, #1
	B check_input_internal
	
check_palindrome:
	LDR R3, =input
	ADD R4, R3, R2

restart_check:
	CMP R3, R4
	BGE palindrome_found

skip_space_low:
	LDRB R5, [R3]
	CMP R5, #32
	BGT skip_space_high

	ADD R3, #1
	CMP R3, R4
	BGE palindrome_found

	B skip_space_low

skip_space_high:
	LDRB R6, [R4]
	CMP R6, #32
	BGT lower_letters

	SUB R4, #1
	CMP R3, R4
	BGE palindrome_found

	B skip_space_high

lower_letters:
	CMP R5, #97
	ADDLT R5, #32

	CMP R6, #97
	ADDLT R6, #32

	CMP R5, R6
	BNE palindrome_not_found

	ADD R3, #1
	SUB R4, #1
	B restart_check

write_string:
	LDR R0, =UART_ADDRESS
write_string_internal:
	LDRB R6, [R5], #1
	CMP R6, #0
	BXEQ LR 

	STR R6, [R0]
	B write_string_internal

palindrome_found:
	// Write 'Palindrome detected' to UART
	LDR R5, =pali
	BL write_string

	// Switch on only the 5 rightmost LEDs
	LDR R1, =PALI_PATTERN
	B palindrome_finish

palindrome_not_found:
	// Write 'Not a palindrome' to UART
	LDR R5, =npali
	BL write_string

	// Switch on only the 5 leftmost LEDs
	LDR R1, =NPALI_PATTERN
	B palindrome_finish

palindrome_finish:
	LDR R0, =LED_ADDRESS
	STR R1, [R0]
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