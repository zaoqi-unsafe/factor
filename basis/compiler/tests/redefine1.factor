USING: accessors compiler compiler.units tools.test math parser
kernel sequences sequences.private classes.mixin generic
definitions arrays words assocs eval strings ;
IN: compiler.tests

GENERIC: method-redefine-generic-1 ( a -- b )

M: integer method-redefine-generic-1 3 + ;

: method-redefine-test-1 ( -- b ) 3 method-redefine-generic-1 ;

[ 6 ] [ method-redefine-test-1 ] unit-test

[ ] [ "IN: compiler.tests USE: math M: fixnum method-redefine-generic-1 4 + ;" eval( -- ) ] unit-test

[ 7 ] [ method-redefine-test-1 ] unit-test

[ ] [ [ fixnum \ method-redefine-generic-1 method forget ] with-compilation-unit ] unit-test

[ 6 ] [ method-redefine-test-1 ] unit-test

GENERIC: method-redefine-generic-2 ( a -- b )

M: integer method-redefine-generic-2 3 + ;

: method-redefine-test-2 ( -- b ) 3 method-redefine-generic-2 ;

[ 6 ] [ method-redefine-test-2 ] unit-test

[ ] [ "IN: compiler.tests USE: kernel USE: math M: fixnum method-redefine-generic-2 4 + ; USE: strings M: string method-redefine-generic-2 drop f ;" eval( -- ) ] unit-test

[ 7 ] [ method-redefine-test-2 ] unit-test

[ ] [
    [
        fixnum string [ \ method-redefine-generic-2 method forget ] bi@
    ] with-compilation-unit
] unit-test

! Test ripple-up behavior
: hey ( -- ) ;
: there ( -- ) hey ;

[ t ] [ \ hey optimized>> ] unit-test
[ t ] [ \ there optimized>> ] unit-test
[ ] [ "IN: compiler.tests : hey ( -- ) 3 ;" eval( -- ) ] unit-test
[ f ] [ \ hey optimized>> ] unit-test
[ f ] [ \ there optimized>> ] unit-test
[ ] [ "IN: compiler.tests : hey ( -- ) ;" eval( -- ) ] unit-test
[ t ] [ \ there optimized>> ] unit-test

: good ( -- ) ;
: bad ( -- ) good ;
: ugly ( -- ) bad ;

[ t ] [ \ good optimized>> ] unit-test
[ t ] [ \ bad optimized>> ] unit-test
[ t ] [ \ ugly optimized>> ] unit-test

[ f ] [ \ good compiled-usage assoc-empty? ] unit-test

[ ] [ "IN: compiler.tests : good ( -- ) 3 ;" eval( -- ) ] unit-test

[ f ] [ \ good optimized>> ] unit-test
[ f ] [ \ bad optimized>> ] unit-test
[ f ] [ \ ugly optimized>> ] unit-test

[ t ] [ \ good compiled-usage assoc-empty? ] unit-test

[ ] [ "IN: compiler.tests : good ( -- ) ;" eval( -- ) ] unit-test

[ t ] [ \ good optimized>> ] unit-test
[ t ] [ \ bad optimized>> ] unit-test
[ t ] [ \ ugly optimized>> ] unit-test

[ f ] [ \ good compiled-usage assoc-empty? ] unit-test
