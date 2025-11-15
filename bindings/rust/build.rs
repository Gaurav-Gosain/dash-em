fn main() {
    // Compile the C library with portable optimization flags
    // Note: We don't use -march=native because it breaks binary portability
    // The C library will be compiled with baseline x86-64 instructions

    let manifest_dir = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));

    // C source files are bundled in src/ directory (copied during build)
    // This works both in monorepo and packaged tarball scenarios
    let dashem_src = manifest_dir.join("src").join("dashem.c");
    let dashem_include = manifest_dir.join("src");

    println!("cargo:warning=Using dashem source: {:?}", dashem_src);
    println!("cargo:warning=Using dashem include: {:?}", dashem_include);

    let mut builder = cc::Build::new();
    builder
        .file(&dashem_src)
        .include(&dashem_include)
        .opt_level(3)
        .warnings(true);

    // Add portable optimization flags based on target architecture
    #[cfg(target_arch = "x86_64")]
    {
        // x86-64 baseline without specific extensions for portability
        // The C library's runtime CPU detection will use AVX2/SSE4.2 if available
        builder.flag("-march=x86-64");
    }

    #[cfg(target_arch = "aarch64")]
    {
        // ARM64 baseline
        builder.flag("-march=armv8-a");
    }

    builder.compile("dashem");

    // Link against the compiled C library
    println!("cargo:rustc-link-lib=static=dashem");
}
