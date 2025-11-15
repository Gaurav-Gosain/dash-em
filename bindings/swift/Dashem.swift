import Foundation

/**
 * Enterprise-Grade Em-Dash Removal Library for Swift
 *
 * High-performance, SIMD-accelerated string processing for removing
 * em-dashes (U+2014) from UTF-8 encoded text.
 */

@_silgen_name("dashem_remove")
private func dashem_remove(
    _ input: UnsafePointer<UInt8>,
    _ inputLen: Int,
    _ output: UnsafeMutablePointer<UInt8>,
    _ outputCapacity: Int,
    _ outputLen: UnsafeMutablePointer<Int>
) -> Int32

@_silgen_name("dashem_version")
private func dashem_version() -> UnsafePointer<CChar>

@_silgen_name("dashem_implementation_name")
private func dashem_implementation_name() -> UnsafePointer<CChar>

/// Remove em-dashes from a string
public func removeEmDashes(_ input: String) -> String {
    let inputData = input.utf8
    var outputBuffer = [UInt8](repeating: 0, count: inputData.count)
    var outputLen: Int = 0

    let result = inputData.withContiguousStorageIfAvailable { inputBuffer in
        return dashem_remove(
            inputBuffer.baseAddress!,
            inputBuffer.count,
            &outputBuffer,
            outputBuffer.count,
            &outputLen
        )
    }

    guard result == 0 else {
        fatalError("dashem_remove failed")
    }

    return String(bytes: outputBuffer[..<outputLen], encoding: .utf8) ?? ""
}

/// Get library version
public func version() -> String {
    return String(cString: dashem_version())
}

/// Get implementation name
public func implementationName() -> String {
    return String(cString: dashem_implementation_name())
}
