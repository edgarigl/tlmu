	.global	_start
_start:
	mov	r3, #419430400
	add	r3, r3, #32768
	add	r3, r3, #32768
	mov	sp, r3
	bl	run
