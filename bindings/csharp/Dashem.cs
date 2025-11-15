using System;
using System.Runtime.InteropServices;

/// <summary>
/// Enterprise-Grade Em-Dash Removal Library for C#/.NET
/// </summary>
public static class Dashem
{
    private const string LibraryName = "dashem";

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dashem_remove(
        byte[] input,
        ulong inputLen,
        byte[] output,
        ulong outputCapacity,
        out ulong outputLen
    );

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr dashem_version();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr dashem_implementation_name();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern uint dashem_detect_cpu_features();

    /// <summary>
    /// Remove all em-dashes from the input string.
    /// </summary>
    /// <param name="input">Input string</param>
    /// <returns>String with em-dashes removed</returns>
    public static string Remove(string input)
    {
        if (string.IsNullOrEmpty(input))
            return input;

        byte[] inputBytes = System.Text.Encoding.UTF8.GetBytes(input);
        byte[] outputBuffer = new byte[inputBytes.Length];

        int result = dashem_remove(
            inputBytes,
            (ulong)inputBytes.Length,
            outputBuffer,
            (ulong)outputBuffer.Length,
            out ulong outputLen
        );

        if (result != 0)
            throw new InvalidOperationException($"dashem_remove failed with code {result}");

        return System.Text.Encoding.UTF8.GetString(outputBuffer, 0, (int)outputLen);
    }

    /// <summary>
    /// Get library version.
    /// </summary>
    public static string Version()
    {
        IntPtr ptr = dashem_version();
        return Marshal.PtrToStringAnsi(ptr) ?? "unknown";
    }

    /// <summary>
    /// Get implementation name.
    /// </summary>
    public static string ImplementationName()
    {
        IntPtr ptr = dashem_implementation_name();
        return Marshal.PtrToStringAnsi(ptr) ?? "unknown";
    }

    /// <summary>
    /// Detect available CPU features.
    /// </summary>
    public static uint DetectCPUFeatures()
    {
        return dashem_detect_cpu_features();
    }
}
