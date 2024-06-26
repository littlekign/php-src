--TEST--
grapheme_str_split function tests
--EXTENSIONS--
intl
--FILE--
<?php
function ut_main()
{
    $res_str = '';

    $tests = array(
        array( "abc", 3, array("abc") ),
        array( "abc", 2, array("ab", "c") ),
        array( "abc", 1, array("a", "b", "c" ) ),
        array( "土下座🙇‍♀を", 1, array("土", "下", "座", "🙇‍♀", "を") ),
        array( "土下座🙇‍♀を", 6, array("土下座🙇‍♀を") ),
        array( "null\x00byte", 1, array("n", "u", "l", "l", "\x00", "b", "y", "t", "e") ),
    );

    foreach( $tests as $test ) {
        $res_str .= "grapheme cluster for str_split - param {$test[0]}, length {$test[1]} ";
        $result = grapheme_str_split($test[0], $test[1]);
        $res_str .= dump_array_bin2hex($test[count($test)-1]) . check_result($result, $test[count($test)-1]) . "\n";
    }
    return $res_str;
}

function dump_array_bin2hex($values) {
	$returns = [];
	foreach ($values as $value) {
		$returns[] = bin2hex($value);
	}
	return '[' . implode(',', $returns) . ']';
}

echo ut_main();

function check_result($result, $expected) {

    if ( $result === false ) {
        $result = 'false';
    }

    if ( $result !== $expected) {
        echo "result: {$result}\n";
        echo "expected: {$expected}\n";
    }

    return "";
}
?>
--EXPECT--
grapheme cluster for str_split - param abc, length 3 [616263]
grapheme cluster for str_split - param abc, length 2 [6162,63]
grapheme cluster for str_split - param abc, length 1 [61,62,63]
grapheme cluster for str_split - param 土下座🙇‍♀を, length 1 [e59c9f,e4b88b,e5baa7,f09f9987e2808de29980,e38292]
grapheme cluster for str_split - param 土下座🙇‍♀を, length 6 [e59c9fe4b88be5baa7f09f9987e2808de29980e38292]
grapheme cluster for str_split - param null byte, length 1 [6e,75,6c,6c,00,62,79,74,65]
