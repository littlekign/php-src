--TEST--
Locale::addLikelySubtags/Locale::minimizeSubtags usage
--EXTENSIONS--
intl
--FILE--
<?php
$locale = "en";
$max = Locale::addLikelySubtags($locale);
$min = Locale::minimizeSubtags($max);
var_dump($min === $locale);
var_dump($max !== $locale && strlen($max) > strlen($locale));
var_dump(Locale::addLikelySubtags($max) === $max);
var_dump(Locale::minimizeSubtags($locale) === $locale);
var_dump(Locale::addLikelySubtags("%%%invalid%%%locale%%%"));
var_dump(intl_get_error_message());
var_dump(Locale::minimizeSubtags("%%%Invalid%%%maximized%%%locale%%%"));
var_dump(intl_get_error_message());
var_dump(Locale::addLikelySubTags(str_repeat($locale, 1024)));
var_dump(intl_get_error_message());
var_dump(Locale::minimizeSubTags(str_repeat($max, 1024)));
var_dump(intl_get_error_message());
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
string(68) "Locale::addLikelySubtags(): invalid locale: U_ILLEGAL_ARGUMENT_ERROR"
bool(false)
string(67) "Locale::minimizeSubtags(): invalid locale: U_ILLEGAL_ARGUMENT_ERROR"
bool(false)
string(68) "Locale::addLikelySubtags(): invalid locale: U_ILLEGAL_ARGUMENT_ERROR"
bool(false)
string(%d) "Locale::minimizeSubtags(): invalid locale: %s"
