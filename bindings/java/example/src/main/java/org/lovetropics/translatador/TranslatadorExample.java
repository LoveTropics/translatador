package org.lovetropics.translatador;

import java.io.IOException;
import java.nio.file.Path;
import java.util.Scanner;

public class TranslatadorExample {
    public static void main(final String[] args) throws IOException, ModelException {
        if (args.length != 3) {
            System.err.println("Usage: <model file> <short-list file> <vocab file>");
            return;
        }
        final Path modelPath = Path.of(args[0]);
        final Path shortListPath = Path.of(args[1]);
        final Path vocabPath = Path.of(args[2]);
        try (final TranslationModel model = Translatador.builder().model(modelPath).vocab(vocabPath).shortList(shortListPath).load()) {
            System.out.println(model.translate("Welcome to the translation example! Type a message to translate."));
            final Scanner scanner = new Scanner(System.in);
            while (scanner.hasNextLine()) {
                System.out.println(model.translate(scanner.nextLine()));
            }
        }
    }
}
