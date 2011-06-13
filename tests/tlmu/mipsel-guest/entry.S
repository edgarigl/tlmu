	.global	_start
_start:
	lui	$v0, 0x1900
	ori	$v0, $v0, 0x8000
	move	$sp, $v0
	j	run
	nop
