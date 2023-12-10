# Translatador (Java Bindings)
This project wraps around the native Translatador API in order to expose translation and language detection to Java.

## Usage
Translatador can be easily included in your project with Gradle:
```gradle
// Replace with desired version and supported platforms (see below)
def translatadorVersion = '0.1.0'
def translatadorNatives = ['native-windows']

repositories {
  maven { url = 'https://maven.gegy.dev/' }
}

dependencies {
  implementation "org.lovetropics:translatador:$translatadorVersion"
  translatadorNatives.each { native ->
    runtimeOnly "org.lovetropics:translatador:$translatadorVersion:$native"
  }
}
```

You can find the latest release [here](https://github.com/LoveTropics/translatador/releases).

As Translatador is a native library, using the Java bindings requires that you include the natives for the platforms you intend to support.
Currently, Translatador publishes prebuilt bindings for:
 - Windows x64 (`native-windows`)
 - Windows ARM64 (`native-windows-arm64`)
 - Linux x64 (`native-linux`)
 - Linux ARM64 (`native-linux-arm64`)
 - MacOS x64 (`native-macos`)

Once you have Translatador imported, you can get started loading a model and translating:
```java
Path modelPath = Path.of("...");
Path vocabPath = Path.of("...");
Path shortListPath = Path.of("...");
try (TranslationModel model = Translatador.builder().model(modelPath).vocab(vocabPath).shortList(shortListPath).load()) {
    System.out.println(model.translate("Hello world!"));
}
```

You can find pre-built open-source models optimized for the CPU in the [firefox-translation-models](https://github.com/mozilla/firefox-translations-models) repository.

Built on [whatlang-rs](https://github.com/greyblake/whatlang-rs), Translatador can detect [69 different languages](https://github.com/greyblake/whatlang-rs/blob/master/SUPPORTED_LANGUAGES.md):
```java
LanguageDetector detector = Translatador.createLanguageDetector();
System.out.println(detector.detect("This is a test string!"));
```
