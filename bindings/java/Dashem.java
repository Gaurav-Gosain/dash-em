/**
 * Enterprise-Grade Em-Dash Removal Library for Java
 *
 * This class provides Java bindings to the high-performance C library
 * for removing em-dashes (U+2014) from UTF-8 encoded strings.
 */

public class Dashem {
    static {
        System.loadLibrary("dashem");
    }

    /**
     * Remove all em-dashes from the input string.
     *
     * @param input Input string
     * @return String with em-dashes removed
     * @throws UnsatisfiedLinkError if native library is not available
     */
    public static native String remove(String input);

    /**
     * Get library version.
     *
     * @return Version string
     */
    public static native String version();

    /**
     * Get implementation name.
     *
     * @return Implementation name (e.g., "AVX2", "SSE4.2")
     */
    public static native String implementationName();

    /**
     * Detect available CPU features.
     *
     * @return Bitmask of CPU features
     */
    public static native int detectCPUFeatures();

    public static void main(String[] args) {
        System.out.println("dash-em " + version());
        System.out.println("Implementation: " + implementationName());

        if (args.length > 0) {
            String result = remove(args[0]);
            System.out.println("Result: " + result);
        }
    }
}
