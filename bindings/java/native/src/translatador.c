#include <jni.h>
#include <stdlib.h>
#include <translatador.h>

struct Batch {
    jint count;
    const TrlString* strings[];
};

void throw_error(JNIEnv* env, const char* exception_type) {
    char* last_error = trl_get_last_error();
    if (last_error) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, exception_type), last_error);
        free(last_error);
    }
}

JNIEXPORT jlong JNICALL Java_org_lovetropics_translatador_TranslatadorNative_createModel(JNIEnv* env, jclass class, const jstring yaml_config_string, const jbyteArray model_bytes, const jbyteArray source_vocab_bytes, const jbyteArray target_vocab_bytes, const jbyteArray short_list_bytes) {
    const char* yaml_config = yaml_config_string ? (*env)->GetStringUTFChars(env, yaml_config_string, 0) : 0;
    jbyte* model = (*env)->GetByteArrayElements(env, model_bytes, 0);
    const size_t model_size = (*env)->GetArrayLength(env, model_bytes);
    jbyte* source_vocab = (*env)->GetByteArrayElements(env, source_vocab_bytes, 0);
    const size_t source_vocab_size = (*env)->GetArrayLength(env, source_vocab_bytes);
    jbyte* target_vocab = 0;
    size_t target_vocab_size = 0;
    if (target_vocab_bytes && source_vocab_bytes != target_vocab_bytes) {
        target_vocab = (*env)->GetByteArrayElements(env, target_vocab_bytes, 0);
        target_vocab_size = (*env)->GetArrayLength(env, target_vocab_bytes);
    }
    jbyte* short_list = 0;
    size_t short_list_size = 0;
    if (short_list_bytes) {
        short_list = (*env)->GetByteArrayElements(env, short_list_bytes, 0);
        short_list_size = (*env)->GetArrayLength(env, short_list_bytes);
    }
    const TrlModel* result = trl_create_model(
        yaml_config,
        (char *)model,
        model_size,
        (char *)source_vocab,
        source_vocab_size,
        (char *)target_vocab,
        target_vocab_size,
        (char *)short_list,
        short_list_size
    );
    if (yaml_config) {
        (*env)->ReleaseStringUTFChars(env, yaml_config_string, yaml_config);
    }
    (*env)->ReleaseByteArrayElements(env, model_bytes, model, JNI_ABORT);
    (*env)->ReleaseByteArrayElements(env, source_vocab_bytes, source_vocab, JNI_ABORT);
    if (target_vocab) {
        (*env)->ReleaseByteArrayElements(env, target_vocab_bytes, target_vocab, JNI_ABORT);
    }
    if (short_list) {
        (*env)->ReleaseByteArrayElements(env, short_list_bytes, short_list, JNI_ABORT);
    }
    if (!result) {
        throw_error(env, "org/lovetropics/translatador/ModelException");
    }
    return (size_t)result;
}

JNIEXPORT jlong JNICALL Java_org_lovetropics_translatador_TranslatadorNative_cloneModel(JNIEnv* env, jclass class, const jlong raw_model) {
    const TrlModel* model = (TrlModel *)(size_t)raw_model;
    return (size_t)trl_clone_model(model);
}

JNIEXPORT void JNICALL Java_org_lovetropics_translatador_TranslatadorNative_destroyModel(JNIEnv* env, jclass class, const jlong model) {
    trl_destroy_model((TrlModel *)(size_t)model);
}

struct Batch* create_batch(JNIEnv* env, const jobjectArray strings_array) {
    const jint count = (*env)->GetArrayLength(env, strings_array);

    struct Batch* batch = malloc(sizeof(struct Batch) + count * sizeof(TrlString *));
    batch->count = count;

    for (jint i = 0; i < count; i++) {
        const jstring string = (*env)->GetObjectArrayElement(env, strings_array, i);
        const char* c_string = (*env)->GetStringUTFChars(env, string, 0);
        batch->strings[i] = trl_create_string(c_string);
        (*env)->ReleaseStringUTFChars(env, string, c_string);
    }

    return batch;
}

void destroy_batch(const struct Batch* batch) {
    for (jint i = 0; i < batch->count; i++) {
        trl_destroy_string(batch->strings[i]);
    }
    free((void *)batch);
}

JNIEXPORT jobjectArray JNICALL Java_org_lovetropics_translatador_TranslatadorNative_getBatchStrings(JNIEnv* env, jclass class, const jlong raw_batch) {
    const struct Batch* batch = (struct Batch *)(size_t)raw_batch;

    const jobjectArray results_array = (*env)->NewObjectArray(env, batch->count, (*env)->FindClass(env, "java/lang/String"), 0);
    for (jint i = 0; i < batch->count; i++) {
        const char* string = trl_get_string_utf((TrlString *)(size_t)batch->strings[i]);
        (*env)->SetObjectArrayElement(env, results_array, i, (*env)->NewStringUTF(env, string));
    }

    return results_array;
}

JNIEXPORT void JNICALL Java_org_lovetropics_translatador_TranslatadorNative_destroyBatch(JNIEnv* env, jclass class, const jlong raw_batch) {
    destroy_batch((struct Batch *)(size_t)raw_batch);
}

jlong translate(JNIEnv* env, const TrlModel* model, const struct Batch* source) {
    const jint count = source->count;

    struct Batch* target = malloc(sizeof(struct Batch) + count * sizeof(TrlString *));
    target->count = count;

    const int error = trl_translate(model, (const TrlString * const *)&source->strings, (const TrlString * *)&target->strings, count);
    if (error) {
        free(target);
        throw_error(env, "org/lovetropics/translatador/TranslationException");
        return 0;
    }

    return (size_t)target;
}

JNIEXPORT jlong JNICALL Java_org_lovetropics_translatador_TranslatadorNative_translate(JNIEnv* env, jclass class, const jlong raw_model, const jlong raw_source_batch) {
    const TrlModel* model = (TrlModel *)(size_t)raw_model;
    const struct Batch* source = (const struct Batch *)(size_t *)raw_source_batch;
    return translate(env, model, source);
}

JNIEXPORT jlong JNICALL Java_org_lovetropics_translatador_TranslatadorNative_translatePlain(JNIEnv* env, jclass class, const jlong raw_model, const jobjectArray strings_array) {
    const TrlModel* model = (TrlModel *)(size_t)raw_model;
    const struct Batch* source = create_batch(env, strings_array);
    const jlong result = translate(env, model, source);
    destroy_batch(source);
    return result;
}
