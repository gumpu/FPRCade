( vi: ft=forth
)
: SP 32 EMIT ;

: INTERPRET
BEGIN
  FIND
  WORDBUFFER C@ 0 =
  IF
    ( No word could be found, request more input )
    DROP
    'O' EMIT 'K' EMIT QUERY
  ELSE
    DUP 0 =
    IF
      ( Word did not occur in the dictionary, is it a number ? )
      DROP
      WORDBUFFER (NUMBER)
      IF
        ( It was a valid number, compile or put on the stack ? )
        STATE @
        IF (LITERAL) ELSE PASS THEN
      ELSE
        ( Not a word that we know )
        DROP
        '?' EMIT ABORT
      THEN
    ELSE
      STATE @
      IF
        ( We are compiling, is this an immediate word ? )
        DUP ?IMMEDIATE
        IF EXECUTE ELSE , THEN
      ELSE
        EXECUTE
      THEN
    THEN
  THEN
AGAIN ;

