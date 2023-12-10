package org.lovetropics.translatador;

/**
 * Represents a language that can be detected using a {@link LanguageDetector}.
 * Each language exposes a corresponding <a href="https://en.wikipedia.org/wiki/ISO_639-3">ISO 639-3</a> language code.
 */
public enum DetectedLanguage {
    ESPERANTO("epo"),
    ENGLISH("eng"),
    RUSSIAN("rus"),
    MANDARIN("cmn"),
    SPANISH("spa"),
    PORTUGUESE("por"),
    ITALIAN("ita"),
    BENGALI("ben"),
    FRENCH("fra"),
    GERMAN("deu"),
    UKRAINIAN("ukr"),
    GEORGIAN("kat"),
    ARABIC("ara"),
    HINDI("hin"),
    JAPANESE("jpn"),
    HEBREW("heb"),
    YIDDISH("yid"),
    POLISH("pol"),
    AMHARIC("amh"),
    JAVANESE("jav"),
    KOREAN("kor"),
    BOKMAL("nob"),
    DANISH("dan"),
    SWEDISH("swe"),
    FINNISH("fin"),
    TURKISH("tur"),
    DUTCH("nld"),
    HUNGARIAN("hun"),
    CZECH("ces"),
    GREEK("ell"),
    BULGARIAN("bul"),
    BELARUSSIAN("bel"),
    MARATHI("mar"),
    KANNADA("kan"),
    ROMANIAN("ron"),
    SLOVENE("slv"),
    CROATIAN("hrv"),
    SERBIAN("srp"),
    MACEDONIAN("mkd"),
    LITHUANIAN("lit"),
    LATVIAN("lav"),
    ESTONIAN("est"),
    TAMIL("tam"),
    VIETNAMESE("vie"),
    URDU("urd"),
    THAI("tha"),
    GUJARATI("guj"),
    UZBEK("uzb"),
    PUNJABI("pan"),
    AZERBAIJANI("aze"),
    INDONESIAN("ind"),
    TELUGU("tel"),
    PERSIAN("pes"),
    MALAYALAM("mal"),
    ORIYA("ori"),
    BURMESE("mya"),
    NEPALI("nep"),
    SINHALESE("sin"),
    KHMER("khm"),
    TURKMEN("tuk"),
    AKAN("aka"),
    ZULU("zul"),
    SHONA("sna"),
    AFRIKAANS("afr"),
    LATIN("lat"),
    SLOVAK("slk"),
    CATALAN("cat"),
    TAGALOG("tgl"),
    ARMENIAN("hye"),
    ;

    private static final DetectedLanguage[] VALUES = values();

    private final String code;

    DetectedLanguage(final String code) {
        this.code = code;
    }

    public static DetectedLanguage byId(final int id) {
        if (id >= 0 && id < VALUES.length) {
            return VALUES[id];
        }
        return null;
    }

    /**
     * @return the <a href="https://en.wikipedia.org/wiki/ISO_639-3">ISO 639-3</a> language code for this {@link DetectedLanguage}
     */
    public String code() {
        return code;
    }
}
