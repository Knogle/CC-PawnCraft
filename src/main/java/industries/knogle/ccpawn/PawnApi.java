package industries.knogle.ccpawn;

import dan200.computercraft.api.lua.IArguments;
import dan200.computercraft.api.lua.ILuaAPI;
import dan200.computercraft.api.lua.LuaException;
import dan200.computercraft.api.lua.LuaFunction;
import org.jspecify.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;
import java.util.regex.Pattern;
import java.util.function.BiConsumer;

public final class PawnApi implements ILuaAPI {
    private static final Logger LOG = LoggerFactory.getLogger(PawnApi.class);
    private static final int MAX_SOURCE_SIZE = 256 * 1024;
    private static final int MAX_PROGRAM_SIZE = 1024 * 1024;
    private static final int MAX_COMPILER_OUTPUT = 128 * 1024;
    private static final int MAX_PROCESSES = 4;
    private static final Duration COMPILE_TIMEOUT = Duration.ofSeconds(10);
    private static final Duration RUN_SLICE = Duration.ofSeconds(2);
    private static final Pattern INCLUDE = Pattern.compile(
        "(?m)^\\s*#\\s*(?:tryinclude|include)\\s*(?:<([^>]+)>|\\\"([^\\\"]+)\\\")\\s*$"
    );
    private static final Pattern INCLUDE_LINE = Pattern.compile("(?m)^\\s*#\\s*(?:tryinclude|include)\\b.*$");
    private static final ExecutorService WORKERS = Executors.newVirtualThreadPerTaskExecutor();

    private final BiConsumer<String, Object[]> events;
    private final AtomicLong identifiers = new AtomicLong();
    private final Map<Long, CompletableFuture<CompileResult>> compilations = new ConcurrentHashMap<>();
    private final Map<Long, RunnerProcess> processes = new ConcurrentHashMap<>();

    public PawnApi(BiConsumer<String, Object[]> events) {
        this.events = events;
    }

    @Override
    public String[] getNames() {
        return new String[] { "ccpawn_native" };
    }

    @Override
    public @Nullable String getModuleName() {
        return "ccpawn.native";
    }

    @LuaFunction
    public final long compile(IArguments arguments) throws LuaException {
        byte[] source = copyBytes(arguments, 0, MAX_SOURCE_SIZE, "PAWN source");
        validateSource(source);
        long identifier = identifiers.incrementAndGet();
        var future = CompletableFuture.supplyAsync(() -> compileSource(source), WORKERS);
        compilations.put(identifier, future);
        future.whenComplete((ignored, error) -> events.accept("ccpawn_compile", new Object[] { identifier }));
        return identifier;
    }

    @LuaFunction
    public final Object[] compileResult(long identifier) throws LuaException {
        var future = compilations.get(identifier);
        if (future == null) throw new LuaException("unknown compile job " + identifier);
        if (!future.isDone()) return new Object[] { false };
        compilations.remove(identifier);
        CompileResult result;
        try {
            result = future.join();
        } catch (RuntimeException error) {
            return new Object[] { true, false, null, "compiler worker failed: " + error.getMessage() };
        }
        return new Object[] { true, result.success(), result.program(), result.diagnostics() };
    }

    @LuaFunction
    public final long start(IArguments arguments) throws LuaException {
        byte[] program = copyBytes(arguments, 0, MAX_PROGRAM_SIZE, "AMX program");
        long running = processes.values().stream().filter(RunnerProcess::isAlive).count();
        if (running >= MAX_PROCESSES) throw new LuaException("at most " + MAX_PROCESSES + " PAWN processes may run per computer");
        try {
            long identifier = identifiers.incrementAndGet();
            var process = new RunnerProcess(identifier, program);
            processes.put(identifier, process);
            process.startReaders();
            return identifier;
        } catch (IOException error) {
            throw new LuaException("cannot start PAWN: " + error.getMessage());
        }
    }

    @LuaFunction
    public final @Nullable String take(long identifier) throws LuaException {
        return requireProcess(identifier).messages.poll();
    }

    @LuaFunction
    public final void respond(long identifier, long sequence, String response) throws LuaException {
        if (response.length() > MAX_PROGRAM_SIZE || response.indexOf('\n') >= 0 || response.indexOf('\r') >= 0) {
            throw new LuaException("invalid PAWN bridge response");
        }
        requireProcess(identifier).respond(sequence, response);
    }

    @LuaFunction
    public final boolean isAlive(long identifier) throws LuaException {
        return requireProcess(identifier).isAlive();
    }

