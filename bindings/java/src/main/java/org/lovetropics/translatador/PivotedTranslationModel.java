package org.lovetropics.translatador;

import java.util.HashMap;
import java.util.Map;

/**
 * Wraps many {@link TranslationModel TranslationModels} in order to translate from multiple languages, into multiple
 * languages through a common pivot language.
 * <p>
 * Made up of a set of {@link TranslationModel TranslationModels} that know how to encode into the pivot language, as
 * well as a set that knows how to decode from the pivot language.
 * <p>
 * {@link PivotedTranslationModel} holds a reference to many {@link TranslationModel TranslationModels}, but it does not
 * take ownership of them. These models must still be held in memory, and {@link AutoCloseable closed} when they are no
 * longer required.
 * <p>
 * As with {@link TranslationModel}, {@link PivotedTranslationModel} is thread-safe, but might experience
 * synchronization on its underlying models. {@link TranslationModel#fork()} should be used to construct multiple
 * instances that can be used from multiple threads concurrently.
 *
 * @param <A> the source language type
 * @param <B> the target language type
 */
public class PivotedTranslationModel<A, B> {
    private final Map<A, TranslationModel> encoders;
    private final Map<B, TranslationModel> decoders;

    public PivotedTranslationModel(final Map<A, TranslationModel> encoders, final Map<B, TranslationModel> decoders) {
        this.encoders = Map.copyOf(encoders);
        this.decoders = Map.copyOf(decoders);
    }

    /**
     * Translates the given string from the given language to all known target languages through the pivot.
     *
     * @param language the source language to translate from
     * @param string   the string to translate
     * @return the translated string
     * @throws TranslationException if the strings could not be translated
     */
    public Map<B, String> translate(final A language, final String string) throws TranslationException {
        final TranslationModel encoder = encoders.get(language);
        if (encoder == null) {
            return Map.of();
        }
        final Map<B, String> results = new HashMap<>(decoders.size());
        try (
                final TranslationBatch source = TranslationBatch.of(string);
                final TranslationBatch pivots = encoder.translateBatch(source)
        ) {
            for (final Map.Entry<B, TranslationModel> entry : decoders.entrySet()) {
                try (final TranslationBatch target = entry.getValue().translateBatch(pivots)) {
                    results.put(entry.getKey(), target.get()[0]);
                }
            }
        }
        return results;
    }

    /**
     * Translates the given batch from the given language to all known target languages through the pivot.
     *
     * @param language the source language to translate from
     * @param batch    the batch to translate
     * @return the translated batch
     * @throws TranslationException if the strings could not be translated
     */
    public Map<B, TranslationBatch> translateBatch(final A language, final TranslationBatch batch) throws TranslationException {
        final TranslationModel encoder = encoders.get(language);
        if (encoder == null) {
            return Map.of();
        }
        final Map<B, TranslationBatch> results = new HashMap<>(decoders.size());
        try (final TranslationBatch pivots = encoder.translateBatch(batch)) {
            for (final Map.Entry<B, TranslationModel> entry : decoders.entrySet()) {
                results.put(entry.getKey(), entry.getValue().translateBatch(pivots));
            }
        }
        return results;
    }
}
