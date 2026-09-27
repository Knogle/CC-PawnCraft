package industries.knogle.ccintegration;

import dan200.computercraft.api.filesystem.Mount;
import dan200.computercraft.api.filesystem.WritableMount;
import dan200.computercraft.api.lua.ILuaAPI;
import dan200.computercraft.api.lua.LuaFunction;
import dan200.computercraft.api.peripheral.IPeripheral;
import dan200.computercraft.core.ComputerContext;
import dan200.computercraft.core.computer.Computer;
import dan200.computercraft.core.computer.ComputerEnvironment;
import dan200.computercraft.core.computer.ComputerSide;
import dan200.computercraft.core.computer.GlobalEnvironment;
import dan200.computercraft.core.filesystem.MemoryMount;
import dan200.computercraft.core.metrics.MetricsObserver;
import dan200.computercraft.core.terminal.Terminal;
import industries.knogle.ccpawn.PawnApi;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.zip.ZipFile;

/** Runs the real PAWN compiler/VM and actual CraftOS ROM in CC's headless Cobalt core. */
public final class PawnCraftOsTest {
    private static final String COMPILE = "assert(shell.run('pawncc main.pwn -o main.amx'), 'pawncc failed')\n";
    private static final String RUN = "assert(shell.run('pawn main.amx'), 'pawn failed')\n";
    private static int passed;

