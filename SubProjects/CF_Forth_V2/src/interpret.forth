: SP 32 EMIT ;

: INTERPRET
BEGIN
  FIND
  WORDBUFFER C@ 0 =
  IF
    DROP
    'O' EMIT 'K' EMIT QUERY
  ELSE
    DUP 0 =
    IF
      DROP
      WORDBUFFER (NUMBER)
      IF
        STATE @
        IF (LITERAL) ELSE PASS THEN
      ELSE
        DROP
        '?' EMIT
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

