package industries.knogle.ccpawn;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.PosixFilePermission;
import java.util.EnumSet;
import java.util.Set;

final class NativeTools {
    static final Set<String> INCLUDES = Set.of("default", "computercraft", "ccstdlib",
        "float", "string", "core", "console", "ccevents", "ccperipheral", "ccjson", "time");

    private static final Set<PosixFilePermission> EXECUTABLE = EnumSet.of(
        PosixFilePermission.OWNER_READ, PosixFilePermission.OWNER_WRITE,
        PosixFilePermission.OWNER_EXECUTE
    );

    private static volatile NativeTools instance;

    final Path runner;
    final Path compiler;
    final Path includeDirectory;
    private final Path temporaryDirectory;

    private NativeTools(Path runner, Path compiler, Path includeDirectory, Path temporaryDirectory) {
        this.runner = runner;
        this.compiler = compiler;
        this.includeDirectory = includeDirectory;
        this.temporaryDirectory = temporaryDirectory;
    }

    static NativeTools get() throws IOException {
        var current = instance;
        if (current != null) return current;
        synchronized (NativeTools.class) {
            if (instance == null) instance = extract();
            return instance;
        }
    }

    private static NativeTools extract() throws IOException {
        var override = System.getProperty("ccpawn.nativeDir");
        if (override != null && !override.isBlank()) {
            var directory = Path.of(override).toAbsolutePath().normalize();
            return new NativeTools(
                requireFile(directory.resolve("ccpawn-runner")),
                requireFile(directory.resolve("ccpawn-pawncc")),
                requireDirectory(directory.resolve("include")), null
            );
        }

        var os = System.getProperty("os.name", "").toLowerCase();
        var architecture = System.getProperty("os.arch", "").toLowerCase();
        if (!os.contains("linux") || !(architecture.equals("amd64") || architecture.equals("x86_64"))) {
            throw new IOException("PawnCraft contains native tools for Linux x86_64 only; " +
                "set -Dccpawn.nativeDir=/path/to/tools for this platform");
        }

        var directory = Files.createTempDirectory("ccpawn-native-");
        var include = Files.createDirectories(directory.resolve("include"));
        var runner = extractResource("/ccpawn-native/linux-x86_64/ccpawn-runner", directory.resolve("ccpawn-runner"), true);
        var compiler = extractResource("/ccpawn-native/linux-x86_64/ccpawn-pawncc", directory.resolve("ccpawn-pawncc"), true);
        for (String name : INCLUDES) {
            extractResource("/ccpawn-native/include/" + name + ".inc", include.resolve(name + ".inc"), false);
        }
        return new NativeTools(runner, compiler, include, directory);
    }

    private static Path extractResource(String resource, Path target, boolean executable) throws IOException {
        try (InputStream input = NativeTools.class.getResourceAsStream(resource)) {
            if (input == null) throw new IOException("missing bundled resource " + resource);
            Files.copy(input, target);
        }
        if (executable) Files.setPosixFilePermissions(target, EXECUTABLE);
        target.toFile().deleteOnExit();
        return target;
    }

    private static Path requireFile(Path path) throws IOException {
        if (!Files.isRegularFile(path) || !Files.isExecutable(path)) {
            throw new IOException("native executable is missing or not executable: " + path);
        }
        return path;
    }

    private static Path requireDirectory(Path path) throws IOException {
        if (!Files.isDirectory(path)) throw new IOException("include directory is missing: " + path);
        return path;
    }
}