    public static void main(String[] args) throws Exception {
        run("PAWN-only CraftOS commands", """
            #include <computercraft>
            main() { return 0; }
            """, """
            assert(ccjava_native == nil, 'Java API must not be exposed')
            assert(shell.resolveProgram('jcc') == nil, 'Java compiler command must be absent')
            assert(shell.resolveProgram('jrun') == nil, 'Java runner command must be absent')
            """ + COMPILE + RUN, Action.NONE, false, List.of());

        run("real CC redstone, files and newline output", """
            #include <computercraft>
            main()
            {
                if (!cc_redstone_get_input("right")) return 1;
                if (cc_redstone_set_output("back", true) < 0) return 2;
                if (cc_sleep(150) < 0) return 3;
                if (cc_redstone_set_output("back", false) < 0) return 4;
                if (!cc_fs_write("result.txt", "from PAWN")) return 5;
                if (!cc_fs_write("result.txt", " / CC filesystem", true)) return 6;
                new text[64];
                cc_fs_read("result.txt", text, sizeof text);
                if (!cc_fs_write("copy.txt", text)) return 7;
                print("PAWN bridge OK\\nsecond line\\n");
                return 0;
            }
            """, COMPILE + "term.clear() term.setCursorPos(1, 1)\n" + RUN + """
            assert(not redstone.getOutput('back'), 'false must turn the output off')
            local f = assert(fs.open('result.txt', 'r'))
            assert(f.readAll() == 'from PAWN / CC filesystem') f.close()
            local g = assert(fs.open('copy.txt', 'r'))
            assert(g.readAll() == 'from PAWN / CC filesystem') g.close()
            """, Action.NONE, true, List.of("PAWN bridge OK", "second line"));

        run("compiler errors remain visible to CraftOS", """
            #include <computercraft>
            main() { this is not valid Pawn syntax }
            """, "assert(not shell.run('pawncc main.pwn -o main.amx'), 'invalid source must fail')\n"
                + "assert(not fs.exists('main.amx'), 'invalid source produced AMX')\n",
            Action.NONE, false, List.of());

        String waiting = """
            #include <computercraft>
            main()
            {
                cc_fs_write("waiting.txt", "ready");
                cc_call("os", "pullEvent", "s", "event_that_never_arrives");
                cc_fs_write("unexpected.txt", "continued after termination");
                return 0;
            }
            """;
        run("Ctrl+T during a blocking event", waiting,
            COMPILE + "assert(not shell.run('pawn main.amx'), 'terminated PAWN must fail')\n"
                + "assert(not fs.exists('unexpected.txt'), 'PAWN continued after termination')\n",
            Action.TERMINATE, false, List.of());
        run("computer shutdown closes a waiting PAWN process", waiting,
            COMPILE + "shell.run('pawn main.amx') error('shutdown must stop the computer')\n",
            Action.SHUTDOWN, false, List.of());
        run("runner death interrupts a pending CC event", waiting,
            COMPILE + "assert(not shell.run('pawn main.amx'), 'killed PAWN runner must fail')\n"
                + "assert(not fs.exists('unexpected.txt'), 'killed PAWN unexpectedly resumed')\n",
            Action.KILL_RUNNER, false, List.of());
        run("busy PAWN watchdog fails cleanly", """
            #include <computercraft>
            main() { while (true) { } }
            """, COMPILE + "assert(not shell.run('pawn main.amx'), 'busy loop must hit watchdog')\n",
            Action.NONE, false, List.of());

        run("standard strings, format and floating point", """
            #include <computercraft>
            #include <string>
            #include <float>
            #include <time>
            #include <console>
            #include <core>
            main()
            {
                new text[128];
                strcopy(text, "Signal");
                strcat(text, " fertig");
                if (strlen(text) != 13 || strcmp(text, "Signal fertig") != 0) return 1;
                if (strfind(text, "fertig") != 7) return 2;
                if (strval("-42") != -42) return 3;
                if (min(8, 3) != 3 || max(8, 3) != 8 || clamp(23, 0, 15) != 15) return 4;
                new Float:value = 1.25 + 2.5;
                if (value < 3.74 || value > 3.76 || floatround(value) != 4) return 5;
                if (strformat(text, sizeof text, false, "Signal %d / %.2f", 42, value) < 0) return 6;
                if (strcmp(text, "Signal 42 / 3.75") != 0) return 7;
                if (!cc_fs_write("formatted.txt", text)) return 8;
                printf("Signal %d / %.2f\\n", 42, value);
                printf("Bool %d; String %s\\n", false, "fertig");
                return 0;
            }
            """, COMPILE + "term.clear() term.setCursorPos(1, 1)\n" + RUN + """
            local f = assert(fs.open('formatted.txt', 'r'))
            assert(f.readAll() == 'Signal 42 / 3.75') f.close()
            """, Action.NONE, false, List.of("Signal 42 / 3.75", "Bool 0; String fertig"));

        run("typed event snapshot survives other CC calls", """
            #include <computercraft>
            main()
            {
                if (cc_call("os", "queueEvent", "sisbf", "test_event", -7, "payload", false, 1.25) < 0) return 1;
                if (cc_pull_event("test_event") != 5) return 2;
                printf("Debug output must not destroy the event\\n");
                new name[64], value[64];
                cc_event_name(name, sizeof name);
                cc_event_string(2, value, sizeof value);
                if (strcmp(name, "test_event") != 0 || strcmp(value, "payload") != 0) return 3;
                if (cc_event_int(1) != -7 || cc_event_bool(3)) return 4;
                new Float:number = cc_event_float(4);
                if (number < 1.24 || number > 1.26) return 5;
                return 0;
            }
            """, COMPILE + RUN, Action.NONE, false, List.of());

        run("CC timers, delay and UTC calendar", """
            #include <computercraft>
            main()
            {
                new granularity, before = tickcount(granularity);
                if (granularity != 50) return 1;
                if (delay(100) < 0) return 2;
                if (tickcount() - before < 50) return 3;
                new timer = cc_start_timer(100);
                if (timer < 0 || cc_wait_timer(timer) < 0) return 4;
                new compatibility_timer = settimer(100, true);
                if (compatibility_timer < 0 || cc_wait_timer(compatibility_timer) < 0) return 5;
                new repeating = settimer(100, false);
                if (repeating < 0 || cc_wait_timer(repeating) < 0) return 11;
                if (cc_wait_timer(repeating) < 0) return 12;
                if (cc_cancel_timer(repeating) < 0) return 13;
                new cancelled = cc_start_timer(10000);
                if (cancelled < 0 || cc_cancel_timer(cancelled) < 0) return 6;
                new hour, minute, second, year, month, day;
                new timestamp = gettime(hour, minute, second);
                new yearday = getdate(year, month, day);
                if (timestamp < 1600000000 || hour < 0 || hour > 23) return 7;
                if (minute < 0 || minute > 59 || second < 0 || second > 60) return 8;
                if (year < 2020 || month < 1 || month > 12 || day < 1 || day > 31) return 9;
                if (yearday < 1 || yearday > 366) return 10;
                return 0;
            }
            """, COMPILE + RUN, Action.NONE, false, List.of());

        run("real peripheral dispatch and JSON table roundtrip", """
            #include <computercraft>
            main()
            {
                new device[64], type[64], json[1024];
                if (cc_peripheral_find("chatBox", device, sizeof device) < 1) return 1;
                if (strcmp(device, "left") != 0) return 2;
                if (!cc_peripheral_is_present(device) || !cc_peripheral_has_type(device, "chatBox")) return 3;
                if (cc_peripheral_is_present("right")) return 4;
                cc_peripheral_get_type(device, type, sizeof type);
                if (strcmp(type, "chatBox") != 0) return 5;
                if (cc_peripheral_call(device, "sendMessage", "s", "Ernter fertig!") < 1) return 6;
                if (!cc_result_bool(0)) return 7;
                if (cc_peripheral_call(device, "getData", "") < 1) return 8;
                if (cc_result_type(0) != CC_RESULT_JSON) return 9;
                cc_result_string(0, json, sizeof json);
                new document = cc_json_parse(json);
                if (!document) return 10;
                if (cc_json_get(document, "/energy") != 1 || cc_result_int(0) != 1234) return 11;
                if (cc_json_get(document, "/active") != 1 || cc_result_bool(0)) return 12;
                if (cc_json_get(document, "/inventory/0/count") != 1 || cc_result_int(0) != 32) return 13;
                if (cc_json_get(document, "/ratio") != 1) return 14;
                new Float:ratio = cc_result_float(0);
                if (ratio < 0.74 || ratio > 0.76) return 15;
                if (cc_json_type(document, "/inventory") != CC_JSON_ARRAY) return 16;
                if (!cc_json_set_string(document, "/name", "changed by PAWN")) return 17;
                if (!cc_json_set_int(document, "/inventory/0/count", 64)) return 18;
                if (!cc_json_set_null(document, "/empty")) return 19;
                if (cc_json_get(document, "/empty") != 1 || cc_result_type(0) != CC_RESULT_NIL) return 20;
                if (cc_json_get(document, "/missing") >= 0) return 21;
                if (cc_json_stringify(document, json, sizeof json) < 0) return 22;
                if (!cc_fs_write("json.txt", json)) return 23;
                if (!cc_json_free(document)) return 24;
                if (cc_json_get(document, "/name") >= 0) return 25;
                return 0;
            }
            """, COMPILE + RUN + """
            local f = assert(fs.open('json.txt', 'r'))
            local data = assert(textutils.unserialiseJSON(f.readAll(), { parse_null = true })) f.close()
            assert(data.name == 'changed by PAWN' and data.energy == 1234 and data.active == false)
            assert(data.inventory[1].count == 64 and data.empty == textutils.json_null)
            """, Action.NONE, false, List.of());

        System.out.println("PASS: " + passed + " real CraftOS PAWN integration tests");
    }

