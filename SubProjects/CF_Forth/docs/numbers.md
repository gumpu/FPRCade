When converting input numbers, the text interpreter shall recognize
integer numbers in the form <anynum>.

    <anynum> := { <BASEnum> | <decnum> | <hexnum> | <binnum> | <cnum> }
    <BASEnum> := [-]<bdigit><bdigit>*
    <decnum> := #[-]<decdigit><decdigit>*
    <hexnum> := $[-]<hexdigit><hexdigit>*
    <binnum> := %[-]<bindigit><bindigit>*
    <cnum> := ’<char>’
    <bindigit> := { 0 | 1 }
    <decdigit> := { 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 }
    <hexdigit> := { <decdigit> | a | b | c | d | e | f | A | B | C | D | E | F }

    <bdigit> represents a digit according to the value of BASE.

    For <hexdigit>, the digits a .. f have the values 10 .. 15.

    <char> represents any printable character.

    The radix used for number conversion is:
    <BASEnum>      the value in BASE
    <decnum>       10
    <hexnum>       16
    <binnum>       2
    <cnum>         the number is the value of <char>


