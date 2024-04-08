package org.lovetropics.translatador;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.locks.StampedLock;

/**
 * Translatador is a minimal wrapper around <a href="https://browser.mt/">Bergamot Translator</a>, which allows
 * lightweight client-side translation of text.
 * <p>
 * Mozilla provides open source translation models for use in Firefox that can be found <a href="https://github.com/mozilla/firefox-translations-models">here</a>,
 * and can be directly used with Translatador.
 *
 * @see Translatador#builder()
 * @see TranslationModel
 * @see PivotedTranslationModel
 */
public class Translatador {
    private Translatador() {
    }

    /**
     * @return a new {@link TranslationModel} builder
     */
    public static Builder builder() {
        return new Builder();
    }

    /**
     * @return a new {@link LanguageDetector}
     */
    public static LanguageDetector createLanguageDetector() {
        TranslatadorNative.checkLoaded();
        return new NativeLanguageDetector();
    }

    /**
     * Detects whether the current operating system and architecture is supported by Translatador.
     *
     * @return {@code true} if the current platform is supported by Translatador
     */
    public static boolean isSupportedPlatform() {
        return Platform.tryDetect() != null;
    }

    public static class Builder {
        private String yamlConfig;
        private byte[] model;
        private byte[] sourceVocab;
        private byte[] targetVocab;
        private byte[] shortList;

        /**
         * Sets the optional Bergamot YAML configuration to be used to load this model with.
         * <p>
         * By default, uses the <a href="https://github.com/mozilla/firefox-translations-models/blob/main/evals/translators/bergamot.config.yml">default parameters used by firefox-translations-models</a>.
         *
         * @param yamlConfig YAML formatted configuration string
         * @return this {@link Builder}
         */
        public Builder yamlConfig(final String yamlConfig) {
            this.yamlConfig = yamlConfig;
            return this;
        }

        /**
         * Sets the required model binary to load.
         *
         * @param model the model binary
         * @return this {@link Builder}
         */
        public Builder model(final byte[] model) {
            this.model = model;
            return this;
        }

        /**
         * Sets the required model binary to load from a {@link Path}.
         *
         * @param model the model binary path to load from
         * @return this {@link Builder}
         * @throws IOException if an I/O error occurs reading from the file
         */
        public Builder model(final Path model) throws IOException {
            return model(Files.readAllBytes(model));
        }

        /**
         * Sets the required model vocabulary set binary to load, with distinct source / target vocabularies.
         *
         * @param sourceVocab the vocabulary binary of the source language
         * @param targetVocab the vocabulary binary of the target language
         * @return this {@link Builder}
         */
        public Builder vocab(final byte[] sourceVocab, final byte[] targetVocab) {
            this.sourceVocab = sourceVocab;
            this.targetVocab = targetVocab;
            return this;
        }

        /**
         * Sets the required model vocabulary set binary to load from {@link Path}, with distinct source / target vocabularies.
         *
         * @param sourceVocab the vocabulary binary path of the source language
         * @param targetVocab the vocabulary binary path of the target language
         * @return this {@link Builder}
         * @throws IOException if an I/O error occurs reading from the file
         */
        public Builder vocab(final Path sourceVocab, final Path targetVocab) throws IOException {
            return vocab(Files.readAllBytes(sourceVocab), Files.readAllBytes(targetVocab));
        }

        /**
         * Sets the required model vocabulary set binary to load, with a shared source / target vocabulary.
         *
         * @param vocab the shared vocabulary binary of the source and target languages
         * @return this {@link Builder}
         */
        public Builder vocab(final byte[] vocab) {
            return vocab(vocab, vocab);
        }

        /**
         * Sets the required model vocabulary set binary to load from {@link Path}, with a shared source / target vocabulary.
         *
         * @param vocab the shared vocabulary binary path of the source and target languages
         * @return this {@link Builder}
         * @throws IOException if an I/O error occurs reading from the file
         */
        public Builder vocab(final Path vocab) throws IOException {
            return vocab(Files.readAllBytes(vocab));
        }

