package dashem

import (
	"testing"
)

func TestRemoveSingleEmDash(t *testing.T) {
	result, err := Remove("Hello—world")
	if err != nil {
		t.Fatalf("Remove failed: %v", err)
	}
	expected := "Helloworld"
	if result != expected {
		t.Errorf("Remove returned %q, expected %q", result, expected)
	}
}

func TestRemoveMultipleEmDashes(t *testing.T) {
	result, err := Remove("First—second—third—fourth")
	if err != nil {
		t.Fatalf("Remove failed: %v", err)
	}
	expected := "Firstsecondthirdfourth"
	if result != expected {
		t.Errorf("Remove returned %q, expected %q", result, expected)
	}
}

func TestRemoveNoEmDashes(t *testing.T) {
	input := "Hello, world!"
	result, err := Remove(input)
	if err != nil {
		t.Fatalf("Remove failed: %v", err)
	}
	if result != input {
		t.Errorf("Remove returned %q, expected %q", result, input)
	}
}

func TestRemoveEmptyString(t *testing.T) {
	result, err := Remove("")
	if err != nil {
		t.Fatalf("Remove failed: %v", err)
	}
	if result != "" {
		t.Errorf("Remove returned %q, expected empty string", result)
	}
}

func TestRemoveUnicodeText(t *testing.T) {
	result, err := Remove("Hello—世界—мир")
	if err != nil {
		t.Fatalf("Remove failed: %v", err)
	}
	expected := "Hello世界мир"
	if result != expected {
		t.Errorf("Remove returned %q, expected %q", result, expected)
	}
}

func TestVersion(t *testing.T) {
	version := Version()
	if version == "" {
		t.Error("Version returned empty string")
	}
}

func TestImplementationName(t *testing.T) {
	impl := ImplementationName()
	if impl == "" {
		t.Error("ImplementationName returned empty string")
	}
	// Should be one of the known implementations
	validImpls := map[string]bool{
		"AVX2":    true,
		"SSE4.2":  true,
		"NEON":    true,
		"scalar":  true,
		"Scalar":  true,
	}
	if !validImpls[impl] {
		t.Logf("ImplementationName: %s (unknown but may be valid)", impl)
	}
}

func TestDetectCPUFeatures(t *testing.T) {
	features := DetectCPUFeatures()
	// Just verify it returns something
	// The actual value depends on the machine it runs on
	t.Logf("CPU Features: 0x%08x", features)
}

func BenchmarkRemove(b *testing.B) {
	input := "Hello—world—from—Go—benchmark—with—many—em—dashes"
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		Remove(input)
	}
}
