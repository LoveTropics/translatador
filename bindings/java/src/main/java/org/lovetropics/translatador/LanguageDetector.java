package org.lovetropics.translatador;

/**
 * {@link LanguageDetector} enables analysis of a string to determine the most likely language.
 * Thread-safe: may be used from multiple threads.
 *
 * @see LanguageDetector#detect(String)
 */
public interface LanguageDetector {
    /**
     * Analyzes the given string to determine its language.
     *
     * @param string the string to analyze
     * @return detected language + confidence pair
     * @throws TranslationException if language detection fails
     */
    Result detect(String string) throws TranslationException;

    /**
     * Represents a detected language + confidence pair.
     *
     * @param language   the detected language
     * @param confidence confidence that this language is correct
     */
    record Result(DetectedLanguage language, float confidence) {
        public static final float RELIABLE_CONFIDENCE = 0.9f;

        /**
         * @return {@code true} if this result should be considered reliable (confidence >= {@value RELIABLE_CONFIDENCE})
         */
        public boolean isReliable() {
            return confidence >= RELIABLE_CONFIDENCE;
        }
    }
}
