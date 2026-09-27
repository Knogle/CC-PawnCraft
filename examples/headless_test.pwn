#include <computercraft>

main()
{
    print("hello from pawn\n");
    cc_call("redstone", "getInput", "s", "right");
    if (cc_result_bool(0))
        print("redstone is high\n");
}
