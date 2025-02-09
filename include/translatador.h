#ifndef TRANSLATADOR_H
#define TRANSLATADOR_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TrlError {
 TRL_OK = 0,
 TRL_ERROR = 1,
} TrlError;

/**
 * \brief A model that supports translation between a source and target language.
 * Should not be used from multiple threads.
 */
typedef struct TrlModel TrlModel;

/**
 * \brief A wrapper around a string that can or has been translated.
 * May contain additional metadata from translation, so strings should be kept in this form as long as possible if they
 * need to be passed through multiple models through a pivot language.
 */
typedef struct TrlString TrlString;

/**
 * \brief Represents an ISO 639-3 language code that may be detected by \link trl_detect_language
 */
typedef enum TrlDetectedLang {
    // Esperanto (Esperanto)
    Epo = 0,
    // English (English)
    Eng = 1,
    // Русский (Russian)
    Rus = 2,
    // 普通话 (Mandarin)
    Cmn = 3,
    // Español (Spanish)
    Spa = 4,
    // Português (Portuguese)
    Por = 5,
    // Italiano (Italian)
    Ita = 6,
    // বাংলা (Bengali)
    Ben = 7,
    // Français (French)
    Fra = 8,
    // Deutsch (German)
    Deu = 9,
    // Українська (Ukrainian)
    Ukr = 10,
    // ქართული (Georgian)
    Kat = 11,
    // العربية (Arabic)
    Ara = 12,
    // हिन्दी (Hindi)
    Hin = 13,
    // 日本語 (Japanese)
    Jpn = 14,
    // עברית (Hebrew)
    Heb = 15,
    // ייִדיש (Yiddish)
    Yid = 16,
    // Polski (Polish)
    Pol = 17,
    // አማርኛ (Amharic)
    Amh = 18,
    // Basa Jawa (Javanese)
    Jav = 19,
    // 한국어 (Korean)
    Kor = 20,
    // Bokmål (Bokmal)
    Nob = 21,
    // Dansk (Danish)
    Dan = 22,
    // Svenska (Swedish)
    Swe = 23,
    // Suomi (Finnish)
    Fin = 24,
    // Türkçe (Turkish)
    Tur = 25,
    // Nederlands (Dutch)
    Nld = 26,
    // Magyar (Hungarian)
    Hun = 27,
    // Čeština (Czech)
    Ces = 28,
    // Ελληνικά (Greek)
    Ell = 29,
    // Български (Bulgarian)
    Bul = 30,
    // Беларуская (Belarusian)
    Bel = 31,
    // मराठी (Marathi)
    Mar = 32,
    // ಕನ್ನಡ (Kannada)
    Kan = 33,
    // Română (Romanian)
    Ron = 34,
    // Slovenščina (Slovene)
    Slv = 35,
    // Hrvatski (Croatian)
    Hrv = 36,
    // Српски (Serbian)
    Srp = 37,
    // Македонски (Macedonian)
    Mkd = 38,
    // Lietuvių (Lithuanian)
    Lit = 39,
    // Latviešu (Latvian)
    Lav = 40,
    // Eesti (Estonian)
    Est = 41,
    // தமிழ் (Tamil)
    Tam = 42,
    // Tiếng Việt (Vietnamese)
    Vie = 43,
    // اُردُو (Urdu)
    Urd = 44,
    // ภาษาไทย (Thai)
    Tha = 45,
    // ગુજરાતી (Gujarati)
    Guj = 46,
    // Oʻzbekcha (Uzbek)
    Uzb = 47,
    // ਪੰਜਾਬੀ (Punjabi)
    Pan = 48,
    // Azərbaycanca (Azerbaijani)
    Aze = 49,
    // Bahasa Indonesia (Indonesian)
    Ind = 50,
    // తెలుగు (Telugu)
    Tel = 51,
    // فارسی (Persian)
    Pes = 52,
    // മലയാളം (Malayalam)
    Mal = 53,
    // ଓଡ଼ିଆ (Oriya)
    Ori = 54,
    // မြန်မာစာ (Burmese)
    Mya = 55,
    // नेपाली (Nepali)
    Nep = 56,
    // සිංහල (Sinhalese)
    Sin = 57,
    // ភាសាខ្មែរ (Khmer)
    Khm = 58,
    // Türkmençe (Turkmen)
    Tuk = 59,
    // Akan (Akan)
    Aka = 60,
    // IsiZulu (Zulu)
    Zul = 61,
    // ChiShona (Shona)
    Sna = 62,
    // Afrikaans (Afrikaans)
    Afr = 63,
    // Lingua Latina (Latin)
    Lat = 64,
    // Slovenčina (Slovak)
    Slk = 65,
    // Català (Catalan)
    Cat = 66,
    // Tagalog (Tagalog)
    Tgl = 67,
    // Հայերեն (Armenian)
    Hye = 68,
} TrlDetectedLang;

