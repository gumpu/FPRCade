: CR 13 EMIT 10 EMIT ;
: SP 32 EMIT ;

: INTERPRET
BEGIN
  FIND
  WORDBUFFER C@ 0 =
  IF
    DROP
    CR 'O' EMIT 'K' EMIT CR QUERY
  ELSE
    DUP 0 =
    IF
      DROP
      WORDBUFFER
      (NUMBER)
      IF
        STATE @
        IF (LITERAL) ELSE PASS THEN
      ELSE
        DROP
        '?' EMIT CR
      THEN
    ELSE
      STATE @
      IF
        DUP ?IMMEDIATE
        IF EXECUTE ELSE , THEN
      ELSE
        EXECUTE
      THEN
    THEN
  THEN
AGAIN ;

