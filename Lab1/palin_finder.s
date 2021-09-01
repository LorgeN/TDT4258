.equ LED_ADDRESS, 0xFF200000
.equ PALI_PATTERN, 0b0000011111
.equ NPALI_PATTERN, 0b1111100000

.global _start

.section .text

_start:
	LDR R0, =LED_ADDRESS
	
	B palindrome_not_found
	// Here your execution starts
	B exit

	
check_input:
	// You could use this symbol to check for your input length
	// you can assume that your input string is at least 2 characters 
	// long and ends with a null byte
	
	
check_palindrome:
	// Here you could check whether input is a palindrome or not
	
	
palindrome_found:
	// Switch on only the 5 rightmost LEDs
	// Write 'Palindrome detected' to UART
	LDR R1, =PALI_PATTERN
	B palindrome_finish

palindrome_not_found:
	// Switch on only the 5 leftmost LEDs
	// Write 'Not a pa.equ LED_ADDRESS, 0xFF200000
.equ PALI_PATTERN, 0b0000011111
.equ NPALI_PATTERN, 0b1111100000

.global _start

.section .text

_start:
	LDR R0, =LED_ADDRESS
	
	B palindrome_not_found
	// Here your execution starts
	B exit

	
check_input:
	// You could use this symbol to check for your input length
	// you can assume that your input string is at least 2 characters 
	// long and ends with a null byte
	
	
check_palindrome:
	// Here you could check whether input is a palindrome or not
	
	
palindrome_found:
	// Switch on only the 5 rightmost LEDs
	// Write 'Palindrome detected' to UART
	LDR R1, =PALI_PATTERN
	B palindrome_finish

palindrome_not_found:
	// Switch on only the 5 leftmost LEDs
	// Write 'Not a palindrome' to UART
	LDR R1, =NPALI_PATTERN
	B palindrome_finish

palindrome_finish:
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


.end


lindrome' to UART
	LDR R1, =NPALI_PATTERN
	B palindrome_finish

palindrome_finish:
	STR R1, [R0]
	B exit
	
exit:
	// Branch here for exit
	b exit
	

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


.end


