#include <translatador.h>
#include <stdio.h>

int read_file(const char* file_name, char** result) {
    FILE* file = fopen(file_name, "rb");

    fseek(file, 0, SEEK_END);
    const int size = ftell(file);
    rewind(file);

    char* buffer = malloc(size);
    fread(buffer, size, 1, file);

    *result = buffer;
    return size;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: <model file> <short-list file> <vocab file>");
        return 0;
    }

    const char* model_file = argv[1];
    const char* short_list_file = argv[2];
    const char* vocab_file = argv[3];

    char* model_buffer;
    int model_size = read_file(model_file, &model_buffer);
    char* short_list_buffer;
    int short_list_size = read_file(short_list_file, &short_list_buffer);
    char* vocab_buffer;
    int vocab_size = read_file(vocab_file, &vocab_buffer);

    const char* yaml_config =
            "beam-size: 1\n"
            "normalize: 1.0\n"
            "word-penalty: 0\n"
            "max-length-break: 128\n"
            "mini-batch-words: 1024\n"
            "workspace: 128\n"
            "max-length-factor: 2.0\n"
            "skip-cost: true\n"
            "gemm-precision: int8shiftAlphaAll\n"
            "alignment: soft\n";

    const TrlModel* model = trl_create_model(yaml_config, model_buffer, model_size, vocab_buffer, vocab_size, 0, 0, short_list_buffer, short_list_size);
    if (!model) {
        printf("Failed to create model: %s", trl_get_last_error());
        return 0;
    }
    free(model_buffer);
    free(vocab_buffer);
    free(short_list_buffer);

    const TrlString* source = trl_create_string("Hello from the C programming language!");
    const TrlString* target;

    if (trl_translate(model, &source, &target, 1)) {
        printf("Failed to translate text: %s", trl_get_last_error());
        return 0;
    }

    printf("%s -> %s\n", trl_get_string_utf(source), trl_get_string_utf(target));

    trl_destroy_string(source);
    trl_destroy_string(target);
    trl_destroy_model(model);

    return 0;
}
