require 'ffi'

# Enterprise-Grade Em-Dash Removal Library for Ruby
#
# High-performance, SIMD-accelerated string processing for removing
# em-dashes (U+2014) from UTF-8 encoded text.

module Dashem
  extend FFI::Library

  ffi_lib 'dashem'

  # Remove em-dashes from a string
  attach_function :remove_internal, :dashem_remove, [
    :pointer, :size_t, :pointer, :size_t, :pointer
  ], :int

  attach_function :version, :dashem_version, [], :string
  attach_function :implementation_name, :dashem_implementation_name, [], :string
  attach_function :detect_cpu_features, :dashem_detect_cpu_features, [], :uint32

  def self.remove(input)
    raise TypeError, 'Input must be a string' unless input.is_a?(String)

    input_bytes = input.encode('UTF-8').bytes
    input_ptr = FFI::MemoryPointer.new(:uint8, input_bytes.length)
    input_ptr.write_array_of_uint8(input_bytes)

    output_ptr = FFI::MemoryPointer.new(:uint8, input_bytes.length)
    output_len_ptr = FFI::MemoryPointer.new(:size_t, 1)

    result = remove_internal(input_ptr, input_bytes.length, output_ptr, input_bytes.length, output_len_ptr)

    raise "dashem_remove failed with code #{result}" unless result == 0

    output_len = output_len_ptr.read_size_t
    output_ptr.read_bytes(output_len).force_encoding('UTF-8')
  end
end
