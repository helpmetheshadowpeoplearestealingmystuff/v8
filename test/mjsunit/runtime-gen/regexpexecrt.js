// Copyright 2014 the V8 project authors. All rights reserved.
// AUTO-GENERATED BY tools/generate-runtime-tests.py, DO NOT MODIFY
// Flags: --allow-natives-syntax --harmony
var _regexp = /ab/g;
var _subject = "foo";
var _index = 1;
var _last_match_info = new Array();
%RegExpExecRT(_regexp, _subject, _index, _last_match_info);
