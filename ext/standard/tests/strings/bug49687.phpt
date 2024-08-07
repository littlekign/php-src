--TEST--
Bug #49687 Several utf8_decode deficiencies and vulnerabilities
--FILE--
<?php

$tests = array(
    "\x41\xC2\x3E\x42",
    "\xE3\x80\x22",
    "\x41\x98\xBA\x42\xE2\x98\x43\xE2\x98\xBA\xE2\x98",
);
foreach ($tests as $t) {
    echo bin2hex(utf8_decode($t)), "\n";
}
echo "Done.\n";
?>
--EXPECTF--
Deprecated: Function utf8_decode() is deprecated since 8.2, visit the php.net documentation for various alternatives in %s on line %d
413f3e42

Deprecated: Function utf8_decode() is deprecated since 8.2, visit the php.net documentation for various alternatives in %s on line %d
3f22

Deprecated: Function utf8_decode() is deprecated since 8.2, visit the php.net documentation for various alternatives in %s on line %d
413f3f423f433f3f
Done.