    private enum Action { NONE, TERMINATE, SHUTDOWN, KILL_RUNNER }

    private static void run(String name, String source, String script, Action action,
            boolean expectRedstone, List<String> expectedLines) throws Exception {
        System.out.println("CraftOS PAWN TEST " + name);
        MemoryMount disk = new MemoryMount().addFile("main.pwn", source).addFile("test.lua", script)
            .addFile("startup.lua", "local ok, message = pcall(assert(loadfile('test.lua', nil, _ENV))) "
                + "testresult.finish(ok, tostring(message)) os.shutdown()");
        var environment = new Environment(disk);
        var terminal = new Terminal(51, 19, true);
        var context = ComputerContext.builder(environment).build();
        var computer = new Computer(context, environment, terminal, 0);
        var result = new ResultApi(terminal);
        var pawnApi = new PawnApi(computer::queueEvent);
        computer.addApi(pawnApi);
        computer.addApi(result);
        computer.getRedstone().setInput(ComputerSide.RIGHT, 15, 0);
        computer.getEnvironment().setPeripheral(ComputerSide.LEFT, new TestPeripheral());
        boolean interrupted = false;
        int waitingTicks = 0;
        boolean seenOn = false;
        boolean backWasOn = false;
        try {
            computer.turnOn();
            for (int tick = 0; tick < 600; tick++) {
                computer.tick();
                backWasOn |= computer.getRedstone().getOutput(ComputerSide.BACK) == 15;
                seenOn |= computer.isOn();
                if (!interrupted && action != Action.NONE && disk.exists("waiting.txt")) {
                    // Allow the native runner to enter its blocking CC call before simulating a crash.
                    waitingTicks++;
                    if (action == Action.KILL_RUNNER && waitingTicks < 5) {
                        Thread.sleep(50);
                        continue;
                    }
                    if (action == Action.SHUTDOWN) computer.shutdown();
                    else if (action == Action.KILL_RUNNER) killRunner();
                    else computer.queueEvent("terminate", null);
                    interrupted = true;
                }
                if (seenOn && !computer.isOn()) break;
                Thread.sleep(50);
            }
            if (computer.isOn() || (action != Action.SHUTDOWN && (!result.finished || !result.success))
                    || (action != Action.NONE && !interrupted)) {
                dump(terminal);
                for (String line : result.lines) System.err.println(line);
                throw new AssertionError(name + ": " + result.message);
            }
            if (expectRedstone && !backWasOn) throw new AssertionError("real redstone output never became high");
            for (int line = 0; line < expectedLines.size(); line++) {
                String actual = result.lines.get(line).stripTrailing();
                if (!actual.equals(expectedLines.get(line))) {
                    for (String savedLine : result.lines) System.err.println(savedLine);
                    throw new AssertionError("line " + (line + 1) + " expected " + expectedLines.get(line) + ", got " + actual);
                }
            }
        } finally {
            computer.shutdown();
            pawnApi.shutdown();
            context.ensureClosed(3, TimeUnit.SECONDS);
            long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(3);
            while (hasNativeChildren() && System.nanoTime() < deadline) Thread.sleep(20);
            if (hasNativeChildren()) throw new AssertionError(name + ": native process survived computer shutdown");
        }
        passed++;
        System.out.println("  PASS");
    }

