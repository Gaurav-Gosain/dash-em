use dash_em;

fn main() {
    // Remove em-dashes from a string
    match dash_em::remove("Hello—world—from—Rust!") {
        Ok(result) => println!("Result: {}", result),
        Err(e) => eprintln!("Error: {}", e),
    }

    // Get version
    println!("Version: {}", dash_em::version());

    // Get implementation
    println!("Implementation: {}", dash_em::implementation_name());
}
