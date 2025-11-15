fn main() {
    cc::Build::new()
        .file("../../src/dashem.c")
        .include("../../src")
        .opt_level(3)
        .flag("-march=native")
        .warnings(true)
        .compile("dashem");

    println!("cargo:rustc-link-search=native=../../build");
}
