#include <computercraft>

main()
{
    new remaining = 180;
    while (remaining > 0)
    {
        printf("Noch %d:%02d\n", remaining / 60, remaining % 60);
        new timer = cc_start_timer(1000);
        if (timer < 0 || cc_wait_timer(timer) < 0)
            return 1;
        remaining--;
    }
    cc_println("Countdown fertig!");
    return 0;
}
