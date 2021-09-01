.global _start // This marks the symbol at which execution starts (entrypoint)


.section .text
	// The text section is the code section containing the instructions
	// that are executed on the processor
loop:
	// This example iteratively multiplies r2*r1
	cmp r1, #0     // Compare r1 to 0
	bne loop_main  // If not equal branch OVER next instruction
	mov pc, lr     // Branch back to who called loop
	loop_main:
	add r0, r0, r2 // Add r0 to r2 and put it on r0
	sub r1, r1, #1 // Substract 1 from r1 and put it on r1
	b loop         // branch back to beginning of loop

_start:
	// Here your execution starts
	mov r1, #10 // Decimal 10 o register r1
	mov r2, #2  // Decimal 2 to register r2
	mov r0, #0  // Decimal 0 to register r0
	bl loop     // branch and link to function loop
	b exit      // branch to exit

exit:
	b exit // Branch to iteself (exit of programm)

.section .data
.align
	// This section is evaluated before execution to put things into
	// memory that are required for the execution of your application

	
.end

Whatever is written after .end is not processed.