    private static void killRunner() {
        // Only this test JVM's uniquely identified PAWN runner child is eligible.
        var runners = ProcessHandle.current().descendants().filter(ProcessHandle::isAlive)
            .filter(process -> process.info().command().map(command -> command.endsWith("/ccpawn-runner")).orElse(false)).toList();
        if (runners.size() != 1) throw new AssertionError("expected exactly one test-owned PAWN runner, got " + runners.size());
        if (!runners.getFirst().destroyForcibly()) throw new AssertionError("could not kill test-owned PAWN runner");
    }

    private static boolean hasNativeChildren() {
        return ProcessHandle.current().descendants().anyMatch(process -> process.isAlive()
            && process.info().command().map(command -> command.endsWith("/ccpawn-runner") || command.endsWith("/ccpawn-pawncc")).orElse(false));
    }

    private static void dump(Terminal terminal) {
        for (int line = 0; line < terminal.getHeight(); line++) System.err.println(terminal.getLine(line));
    }

    public static final class ResultApi implements ILuaAPI {
        private final Terminal terminal;
        volatile List<String> lines = List.of();
        ResultApi(Terminal terminal) { this.terminal = terminal; }
        volatile boolean finished;
        volatile boolean success;
        volatile String message = "no completion";
        @Override public String[] getNames() { return new String[] { "testresult" }; }
        @LuaFunction public void finish(boolean success, String message) {
            // CC clears the terminal on shutdown, so capture it before startup.lua calls os.shutdown().
            lines = java.util.stream.IntStream.range(0, terminal.getHeight()).mapToObj(line -> terminal.getLine(line).toString()).toList();
            this.success = success; this.message = message; this.finished = true;
        }
    }

    /** Test device only: exercises CC's real peripheral dispatcher, not the AP mod itself. */
    public static final class TestPeripheral implements IPeripheral {
        @Override public String getType() { return "chatBox"; }
        @Override public boolean equals(IPeripheral other) { return this == other; }
        @LuaFunction public boolean sendMessage(String message) { return message.equals("Ernter fertig!"); }
        @LuaFunction public Map<String, Object> getData() {
            return Map.of("energy", 1234, "active", false, "name", "test machine", "ratio", 0.75,
                "inventory", List.of(Map.of("name", "minecraft:stone", "count", 32)));
        }
    }

    private static final class Environment implements GlobalEnvironment, ComputerEnvironment {
        private final MemoryMount disk;
        private final MemoryMount rom = new MemoryMount();
        Environment(MemoryMount disk) throws Exception {
            this.disk = disk;
            Path core = Path.of(ComputerContext.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            String prefix = "data/computercraft/lua/rom/";
            try (var zip = new ZipFile(core.toFile())) {
                var entries = zip.entries();
                while (entries.hasMoreElements()) {
                    var entry = entries.nextElement();
                    if (!entry.isDirectory() && entry.getName().startsWith(prefix)) {
                        try (var input = zip.getInputStream(entry)) {
                            rom.addFile(entry.getName().substring(prefix.length()), input.readAllBytes());
                        }
                    }
                }
            }
            Path overlay = Path.of("src/main/resources/data/computercraft/lua/rom");
            try (var files = Files.walk(overlay)) {
                for (Path file : files.filter(Files::isRegularFile).toList()) {
                    rom.addFile(overlay.relativize(file).toString(), Files.readAllBytes(file));
                }
            }
        }
        @Override public String getHostString() { return "ComputerCraft 1.120.2 (CC PAWN local integration)"; }
        @Override public String getUserAgent() { return "CCPawnLocalTest"; }
        @Override public Mount createResourceMount(String domain, String path) {
            return domain.equals("computercraft") && path.equals("lua/rom") ? rom : null;
        }
        @Override public InputStream createResourceFile(String domain, String path) {
            return ComputerContext.class.getClassLoader().getResourceAsStream("data/" + domain + "/" + path);
        }
        @Override public int getDay() { return 1; }
        @Override public double getTimeOfDay() { return 12; }
        @Override public MetricsObserver getMetrics() { return MetricsObserver.discard(); }
        @Override public WritableMount createRootMount() { return disk; }
    }
}
