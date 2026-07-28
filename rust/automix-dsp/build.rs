fn main() {
    let crate_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    // cbindgen parses the whole crate, so narrowing this to ffi.rs and
    // constants.rs let the committed header drift: moving a #[repr(C)] struct
    // or a pub const into any other module changed the ABI without
    // regenerating it. Watch the entire source tree instead.
    println!("cargo:rerun-if-changed=src");
    println!("cargo:rerun-if-changed=cbindgen.toml");

    // Ensure the output directory exists
    std::fs::create_dir_all("include").unwrap();

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(cbindgen::Config::from_file("cbindgen.toml").unwrap())
        .generate()
        .expect("Unable to generate C bindings")
        .write_to_file("include/automix_dsp.h");
}
