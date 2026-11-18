# cJSON provenance

Upstream: https://github.com/DaveGamble/cJSON
Tag v1.7.19, commit `c859b25da02955fef659d658b8f324b5cde87be3`.
MIT license in LICENSE. cJSON.c/h are unmodified upstream files.
Build sets CJSON_NESTING_LIMIT=8 and CJSON_HIDE_SYMBOLS.
Local tv4_namespace.h prefixes public symbols with tv4_ota_json_, avoiding SDK/global cJSON hook collisions.
No global hooks are installed; input/token/UTF-8 limits apply before parsing.

Upstream cJSON.h:264 has a trailing space; LICENSE has a final blank line.
These upstream whitespace details are intentionally retained with the exact file hashes.
