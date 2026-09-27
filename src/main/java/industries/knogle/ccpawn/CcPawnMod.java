package industries.knogle.ccpawn;

import com.mojang.logging.LogUtils;
import dan200.computercraft.api.ComputerCraftAPI;
import net.neoforged.fml.common.Mod;
import org.slf4j.Logger;

@Mod(CcPawnMod.MOD_ID)
public final class CcPawnMod {
    public static final String MOD_ID = "ccpawn";
    public static final Logger LOG = LogUtils.getLogger();

    public CcPawnMod() {
        ComputerCraftAPI.registerAPIFactory(computer -> new PawnApi(computer::queueEvent));
        LOG.info("PawnCraft registered its server-side ComputerCraft API");
    }
}