        /**
         * Sets the optional model short list binary to load.
         *
         * @param shortList the short list binary
         * @return this {@link Builder}
         */
        public Builder shortList(final byte[] shortList) {
            this.shortList = shortList;
            return this;
        }

        /**
         * Sets the optional model short list binary to load from a {@link Path}.
         *
         * @param shortList the short list binary path
         * @return this {@link Builder}
         * @throws IOException if an I/O error occurs reading from the file
         */
        public Builder shortList(final Path shortList) throws IOException {
            return shortList(Files.readAllBytes(shortList));
        }

        /**
         * Loads a {@link TranslationModel} from the given data.
         * <p>
         * This model wraps native resources, and must be {@link AutoCloseable#close() closed} to free its off-heap memory.
         *
         * @return a new {@link TranslationModel}
         * @throws ModelException        if the model was invalid and could not be loaded
         * @throws IllegalStateException if required model or vocabularies are not defined
         */
        public TranslationModel load() throws ModelException {
            if (model == null) {
                throw new IllegalStateException("Missing translation model binary");
            }
            if (sourceVocab == null || targetVocab == null) {
                throw new IllegalStateException("Missing translation model vocabularies");
            }
            return new NativeModel(TranslatadorNative.createModel(yamlConfig, model, sourceVocab, targetVocab, shortList));
        }
    }

    private static class NativeModel implements TranslationModel {
        private long pointer;

        private NativeModel(final long pointer) {
            this.pointer = pointer;
        }

        @Override
        public synchronized TranslationBatch translateBatch(final TranslationBatch batch) {
            final long pointer = checkOpen();
            if (batch instanceof final NativeBatch nativeBatch) {
                final long stamp = nativeBatch.lock.readLock();
                try {
                    final long batchPointer = nativeBatch.checkOpen();
                    return new NativeBatch(TranslatadorNative.translate(pointer, batchPointer));
                } finally {
                    nativeBatch.lock.unlockRead(stamp);
                }
            } else {
                return new NativeBatch(TranslatadorNative.translatePlain(pointer, batch.get()));
            }
        }

        @Override
        public synchronized TranslationModel fork() {
            final long pointer = checkOpen();
            return new NativeModel(TranslatadorNative.cloneModel(pointer));
        }

        @Override
        public synchronized void close() {
            if (pointer != 0) {
                TranslatadorNative.destroyModel(pointer);
                pointer = 0;
            }
        }

        private synchronized long checkOpen() {
            if (pointer == 0) {
                throw new IllegalStateException("Model has already been closed");
            }
            return pointer;
        }

        private static class NativeBatch extends TranslationBatch {
            private long pointer;
            private String[] values;
            private final StampedLock lock = new StampedLock();

            private NativeBatch(final long pointer) {
                this.pointer = pointer;
            }

            @Override
            public String[] get() {
                final long stamp = lock.readLock();
                try {
                    final long pointer = checkOpen();
                    if (values == null) {
                        values = TranslatadorNative.getBatchStrings(pointer);
                    }
                    return values;
                } finally {
                    lock.unlockRead(stamp);
                }
            }

            @Override
            public void close() {
                final long stamp = lock.writeLock();
                try {
                    if (pointer != 0) {
                        TranslatadorNative.destroyBatch(pointer);
                        pointer = 0;
                        values = null;
                    }
                } finally {
                    lock.unlockWrite(stamp);
                }
            }

            private long checkOpen() {
                if (pointer == 0) {
                    throw new IllegalStateException("Batch has already been closed");
                }
                return pointer;
            }
        }
    }

    private static class NativeLanguageDetector implements LanguageDetector {
        @Override
        public Result detect(final String string) {
            final long result = TranslatadorNative.detectLanguage(string);
            final DetectedLanguage language = DetectedLanguage.byId((int) (result & 0xff));
            final float confidence = Float.intBitsToFloat((int) (result >> 8 & 0xffffffffL));
            return new Result(language, confidence);
        }
    }
}
