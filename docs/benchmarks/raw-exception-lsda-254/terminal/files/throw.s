	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 15, 0	sdk_version 15, 5
	.globl	__Z12throwing_apiv              ; -- Begin function _Z12throwing_apiv
	.p2align	2
__Z12throwing_apiv:                     ; @_Z12throwing_apiv
Lfunc_begin0:
	.cfi_startproc
	.cfi_personality 155, ___gxx_personality_v0
	.cfi_lsda 16, Lexception0
; %bb.0:
	stp	x20, x19, [sp, #-32]!           ; 16-byte Folded Spill
	stp	x29, x30, [sp, #16]             ; 16-byte Folded Spill
	add	x29, sp, #16
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	.cfi_offset w19, -24
	.cfi_offset w20, -32
	mov	x19, x8
	adrp	x8, _selected_exception@PAGE
	ldr	w9, [x8, _selected_exception@PAGEOFF]
	cbz	w9, LBB0_4
; %bb.1:
	ldr	w9, [x8, _selected_exception@PAGEOFF]
	cmp	w9, #1
	b.eq	LBB0_6
; %bb.2:
	ldr	w8, [x8, _selected_exception@PAGEOFF]
	cmp	w8, #2
	b.eq	LBB0_8
; %bb.3:
	mov	w0, #32                         ; =0x20
	bl	__Znwm
	str	xzr, [x0, #16]
Lloh0:
	adrp	x8, __ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE@GOTPAGE
Lloh1:
	ldr	x8, [x8, __ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE@GOTPAGEOFF]
	add	x8, x8, #16
	stp	x8, xzr, [x0]
	mov	w8, #7                          ; =0x7
	str	x0, [x19, #8]
	str	w8, [x0, #24]!
	str	x0, [x19]
	ldp	x29, x30, [sp, #16]             ; 16-byte Folded Reload
	ldp	x20, x19, [sp], #32             ; 16-byte Folded Reload
	ret
LBB0_4:
	mov	w0, #16                         ; =0x10
	bl	___cxa_allocate_exception
	mov	x19, x0
Ltmp6:
Lloh2:
	adrp	x1, l_.str@PAGE
Lloh3:
	add	x1, x1, l_.str@PAGEOFF
	bl	__ZNSt16invalid_argumentC1B8ne190102EPKc
Ltmp7:
; %bb.5:
Lloh4:
	adrp	x1, __ZTISt16invalid_argument@GOTPAGE
Lloh5:
	ldr	x1, [x1, __ZTISt16invalid_argument@GOTPAGEOFF]
Lloh6:
	adrp	x2, __ZNSt16invalid_argumentD1Ev@GOTPAGE
Lloh7:
	ldr	x2, [x2, __ZNSt16invalid_argumentD1Ev@GOTPAGEOFF]
	mov	x0, x19
	bl	___cxa_throw
LBB0_6:
	mov	w0, #16                         ; =0x10
	bl	___cxa_allocate_exception
	mov	x19, x0
Ltmp3:
Lloh8:
	adrp	x1, l_.str.1@PAGE
Lloh9:
	add	x1, x1, l_.str.1@PAGEOFF
	bl	__ZNSt12length_errorC1B8ne190102EPKc
Ltmp4:
; %bb.7:
Lloh10:
	adrp	x1, __ZTISt12length_error@GOTPAGE
Lloh11:
	ldr	x1, [x1, __ZTISt12length_error@GOTPAGEOFF]
Lloh12:
	adrp	x2, __ZNSt12length_errorD1Ev@GOTPAGE
Lloh13:
	ldr	x2, [x2, __ZNSt12length_errorD1Ev@GOTPAGEOFF]
	mov	x0, x19
	bl	___cxa_throw
LBB0_8:
	mov	w0, #16                         ; =0x10
	bl	___cxa_allocate_exception
	mov	x19, x0
Ltmp0:
Lloh14:
	adrp	x1, l_.str.2@PAGE
Lloh15:
	add	x1, x1, l_.str.2@PAGEOFF
	bl	__ZNSt13runtime_errorC1EPKc
Ltmp1:
; %bb.9:
Lloh16:
	adrp	x1, __ZTISt13runtime_error@GOTPAGE
Lloh17:
	ldr	x1, [x1, __ZTISt13runtime_error@GOTPAGEOFF]
Lloh18:
	adrp	x2, __ZNSt13runtime_errorD1Ev@GOTPAGE
Lloh19:
	ldr	x2, [x2, __ZNSt13runtime_errorD1Ev@GOTPAGEOFF]
	mov	x0, x19
	bl	___cxa_throw
LBB0_10:
Ltmp2:
	mov	x20, x0
	mov	x0, x19
	bl	___cxa_free_exception
	mov	x0, x20
	bl	__Unwind_Resume
LBB0_11:
Ltmp5:
	mov	x20, x0
	mov	x0, x19
	bl	___cxa_free_exception
	mov	x0, x20
	bl	__Unwind_Resume
LBB0_12:
Ltmp8:
	mov	x20, x0
	mov	x0, x19
	bl	___cxa_free_exception
	mov	x0, x20
	bl	__Unwind_Resume
	.loh AdrpLdrGot	Lloh0, Lloh1
	.loh AdrpAdd	Lloh2, Lloh3
	.loh AdrpLdrGot	Lloh6, Lloh7
	.loh AdrpLdrGot	Lloh4, Lloh5
	.loh AdrpAdd	Lloh8, Lloh9
	.loh AdrpLdrGot	Lloh12, Lloh13
	.loh AdrpLdrGot	Lloh10, Lloh11
	.loh AdrpAdd	Lloh14, Lloh15
	.loh AdrpLdrGot	Lloh18, Lloh19
	.loh AdrpLdrGot	Lloh16, Lloh17
Lfunc_end0:
	.cfi_endproc
	.section	__TEXT,__gcc_except_tab
	.p2align	2, 0x0
GCC_except_table0:
Lexception0:
	.byte	255                             ; @LPStart Encoding = omit
	.byte	255                             ; @TType Encoding = omit
	.byte	1                               ; Call site Encoding = uleb128
	.uleb128 Lcst_end0-Lcst_begin0
Lcst_begin0:
	.uleb128 Lfunc_begin0-Lfunc_begin0      ; >> Call Site 1 <<
	.uleb128 Ltmp6-Lfunc_begin0             ;   Call between Lfunc_begin0 and Ltmp6
	.byte	0                               ;     has no landing pad
	.byte	0                               ;   On action: cleanup
	.uleb128 Ltmp6-Lfunc_begin0             ; >> Call Site 2 <<
	.uleb128 Ltmp7-Ltmp6                    ;   Call between Ltmp6 and Ltmp7
	.uleb128 Ltmp8-Lfunc_begin0             ;     jumps to Ltmp8
	.byte	0                               ;   On action: cleanup
	.uleb128 Ltmp7-Lfunc_begin0             ; >> Call Site 3 <<
	.uleb128 Ltmp3-Ltmp7                    ;   Call between Ltmp7 and Ltmp3
	.byte	0                               ;     has no landing pad
	.byte	0                               ;   On action: cleanup
	.uleb128 Ltmp3-Lfunc_begin0             ; >> Call Site 4 <<
	.uleb128 Ltmp4-Ltmp3                    ;   Call between Ltmp3 and Ltmp4
	.uleb128 Ltmp5-Lfunc_begin0             ;     jumps to Ltmp5
	.byte	0                               ;   On action: cleanup
	.uleb128 Ltmp4-Lfunc_begin0             ; >> Call Site 5 <<
	.uleb128 Ltmp0-Ltmp4                    ;   Call between Ltmp4 and Ltmp0
	.byte	0                               ;     has no landing pad
	.byte	0                               ;   On action: cleanup
	.uleb128 Ltmp0-Lfunc_begin0             ; >> Call Site 6 <<
	.uleb128 Ltmp1-Ltmp0                    ;   Call between Ltmp0 and Ltmp1
	.uleb128 Ltmp2-Lfunc_begin0             ;     jumps to Ltmp2
	.byte	0                               ;   On action: cleanup
	.uleb128 Ltmp1-Lfunc_begin0             ; >> Call Site 7 <<
	.uleb128 Lfunc_end0-Ltmp1               ;   Call between Ltmp1 and Lfunc_end0
	.byte	0                               ;     has no landing pad
	.byte	0                               ;   On action: cleanup
Lcst_end0:
	.p2align	2, 0x0
                                        ; -- End function
	.section	__TEXT,__text,regular,pure_instructions
	.private_extern	__ZNSt16invalid_argumentC1B8ne190102EPKc ; -- Begin function _ZNSt16invalid_argumentC1B8ne190102EPKc
	.globl	__ZNSt16invalid_argumentC1B8ne190102EPKc
	.weak_def_can_be_hidden	__ZNSt16invalid_argumentC1B8ne190102EPKc
	.p2align	2
__ZNSt16invalid_argumentC1B8ne190102EPKc: ; @_ZNSt16invalid_argumentC1B8ne190102EPKc
	.cfi_startproc
; %bb.0:
	stp	x29, x30, [sp, #-16]!           ; 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	bl	__ZNSt11logic_errorC2EPKc
Lloh20:
	adrp	x8, __ZTVSt16invalid_argument@GOTPAGE
Lloh21:
	ldr	x8, [x8, __ZTVSt16invalid_argument@GOTPAGEOFF]
	add	x8, x8, #16
	str	x8, [x0]
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
	.loh AdrpLdrGot	Lloh20, Lloh21
	.cfi_endproc
                                        ; -- End function
	.private_extern	__ZNSt12length_errorC1B8ne190102EPKc ; -- Begin function _ZNSt12length_errorC1B8ne190102EPKc
	.globl	__ZNSt12length_errorC1B8ne190102EPKc
	.weak_def_can_be_hidden	__ZNSt12length_errorC1B8ne190102EPKc
	.p2align	2
__ZNSt12length_errorC1B8ne190102EPKc:   ; @_ZNSt12length_errorC1B8ne190102EPKc
	.cfi_startproc
; %bb.0:
	stp	x29, x30, [sp, #-16]!           ; 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	bl	__ZNSt11logic_errorC2EPKc
Lloh22:
	adrp	x8, __ZTVSt12length_error@GOTPAGE
Lloh23:
	ldr	x8, [x8, __ZTVSt12length_error@GOTPAGEOFF]
	add	x8, x8, #16
	str	x8, [x0]
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	ret
	.loh AdrpLdrGot	Lloh22, Lloh23
	.cfi_endproc
                                        ; -- End function
	.globl	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED1Ev ; -- Begin function _ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED1Ev
	.weak_def_can_be_hidden	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED1Ev
	.p2align	2
__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED1Ev: ; @_ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED1Ev
	.cfi_startproc
; %bb.0:
Lloh24:
	adrp	x8, __ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE@GOTPAGE
Lloh25:
	ldr	x8, [x8, __ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE@GOTPAGEOFF]
	add	x8, x8, #16
	str	x8, [x0]
	b	__ZNSt3__119__shared_weak_countD2Ev
	.loh AdrpLdrGot	Lloh24, Lloh25
	.cfi_endproc
                                        ; -- End function
	.globl	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED0Ev ; -- Begin function _ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED0Ev
	.weak_def_can_be_hidden	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED0Ev
	.p2align	2
__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED0Ev: ; @_ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED0Ev
	.cfi_startproc
; %bb.0:
	stp	x29, x30, [sp, #-16]!           ; 16-byte Folded Spill
	mov	x29, sp
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
Lloh26:
	adrp	x8, __ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE@GOTPAGE
Lloh27:
	ldr	x8, [x8, __ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE@GOTPAGEOFF]
	add	x8, x8, #16
	str	x8, [x0]
	bl	__ZNSt3__119__shared_weak_countD2Ev
	ldp	x29, x30, [sp], #16             ; 16-byte Folded Reload
	b	__ZdlPv
	.loh AdrpLdrGot	Lloh26, Lloh27
	.cfi_endproc
                                        ; -- End function
	.private_extern	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE16__on_zero_sharedEv ; -- Begin function _ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE16__on_zero_sharedEv
	.globl	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE16__on_zero_sharedEv
	.weak_def_can_be_hidden	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE16__on_zero_sharedEv
	.p2align	2
__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE16__on_zero_sharedEv: ; @_ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE16__on_zero_sharedEv
	.cfi_startproc
; %bb.0:
	ret
	.cfi_endproc
                                        ; -- End function
	.private_extern	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE21__on_zero_shared_weakEv ; -- Begin function _ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE21__on_zero_shared_weakEv
	.globl	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE21__on_zero_shared_weakEv
	.weak_def_can_be_hidden	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE21__on_zero_shared_weakEv
	.p2align	2
__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE21__on_zero_shared_weakEv: ; @_ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE21__on_zero_shared_weakEv
	.cfi_startproc
; %bb.0:
	b	__ZdlPv
	.cfi_endproc
                                        ; -- End function
	.globl	_selected_exception             ; @selected_exception
.zerofill __DATA,__common,_selected_exception,4,2
	.section	__TEXT,__cstring,cstring_literals
l_.str:                                 ; @.str
	.asciz	"invalid marker"

l_.str.1:                               ; @.str.1
	.asciz	"length marker"

l_.str.2:                               ; @.str.2
	.asciz	"runtime marker"

	.section	__DATA,__const
	.globl	__ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE ; @_ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.weak_def_can_be_hidden	__ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.p2align	3, 0x0
__ZTVNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE:
	.quad	0
	.quad	__ZTINSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.quad	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED1Ev
	.quad	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEED0Ev
	.quad	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE16__on_zero_sharedEv
	.quad	__ZNKSt3__119__shared_weak_count13__get_deleterERKSt9type_info
	.quad	__ZNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEE21__on_zero_shared_weakEv

	.private_extern	__ZTSNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE ; @_ZTSNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.section	__TEXT,__const
	.globl	__ZTSNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.weak_definition	__ZTSNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
__ZTSNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE:
	.asciz	"NSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE"

	.private_extern	__ZTINSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE ; @_ZTINSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.section	__DATA,__const
	.globl	__ZTINSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.weak_definition	__ZTINSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE
	.p2align	3, 0x0
__ZTINSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE:
	.quad	__ZTVN10__cxxabiv120__si_class_type_infoE+16
	.quad	__ZTSNSt3__120__shared_ptr_emplaceIKiNS_9allocatorIiEEEE-9223372036854775808
	.quad	__ZTINSt3__119__shared_weak_countE

.subsections_via_symbols
