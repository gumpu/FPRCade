" Vim syntax file
" Language:     Fasm stack-master-16 assembler format
" Maintainer:   Frans Slothouber
" Last Change:  2025
" Version:      0.01
" Remark:       --
" License:      GPL
"

if version < 600
	syntax clear
elseif exists("b:current_syntax")
	finish
endif

syn case ignore
syn iskeyword @,48-57,.,_,$,%,#

syn match   smasmLabel			/[A-Za-z0-9_]\+:/
syn match   smasmTODO			/\<\(TODO\|FIXME\|NOTE\)\>/ contained
syn region  smasmCommentSingle	        start=/;/ end=/$/ contains=smasmTODO
syn keyword smasmDirective		.b .str .w .l .org .align .def
syn region  smasmString			start=/"/  end=/"/ skip=/\\"/
syn region  smasmChar			start=/'/  end=/'/ skip=/\\'/

" Numbers
syn match   smasmBinaryNumber		/\<%[01]\+\>/
syn match   smasmHexNumber		/\<$\x\+\>/
syn match   smasmDecimalNumber		/\<[0-9]\+\>/

syn keyword smasmOpcode			leave enter nop ldl ldh dup swap
syn keyword smasmOpcode			add gt lt bif neg halt
syn keyword smasmOpcode			eq xor or and
syn keyword smasmOpcode			rd ird sto isto

hi def link smasmDirective	Type
hi def link smasmLabel		Function
hi def link smasmTODO		Todo
hi def link smasmOpcode		Keyword
hi def link smasmCommentSingle	Comment
hi def link smasmString		String
hi def link smasmChar		Character
hi def link smasmBinaryNumber	Constant
hi def link smasmHexNumber	Constant
hi def link smasmDecimalNumber	Constant

" finishing touches
let b:current_syntax = "smasm"

syn sync ccomment
syn sync linebreaks=1

" vim: ts=8 sw=8 :
