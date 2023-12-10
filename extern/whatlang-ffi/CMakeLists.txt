include(FetchContent)
FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.4.5
)
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(MANIFEST_PATH Cargo.toml CRATE_TYPES staticlib)
target_include_directories(whatlang INTERFACE include)
