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
 * \param yaml_config optional Bergamot YAML configuration to be used to load this model, or null to use defaults (<https://github.com/mozilla/firefox-translations-models/blob/main/evals/translators/bergamot.config.yml>)
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
 * If an error occurs, the target will not be modified, and the error message will be accessible through \trl_get_last_error.
 *
 * \param model the model to use for translation
 * \param source the source strings to translate
 * \param target a pointer to place translated strings
 * \param count the number of strings to translate
 * \return \link TRL_OK if translation was successful, or \link TRL_ERROR if not
 */
TrlError trl_translate(const TrlModel* model, const TrlString* const* source, const TrlString** target, size_t count);

#ifdef __cplusplus
}
#endif

#endif
