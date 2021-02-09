\ DOT S word  .S called 'CYA' here.
HEX
: #SD> DEPTH [CHAR] < EMIT . 8 EMIT [CHAR] > EMIT SPACE ;
: ###> <# DUP ABS 0 #S ROT SIGN #> TYPE SPACE ;
: CYAB 1 DEPTH 0 2 + = IF -1 EXIT THEN ;
: CYAO 0 DEPTH 0 1 + = IF -1 EXIT THEN ;
: DOTTUS
  DEPTH
  BEGIN
      SWAP >R
      DEPTH 2 = IF 
          SWAP DUP ###> SWAP 2 -
          BEGIN
              R> DUP ###> SWAP 1 -
              DUP 1 + 0 = IF
                  EXIT THEN 
          AGAIN
      EXIT THEN
  AGAIN  ;
: CYA #SD>
  CYAO -1 = IF
      DROP QUIT THEN 
  CYAB -1 = IF
      DROP DUP ###> QUIT THEN
  DOTTUS DROP ;

\ END
\ : #SD> DEPTH 3C EMIT . 8 EMIT 3E EMIT SPACE ;