/**
 * \brief Holds a detected language, as well as a confidence value that this language matches the analyzed string.
 */
typedef struct TrlDetectedLangInfo {
 TrlDetectedLang lang;
 float confidence;
} TrlDetectedLangInfo;

/**
 * \brief Returns a string describing the last error to occur. If none has occurred since the library was initialized,
 * or since this function was last called, null will be returned.
 *
 * The caller is expected to free() this memory after use.
 *
 * \return an error string, or null
 */
char* trl_get_last_error();

/**
 * \brief Loads a translation model from the given binaries and configurations.
 * If the given data is malformed, null will be returned, and an error message should be accessible through \link trl_get_last_error.
 *
 * This function does not take ownership of any of the passed memory, and this should be freed by the caller when no longer required.
 *
 * \link trl_destroy_model should be used once the model is no longer needed.
 *
 * \param yaml_config optional Marian YAML configuration to be used to load this model, or null to use defaults (<https://github.com/mozilla/firefox-translations-models/blob/main/evals/translators/bergamot.config.yml>)
 * \param model model binary to load
 * \param model_size size of the model binary
 * \param source_vocab vocabulary of the source language to load
 * \param source_vocab_size size of the source vocabulary
 * \param target_vocab optional vocabulary of the target language to load, or null to use a shared vocabulary between source and target
 * \param target_vocab_size size of the target vocabulary, or 0 if shared
 * \param short_list optional short list to load
 * \param short_list_size size of teh short list, or 0 if unused
 * \return the loaded model, or null if the model failed to load
 */
const TrlModel* trl_create_model(const char* yaml_config, const char* model, size_t model_size, const char* source_vocab, size_t source_vocab_size, const char* target_vocab, size_t target_vocab_size, const char* short_list, size_t short_list_size);

/**
 * \brief Takes a copy of the given translation model. As \link TrlModel is not thread-safe, this might be used from another thread.
 *
 * \param model the model to clone
 * \return a new model instance
 */
const TrlModel* trl_clone_model(const TrlModel* model);

/**
 * \brief Tears down and frees the memory held by the given \TrlModel.
 * \param model the model to destroy
 */
void trl_destroy_model(const TrlModel* model);

/**
 * \brief Wraps the given string by copying for use in translation.
 * \param utf plain string to wrap
 * \return a new \link TrlString that can be used for translation
 */
const TrlString* trl_create_string(const char* utf);

/**
 * \brief Unwraps the plain string held by the given \link TrlString.
 * \param string the string to unwrap
 * \return a reference to the plain string held by the given \link TrlString
 */
const char* trl_get_string_utf(const TrlString* string);

/**
 * \brief Tears down and frees the memory held by the given \link TrlString.
 * \param string the string to destroy
 */
void trl_destroy_string(const TrlString* string);

/**
 * \brief Translates the given source strings into the target language using the given model.
 * If an error occurs, the target will not be modified, and the error message will be accessible through \link trl_get_last_error.
 *
 * \param model the model to use for translation
 * \param source the source strings to translate
 * \param target a pointer to place translated strings (if successful)
 * \param count the number of strings to translate
 * \return \link TRL_OK if translation was successful, or \link TRL_ERROR if not
 */
TrlError trl_translate(const TrlModel* model, const TrlString* const* source, const TrlString** target, size_t count);

/**
 * \brief Analyzes the given string to determine which language it is most likely written in.
 * If an error occurs, the result will not be modified, and the error message will be accessible through \link trl_get_last_error.
 *
 * \param string the string to analyze
 * \param result pointer to place the detected language information, including confidence (if successful)
 * \return \link TRL_OK if detection was successful, or \link TRL_ERROR if not
 */
TrlError trl_detect_language(const char* string, TrlDetectedLangInfo* result);

#ifdef __cplusplus
}
#endif

#endif
