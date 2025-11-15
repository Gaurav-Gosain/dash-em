//! Enterprise-Grade Em-Dash Removal Library for Rust
//!
//! This crate provides safe Rust bindings to the high-performance C library
//! for removing em-dashes (U+2014) from UTF-8 encoded strings.

extern "C" {
    fn dashem_remove(
        input: *const u8,
        input_len: usize,
        output: *mut u8,
        output_capacity: usize,
        output_len: *mut usize,
    ) -> i32;

    fn dashem_version() -> *const u8;
    fn dashem_implementation_name() -> *const u8;
    fn dashem_detect_cpu_features() -> u32;
}

/// Removes all em-dashes (U+2014) from the input string.
///
/// # Examples
///
/// ```
/// let result = dash_em::remove("Hello—world").unwrap();
/// assert_eq!(result, "Helloworld");
/// ```
///
/// # Errors
///
/// Returns an error if the input is invalid or the operation fails.
pub fn remove(input: &str) -> Result<String, String> {
    let input_bytes = input.as_bytes();
    let mut output = vec![0u8; input_bytes.len()];
    let mut output_len = 0usize;

    let result = unsafe {
        dashem_remove(
            input_bytes.as_ptr(),
            input_bytes.len(),
            output.as_mut_ptr(),
            output.len(),
            &mut output_len,
        )
    };

    if result != 0 {
        return Err(format!("dashem_remove failed with code {}", result));
    }

    output.truncate(output_len);
    Ok(String::from_utf8(output)
        .map_err(|e| format!("Invalid UTF-8 in output: {}", e))?)
}

/// Returns the library version string.
pub fn version() -> String {
    unsafe {
        let ptr = dashem_version();
        std::ffi::CStr::from_ptr(ptr as *const i8)
            .to_string_lossy()
            .to_string()
    }
}

/// Returns the name of the active implementation (e.g., "AVX2", "SSE4.2", "Scalar").
pub fn implementation_name() -> String {
    unsafe {
        let ptr = dashem_implementation_name();
        std::ffi::CStr::from_ptr(ptr as *const i8)
            .to_string_lossy()
            .to_string()
    }
}

/// Detects available CPU features.
pub fn detect_cpu_features() -> u32 {
    unsafe { dashem_detect_cpu_features() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_remove_single_emdash() {
        let result = remove("Hello—world").unwrap();
        assert_eq!(result, "Helloworld");
    }

    #[test]
    fn test_remove_multiple_emdashes() {
        let result = remove("First—second—third").unwrap();
        assert_eq!(result, "Firstsecondthird");
    }

    #[test]
    fn test_remove_no_emdashes() {
        let input = "Hello, world!";
        let result = remove(input).unwrap();
        assert_eq!(result, input);
    }

    #[test]
    fn test_version() {
        let ver = version();
        assert!(!ver.is_empty());
    }

    #[test]
    fn test_implementation_name() {
        let name = implementation_name();
        assert!(!name.is_empty());
    }
}
