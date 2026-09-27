#include <computercraft>

main()
{
    print("Redstone controller started\n");
    while (true)
    {
        if (cc_redstone_get_input("right"))
        {
            cc_redstone_set_output("back", true);
            cc_sleep(200);
            cc_redstone_set_output("back", false);
        }
        cc_sleep(50);
    }
}
