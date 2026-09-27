#include <computercraft>

#define EINGANG_1 "right"
#define EINGANG_2 "left"
#define AUSGANG   "back"

main()
{
    new bool:eingeschaltet = true;
    new bool:vorheriges_signal = false;

    if (cc_redstone_set_output(AUSGANG, eingeschaltet) < 0)
        return 1;

    for (;;)
    {
        new bool:signal_1 = cc_redstone_get_input(EINGANG_1);
        new bool:signal_2 = cc_redstone_get_input(EINGANG_2);
        new bool:signal = signal_1 || signal_2;

        if (signal && !vorheriges_signal)
        {
            eingeschaltet = !eingeschaltet;
            if (cc_redstone_set_output(AUSGANG, eingeschaltet) < 0)
                return 1;
        }

        vorheriges_signal = signal;
        if (cc_sleep(50) < 0)
            return 1;
    }
}
