fn main() {
    // Remove em-dashes from a string
    match dashem::remove("Hello—world—from—Rust!") {
        Ok(result) => println!("Result: {}", result),
        Err(e) => eprintln!("Error: {}", e),
    }

    // Get version
    println!("Version: {}", dashem::version());

    // Get implementation
    println!("Implementation: {}", dashem::implementation_name());
}
