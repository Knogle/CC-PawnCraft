#include <computercraft>

main()
{
    cc_redstone_set_output("back", cc_redstone_get_input("right"));
    cc_println("Warte auf Redstone-Aenderungen; Ctrl+T beendet das Programm.");
    while (cc_pull_event("redstone") > 0)
    {
        new bool:signal = cc_redstone_get_input("right");
        printf("Rechts: %d\n", signal);
        if (cc_redstone_set_output("back", signal) < 0)
            return 1;
    }
    return 1;
}
