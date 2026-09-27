#include <computercraft>

// Annihilator bamboo harvester, adapted from the maintainer's in-game script.
// Setup and limitations: docs/showcases/annihilator.md
#define STATE_RUNNING   1
#define STATE_SWITCHING 2

#define INPUT  "right"
#define OUTPUT "left"
#define MAX_RUNNING_LOOPS 75

new status = STATE_RUNNING;
new bool:vDirection = false;
new stuckTimer = 0; // Loop counter, NOT elapsed seconds or a CC timer ID.

main()
{
    for (;;)
    {
        if (cc_sleep(1000) < 0)
            return 1;

        stuckTimer++;
        if (printf("Loop count: %d\n", stuckTimer) < 0)
            return 1;

        switch (status)
        {
            case STATE_RUNNING:
            {
                if (cc_redstone_get_input(INPUT) || stuckTimer >= MAX_RUNNING_LOOPS)
                {
                    status = STATE_SWITCHING;
                    stuckTimer = 0;
                }
            }
            case STATE_SWITCHING:
            {
                // The mechanism interprets this signal as its travel direction.
                vDirection = !vDirection;
                if (cc_redstone_set_output(OUTPUT, vDirection) < 0)
                    return 1;

                if (cc_sleep(10000) < 0)
                    return 1;
                status = STATE_RUNNING;
            }
        }

        if (cc_sleep(500) < 0)
            return 1;
    }
}
