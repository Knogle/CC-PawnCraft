#include <computercraft>
#pragma rational Float

main()
{
    cc_redstone_set_output("back", true);
    cc_redstone_set_output("back", false);
    cc_redstone_set_analog_output("back", 0);
    cc_redstone_set_analog_output("back", 15);
    cc_call("term", "setCursorPos", "ii", -7, 42);
    cc_call("os", "startTimer", "f", 1.5);
    cc_sleep(50);
}
