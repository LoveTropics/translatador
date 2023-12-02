package org.lovetropics.translatador;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Set;

class TranslatadorNative {
    static {
        Loader.load("translatador-java");
    }

    public static native long createModel(String yamlConfig, byte[] model, byte[] sourceVocab, byte[] targetVocab, byte[] shortList) throws ModelException;

    public static native long cloneModel(long model);

    public static native void destroyModel(long model);

    public static native String[] getBatchStrings(long batch);

    public static native void destroyBatch(long batch);

    public static native long translate(long model, long batch) throws TranslationException;

    public static native long translatePlain(long model, String[] batch) throws TranslationException;

    private static class Loader {
        private static final Path UNPACK_ROOT = prepareUnpackRoot();

        private static Path prepareUnpackRoot() {
            final Path path = Path.of(System.getProperty("java.io.tmpdir")).resolve("translatador-natives");
            if (Files.exists(path)) {
                try {
                    Files.walkFileTree(path, Set.of(), 1, new SimpleFileVisitor<>() {
                        @Override
                        public FileVisitResult visitFile(final Path file, final BasicFileAttributes attrs) throws IOException {
                            Files.delete(file);
                            return FileVisitResult.CONTINUE;
                        }
                    });
                } catch (final IOException ignored) {
                }
            } else {
                try {
                    Files.createDirectory(path);
                } catch (final IOException e) {
                    throw new LinkageError("Could not create directory for native unpacking", e);
                }
            }
            return path;
        }

        public static void load(final String libraryName) {
            final Path path = unpackLibrary(UNPACK_ROOT, libraryName, Platform.detect());
            System.load(path.toAbsolutePath().toString());
            try {
                Files.deleteIfExists(path);
            } catch (final IOException ignored) {
                // Will fail on Windows, but will be cleaned up the next time the application boots
            }
        }

        private static Path unpackLibrary(final Path root, final String libraryName, final Platform platform) {
            final String libraryPath = platform.getLibraryPath(libraryName);
            try (final InputStream input = Loader.class.getResourceAsStream(libraryPath)) {
                if (input == null) {
                    throw new LinkageError("Missing native library at " + libraryPath + " for platform " + platform.classifier);
                }
                final Path path = Files.createTempFile(root, libraryName, null);
                Files.copy(input, path, StandardCopyOption.REPLACE_EXISTING);
                return path;
            } catch (final IOException e) {
                throw new LinkageError("Unable to load native library at " + libraryPath, e);
            }
        }
    }
}
