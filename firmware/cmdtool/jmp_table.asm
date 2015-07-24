	.module jmp_table
	.optsdcc -mmcs51 --model-small

	.globl _putchar
	.globl _putchar_jmp
	
	.globl _getchar
	.globl _getchar_jmp
	
	.area JTABL (ABS, CODE)
	.org 0x7f80
_putchar_jmp:
	ljmp _putchar
_getchar_jmp:
	ljmp _getchar
	
