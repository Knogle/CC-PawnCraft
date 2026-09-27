#include <computercraft>
#include <ccjson>

main()
{
    new document = cc_json_parse("{\"machine\":{\"active\":true},\"items\":[{\"count\":12}],\"empty\":[],\"value\":null}");
    if (!document) return 1;

    if (cc_json_get(document, "/items/0/count") < 1) return 2;
    new count = cc_result_int(0);
    if (!cc_json_set_int(document, "/items/0/count", count + 1)) return 3;
    if (!cc_json_set_string(document, "/machine/name", "Ernter")) return 4;
    if (!cc_json_set_bool(document, "/machine/active", false)) return 5;
    if (!cc_json_set_json(document, "/items/-", "{\"count\":5}")) return 6;
    if (cc_json_type(document, "/empty") != CC_JSON_ARRAY) return 7;
    if (cc_json_type(document, "/value") != CC_JSON_NULL) return 8;

    new text[512];
    if (cc_json_stringify(document, text) < 0) return 9;
    cc_println(text);
    if (!cc_json_free(document)) return 10;
    return 0;
}
