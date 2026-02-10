fn main() {
    let crate_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    // Only re-run when FFI-relevant sources change
    println!("cargo:rerun-if-changed=src/ffi.rs");
    println!("cargo:rerun-if-changed=src/constants.rs");
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
