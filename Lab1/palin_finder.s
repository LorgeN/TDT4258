// Useful constants
.equ LED_ADDRESS, 0xFF200000
.equ UART_ADDRESS, 0xFF201000
.equ PALI_PATTERN, 0b0000011111
.equ NPALI_PATTERN, 0b1111100000

.global _start

.section .text

_start:
	BL check_input
	B check_palindrome

// Checks the length of the input and saves it in register R0 by iterating
// until it reaches a null value
check_input:
	LENGTH .req R0
	LETTER_ADR .req R1
	LETTER .req R2

	// Load the address of the first letter to LETTER_ADR register
	LDR LETTER_ADR, =input
	// Make sure the initial value in the LENGTH register is 0
	MOV LENGTH, #0

check_input_internal:
	// Load the current letter into the LETTER register, and increment the 
	// address by 1
	LDRB LETTER, [LETTER_ADR], #1
	// Compare letter to 0, which is null, indicating the end of the string
	CMP LETTER, #0
	
	// If the current letter is null we exit the function
	BXEQ LR
	
	// If not, we increment the length value by 1 and continue the loop
	ADD LENGTH, #1
	B check_input_internal
	
// Function to check if the input is a palindrome by iterating from both 
// ends, skipping spaces and ignoring casing
check_palindrome:
	// Push the values of callee-saved registers to the stack so we can pop
	// them back later
	PUSH {R4, R5, R6, R7}
	INDEX_LOW .req R4
	INDEX_HIGH .req R5
	LETTER_LOW .req R6
	LETTER_HIGH .req R7

	// Initialize values. INDEX_LOW is the left point of where we are 
	// currently evaluating the string, INDEX_HIGH is the right part
	LDR INDEX_LOW, =input
	ADD INDEX_HIGH, INDEX_LOW, LENGTH

// Internal looping branch point for the palindrome check
restart_check:
	CMP INDEX_LOW, INDEX_HIGH
	// Ensure that we have not hit the middle of the string. If we have,
	// we know that the string is a palindrome since we have found no
	// letters that arent matching. If this is the case we branch to
	// palindrome_found label, but first pop the callee-saved registers
	// we use back from the stack
	POPGE {R4, R5, R6, R7}
	BGE palindrome_found

	// Load the byte value at the low index address and compare it to
	// 32 which is the highest ASCII value for whitespace 
	LDRB LETTER_LOW, [INDEX_LOW]
	CMP LETTER_LOW, #32

	// If it's less than or equal to 32, we ignore it since it's white 
	// space and restart the check. We have to add here only if it is
	// invalid to avoid skipping letters when restarting from the procedure
	// from the same check for high index
	ADDLE INDEX_LOW, #1
	BLE restart_check

	// Same procedure for the high index as for the low index
	LDRB LETTER_HIGH, [INDEX_HIGH]
	CMP LETTER_HIGH, #32

	// Move high index down 1. Don't need to check anything here as we
	// know that there is no whitespace at low index
	SUB INDEX_HIGH, #1

	// Restart check if letter at high index is whitespace
	BLE restart_check

	// Now we increment the low index either way to prepare it for the
	// next run, same as the decrement for index high earlier
	ADD INDEX_LOW, #1

	// Check casing for both letters. 97 is where lowercase letters
	// start in ASCII. If the letter it is below that, it is uppercase
	// and we add 32 to make it lowercase
	CMP LETTER_LOW, #97
	ADDLT LETTER_LOW, #32

	CMP LETTER_HIGH, #97
	ADDLT LETTER_HIGH, #32

	// Compare the two letters. We expect them to be the same if this
	// is a valid palindrome. If not, we have to exit the loop and call
	// the palindrome_not_found label
	CMP LETTER_LOW, LETTER_HIGH
	POPNE {R4, R5, R6, R7}
	BNE palindrome_not_found

	// All is good so far; we restart the check for the next indices!
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