    @LuaFunction
    public final void close(long identifier) throws LuaException {
        var process = processes.remove(identifier);
        if (process == null) throw new LuaException("unknown PAWN process " + identifier);
        process.close();
    }

    @Override
    public void shutdown() {
        processes.values().forEach(RunnerProcess::close);
        processes.clear();
        compilations.clear();
    }

    private RunnerProcess requireProcess(long identifier) throws LuaException {
        var process = processes.get(identifier);
        if (process == null) throw new LuaException("unknown PAWN process " + identifier);
        return process;
    }

    private static byte[] copyBytes(IArguments arguments, int index, int maximum, String description) throws LuaException {
        ByteBuffer buffer = arguments.getBytes(index);
        if (buffer.remaining() > maximum) throw new LuaException(description + " exceeds " + maximum + " bytes");
        byte[] copy = new byte[buffer.remaining()];
        buffer.get(copy);
        return copy;
    }

    private static void validateSource(byte[] bytes) throws LuaException {
        final String source;
        try {
            source = StandardCharsets.UTF_8.newDecoder()
                .onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT)
                .decode(ByteBuffer.wrap(bytes)).toString();
        } catch (CharacterCodingException error) {
            throw new LuaException("PAWN source must be UTF-8");
        }
        var allLines = INCLUDE_LINE.matcher(source);
        var allowed = INCLUDE.matcher(source);
        int includeLines = 0;
        while (allLines.find()) includeLines++;
        int validLines = 0;
        while (allowed.find()) {
            validLines++;
            String name = allowed.group(1) == null ? allowed.group(2) : allowed.group(1);
            if (!NativeTools.INCLUDES.contains(name)) {
                throw new LuaException("only bundled PawnCraft includes are allowed: " + NativeTools.INCLUDES);
            }
        }
        if (includeLines != validLines) throw new LuaException("invalid or unsafe #include directive");
    }

    private static CompileResult compileSource(byte[] source) {
        Path directory = null;
        Process process = null;
        try {
            NativeTools tools = NativeTools.get();
            directory = Files.createTempDirectory("ccpawn-compile-");
            Path input = directory.resolve("program.pwn");
            Path output = directory.resolve("program.amx");
            Files.write(input, source);
            var builder = new ProcessBuilder(
                tools.compiler.toString(), input.toString(), "-o" + output,
                "-i" + tools.includeDirectory, "-d2"
            ).directory(directory.toFile()).redirectErrorStream(true);
            builder.environment().clear();
            builder.environment().put("LC_ALL", "C");
            process = builder.start();
            Process activeProcess = process;
            var outputReader = CompletableFuture.supplyAsync(
                () -> readLimited(activeProcess.getInputStream(), MAX_COMPILER_OUTPUT), WORKERS
            );
            if (!process.waitFor(COMPILE_TIMEOUT.toMillis(), TimeUnit.MILLISECONDS)) {
                process.destroyForcibly();
                return new CompileResult(false, null, "compiler exceeded the 10 second limit");
            }
            String diagnostics = outputReader.join();
            if (process.exitValue() != 0 || !Files.isRegularFile(output)) {
                return new CompileResult(false, null, diagnostics.isBlank() ? "compiler failed" : diagnostics);
            }
            byte[] program = Files.readAllBytes(output);
            if (program.length > MAX_PROGRAM_SIZE) {
                return new CompileResult(false, null, "compiled AMX exceeds " + MAX_PROGRAM_SIZE + " bytes");
            }
            return new CompileResult(true, program, diagnostics);
        } catch (Exception error) {
            if (process != null) process.destroyForcibly();
            return new CompileResult(false, null, error.getClass().getSimpleName() + ": " + error.getMessage());
        } finally {
            deleteCompilationDirectory(directory);
        }
    }

    private static String readLimited(InputStream input, int maximum) {
        try (input; var output = new ByteArrayOutputStream(Math.min(maximum, 8192))) {
            byte[] buffer = new byte[4096];
            int total = 0;
            int count;
            while ((count = input.read(buffer)) >= 0) {
                if (total < maximum) {
                    int kept = Math.min(count, maximum - total);
                    output.write(buffer, 0, kept);
                    total += kept;
                }
            }
            String text = output.toString(StandardCharsets.UTF_8);
            return total >= maximum ? text + "\n[compiler output truncated]" : text;
        } catch (IOException error) {
            return "cannot read process output: " + error.getMessage();
        }
    }

    private static void deleteCompilationDirectory(@Nullable Path directory) {
        if (directory == null) return;
        try { Files.deleteIfExists(directory.resolve("program.pwn")); } catch (IOException ignored) { }
        try { Files.deleteIfExists(directory.resolve("program.amx")); } catch (IOException ignored) { }
        try { Files.deleteIfExists(directory.resolve("program.asm")); } catch (IOException ignored) { }
        try { Files.deleteIfExists(directory); } catch (IOException ignored) { }
    }

    private record CompileResult(boolean success, @Nullable byte[] program, String diagnostics) { }

    private final class RunnerProcess {
        private final long identifier;
        private final Path directory;
        private final Path programFile;
        private final Process process;
        private final Writer input;
        private final LinkedBlockingQueue<String> messages = new LinkedBlockingQueue<>(32);
        private final AtomicLong pendingSequence = new AtomicLong(-1);
        private volatile boolean waitingForResponse;
        private volatile long lastProgress = System.nanoTime();

        private RunnerProcess(long identifier, byte[] program) throws IOException {
            this.identifier = identifier;
            NativeTools tools = NativeTools.get();
            directory = Files.createTempDirectory("ccpawn-run-");
            programFile = directory.resolve("program.amx");
            Files.write(programFile, program);
            var builder = new ProcessBuilder(tools.runner.toString(), programFile.toString())
                .directory(directory.toFile());
            builder.environment().clear();
            process = builder.start();
            input = new OutputStreamWriter(process.getOutputStream(), StandardCharsets.US_ASCII);
        }

        private void startReaders() {
            WORKERS.execute(this::readProtocol);
            WORKERS.execute(() -> {
                String errors = readLimited(process.getErrorStream(), MAX_COMPILER_OUTPUT);
                if (!errors.isBlank()) LOG.warn("PAWN process {} stderr: {}", identifier, errors.trim());
            });
            WORKERS.execute(this::watchdog);
        }

        private void readProtocol() {
            try (var reader = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.US_ASCII))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    lastProgress = System.nanoTime();
                    if (line.startsWith("CALL\t")) {
                        String[] fields = line.split("\\t", 4);
                        if (fields.length < 4) throw new IOException("malformed CALL from runner");
                        pendingSequence.set(Long.parseLong(fields[1]));
                        waitingForResponse = true;
                    }
                    messages.put(line);
                    events.accept("ccpawn_process", new Object[] { identifier });
                }
                int exitCode = process.waitFor();
                messages.put("PROCESS_EXIT\t" + exitCode);
                events.accept("ccpawn_process", new Object[] { identifier });
            } catch (Exception error) {
                messages.offer("PROCESS_ERROR\t" + safeMessage(error));
                events.accept("ccpawn_process", new Object[] { identifier });
            } finally {
                cleanupFiles();
            }
        }

        private synchronized void respond(long sequence, String response) throws LuaException {
            if (!process.isAlive()) throw new LuaException("PAWN process has exited");
            if (!waitingForResponse || pendingSequence.get() != sequence) {
                throw new LuaException("PAWN process is not waiting for response " + sequence);
            }
            if (!(response.startsWith("RETURN\t" + sequence + "\t") || response.startsWith("ERROR\t" + sequence + "\t"))) {
                throw new LuaException("response sequence/type does not match pending call");
            }
            try {
                // Retire this call before the child can send its next request.
                pendingSequence.set(-1);
                lastProgress = System.nanoTime();
                waitingForResponse = false;
                input.write(response);
                input.write('\n');
                input.flush();
            } catch (IOException error) {
                throw new LuaException("cannot answer PAWN process: " + error.getMessage());
            }
        }

        private void watchdog() {
            try {
                while (process.isAlive()) {
                    Thread.sleep(250);
                    if (!waitingForResponse && System.nanoTime() - lastProgress > RUN_SLICE.toNanos()) {
                        messages.offer("WATCHDOG\tPAWN code ran for over 2 seconds without a CC call");
                        events.accept("ccpawn_process", new Object[] { identifier });
                        process.destroyForcibly();
                        return;
                    }
                }
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
        }

        private boolean isAlive() {
            return process.isAlive();
        }

        private void close() {
            try { input.close(); } catch (IOException ignored) { }
            if (process.isAlive()) process.destroyForcibly();
            cleanupFiles();
        }

        private void cleanupFiles() {
            try { Files.deleteIfExists(programFile); } catch (IOException ignored) { }
            try { Files.deleteIfExists(directory); } catch (IOException ignored) { }
        }

        private String safeMessage(Exception error) {
            String message = error.getMessage();
            if (message == null) message = error.getClass().getSimpleName();
            return message.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
        }
    }
}
