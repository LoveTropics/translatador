# Translatador
Translatador is a minimal C wrapper around [Bergamot Translator](https://github.com/browsermt/bergamot-translator),
providing lightweight offline translation of text on the CPU.

Translatador additionally provides Java bindings, which you can find more about [here](bindings/java/README.md).

## Usage
This repository includes a simple [example project](examples) to help you to get started with Translatador.

### Models
Translatador does not support training models: as such, you will need to use a pretrained model built for Bergamot.
We recommend taking a look at Mozilla's CPU-optimized open-source models in the [firefox-translation-models](https://github.com/mozilla/firefox-translations-models) project, which are currently used for offline translation within Firefox.
