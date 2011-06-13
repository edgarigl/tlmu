	.global	_start
_start:
	move.d	0x19008000, $sp
	jsr	_run
	nop
