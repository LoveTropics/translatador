package org.lovetropics.translatador;

import java.util.Arrays;
import java.util.List;

/**
 * A {@link TranslationModel} maps between strings of one language into strings of another language.
 * <p>
 * {@link TranslationModel TranslationModels} might hold onto native resources, and must be
 * {@link AutoCloseable#close() closed} once they are no longer required.
 * <p>
 * {@link TranslationModel TranslationModels} should be usable safely from multiple threads, but it is not guaranteed
 * that this will give optimal performance. {@link TranslationModel#fork()} should be used in this case.
 *
 * @see Translatador#builder()
 * @see TranslationModel#translate(String)
 * @see TranslationModel#compose(TranslationModel)
 * @see PivotedTranslationModel
 */
public interface TranslationModel extends AutoCloseable {
    /**
     * The identity {@link TranslationModel}. Makes no modification to the given input strings.
     */
    TranslationModel IDENTITY = new TranslationModel() {
        @Override
        public String translate(final String string) throws TranslationException {
            return string;
        }

        @Override
        public String[] translateBatch(final String... strings) throws TranslationException {
            return Arrays.copyOf(strings, strings.length);
        }

        @Override
        public TranslationBatch translateBatch(final TranslationBatch batch) throws TranslationException {
            return TranslationBatch.plainCopyOf(batch);
        }
    };

    /**
     * Translates the given string as per this model.
     *
     * @param string source string to translate
     * @return the translated string
     * @throws TranslationException if the string could not be translated
     * @see TranslationModel#translateBatch(String...)
     */
    default String translate(final String string) throws TranslationException {
        return translateBatch(string)[0];
    }

    /**
     * Translates all the given strings as per this model. This is likely to be more efficient than passing single
     * strings at a time, but the implementation makes no guarantees about how it processes the batch.
     *
     * @param strings source strings to translate
     * @return the translated strings, directly mapped from the source strings
     * @throws TranslationException if the strings could not be translated
     * @see TranslationModel#translate(String)
     * @see TranslationModel#translateBatch(List)
     */
    default String[] translateBatch(final String... strings) throws TranslationException {
        try (
                final TranslationBatch source = TranslationBatch.of(strings);
                final TranslationBatch target = translateBatch(source)
        ) {
            return target.get();
        }
    }

    /**
     * Translates all the given strings as per this model. This is likely to be more efficient than passing single
     * strings at a time, but the implementation makes no guarantees about how it processes the batch.
     *
     * @param strings source strings to translate
     * @return the translated strings, directly mapped from the source strings
     * @throws TranslationException if the strings could not be translated
     * @see TranslationModel#translate(String)
     * @see TranslationModel#translateBatch(String...)
     */
    default List<String> translateBatch(final List<String> strings) throws TranslationException {
        return Arrays.asList(translateBatch(strings.toArray(String[]::new)));
    }

    /**
     * Translates all the given strings as per this model. This is likely to be more efficient than passing single
     * strings at a time, but the implementation makes no guarantees about how it processes the batch.
     *
     * @param batch a batch filled with the source strings to translate
     * @return the translated strings, directly mapped from the source strings
     * @throws TranslationException if the strings could not be translated
     * @see TranslationModel#translateBatch(String...)
     * @see TranslationModel#translateBatch(List)
     */
    TranslationBatch translateBatch(TranslationBatch batch) throws TranslationException;

    /**
     * Constructs a composed {@link TranslationModel} using a pivot language. This might be used to translate between
     * two languages without a model that directly maps between them.
     * This is likely to be more efficient and accurate than passing strings through the two models separately, but the
     * implementation makes no guarantees to this.
     * <p>
     * The newly constructed model references both {@code this} and {@code before}, but it does not take ownership.
     * The composed model can only be used so long as both component models have not yet been closed.
     *
     * @param before the {@link TranslationModel} that should be applied before this one
     * @return a new composed model
     * @see PivotedTranslationModel
     */
    default TranslationModel compose(final TranslationModel before) {
        return compose(before, this);
    }

    /**
     * Returns an identical instance of this {@link TranslationModel} that can be used concurrently from another thread.
     * Although {@link TranslationModel} should always be thread-safe, it should not be expected that they can be used
     * concurrently without synchronization. This function should be used to efficiently use a {@link TranslationModel}
     * from multiple threads.
     * <p>
     * This might be thought of as a copy. The forked {@link TranslationModel} may outlive its original, and it must be
     * independently closed when it is no longer required.
     *
     * @return an identical instance of this model that can be used from another thread
     */
    default TranslationModel fork() {
        return this;
    }

    /**
     * Frees the resources associated with this {@link TranslationModel}. This should always be called once a
     * {@link TranslationModel} is no longer required.
     */
    @Override
    default void close() {
    }

    /**
     * Constructs a composed {@link TranslationModel} using a pivot language. This might be used to translate between
     * two languages without a model that directly maps between them.
     * This is likely to be more efficient and accurate than passing strings through the two models separately, but the
     * implementation makes no guarantees to this.
     * <p>
     * The newly constructed model references both {@code first} and {@code second}, but it does not take ownership.
     * The composed model can only be used so long as both component models have not yet been closed.
     *
     * @param first  the first {@link TranslationModel} to apply
     * @param second the second {@link TranslationModel} to apply
     * @return a new composed model
     * @see PivotedTranslationModel
     */
    static TranslationModel compose(final TranslationModel first, final TranslationModel second) {
        return new TranslationModel() {
            @Override
            public TranslationBatch translateBatch(final TranslationBatch batch) throws TranslationException {
                try (final TranslationBatch pivot = first.translateBatch(batch)) {
                    return second.translateBatch(pivot);
                }
            }

            @Override
            public TranslationModel fork() {
                return TranslationModel.compose(first.fork(), second.fork());
            }
        };
    }
}
