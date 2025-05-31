
# file ./utest
file ./ff
# set args -d
# break info
# break instr_parse_name
# break FF_Exit
break instr_halt

#
# break instr_colon
# break inner_interpreter
# break instr_compile
# break test_number_parsing

break decompile_dictionary
break test
run
# display ctx->data_stack
# display context->data_stack
# break instr_compile
# c

tui enable

