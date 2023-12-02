package org.lovetropics.translatador;

import java.util.Arrays;
import java.util.List;

/**
 * Wrapper around a collection of strings to be used with a {@link TranslationModel}.
 * <p>
 * The batch might hold additional metadata about strings passed through the model. If a string should be passed through
 * multiple models, it should ideally be kept within {@link TranslationBatch} until it should be resolved.
 * <p>
 * {@link TranslationBatch} might hold onto native resources, and must be {@link AutoCloseable#close() closed} once no longer required.
 *
 * @see TranslationModel#translateBatch(TranslationBatch)
 */
public abstract class TranslationBatch implements AutoCloseable {
    /**
     * Creates a new {@link TranslationBatch} from the given plain strings.
     *
     * @param strings plain strings
     * @return a new batch
     */
    static TranslationBatch of(final String... strings) {
        return new TranslationBatch() {
            @Override
            public String[] get() {
                return strings;
            }

            @Override
            public void close() {
            }
        };
    }

    /**
     * Creates a new {@link TranslationBatch} from the given plain strings.
     *
     * @param strings plain strings
     * @return a new batch
     */
    static TranslationBatch of(final List<String> strings) {
        return of(strings.toArray(String[]::new));
    }

    /**
     * Takes a plain copy of the given {@link TranslationBatch}. This will strip any metadata stored within this batch.
     *
     * @param batch the batch to copy
     * @return a plain copy of the given batch
     */
    static TranslationBatch plainCopyOf(final TranslationBatch batch) {
        return of(batch.get());
    }

    /**
     * @return this batch's resolved plain strings
     */
    public abstract String[] get();

    /**
     * @return this batch's resolved plain strings
     */
    public List<String> asList() {
        return Arrays.asList(get());
    }

    @Override
    public String toString() {
        return Arrays.toString(get());
    }

    @Override
    public abstract void close();
}
