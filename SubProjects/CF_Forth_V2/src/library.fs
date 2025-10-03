( library )
: hex #16 base ! ;
: decimal #10 base ! ;
: space #32 emit ;
: cr $0d emit $0a emit ;
: 1+ 1 + ;
: 2+ 2 + ;
: 0= 0 = ;
: variable create 2 allot ;
: ] state 1 ! ; immediate
: [ state 0 ! ; immediate
: constant create , does> @ ;
99 constant foo
foo

