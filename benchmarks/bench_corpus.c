/**
 * @file bench_corpus.c
 * @brief Real-world text corpus benchmarks for dash-em
 *
 * This benchmark suite tests performance with real-world text patterns:
 * - Natural language prose (books, articles)
 * - Technical documentation
 * - Source code with comments
 * - Mixed Unicode content
 * - Various natural em-dash densities
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../src/dashem.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

/* Corpus configuration */
#define CORPUS_DIR "./benchmarks/corpus"
#define MAX_FILE_SIZE (10 * 1024 * 1024)  /* 10MB max per file */
#define MIN_FILE_SIZE 1024                 /* 1KB minimum */

/* Timing configuration */
#define WARMUP_RUNS 5
#define BENCHMARK_RUNS 50

typedef struct {
    char* name;
    char* content;
    size_t size;
    size_t emdash_count;
    double density;  /* Em-dashes per KB */
} corpus_file_t;

typedef struct {
    const char* category;
    corpus_file_t* files;
    size_t file_count;
    size_t total_size;
    size_t total_emdashes;
    double avg_density;
} corpus_category_t;

/* Get high-resolution timestamp in microseconds */
static double get_time_us(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int freq_initialized = 0;
    LARGE_INTEGER counter;

    if (!freq_initialized) {
        QueryPerformanceFrequency(&frequency);
        freq_initialized = 1;
    }

    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart * 1000000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
#endif
}

/* Count em-dashes in text */
static size_t count_emdashes(const char* text, size_t len) {
    size_t count = 0;
    for (size_t i = 0; i + 3 <= len; i++) {
        if ((unsigned char)text[i] == 0xE2 &&
            (unsigned char)text[i+1] == 0x80 &&
            (unsigned char)text[i+2] == 0x94) {
            count++;
            i += 2;  /* Skip the rest of the em-dash */
        }
    }
    return count;
}

/* Generate sample corpus files if they don't exist */
static void generate_sample_corpus(void) {
    struct stat st;
    if (stat(CORPUS_DIR, &st) == 0) {
        return;  /* Directory exists */
    }

#ifdef _WIN32
    CreateDirectory(CORPUS_DIR, NULL);
#else
    mkdir(CORPUS_DIR, 0755);
#endif

    /* Create subdirectories */
    const char* categories[] = {"prose", "technical", "code", "mixed"};
    for (size_t i = 0; i < 4; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/%s", CORPUS_DIR, categories[i]);
#ifdef _WIN32
        CreateDirectory(path, NULL);
#else
        mkdir(path, 0755);
#endif
    }

    /* Generate sample files */
    FILE* f;
    char path[256];

    /* 1. Prose with natural em-dash usage */
    snprintf(path, sizeof(path), "%s/prose/sample_article.txt", CORPUS_DIR);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "The History of Computing — A Brief Overview\n\n");
        fprintf(f, "Computing has evolved dramatically over the past century — from mechanical ");
        fprintf(f, "calculators to quantum computers. The journey has been remarkable — each ");
        fprintf(f, "decade bringing innovations that seemed impossible before.\n\n");

        fprintf(f, "Charles Babbage — often called the 'father of computing' — designed the ");
        fprintf(f, "Analytical Engine in the 1830s. This mechanical marvel — though never fully ");
        fprintf(f, "built in his lifetime — contained all the fundamental principles of modern ");
        fprintf(f, "digital computers.\n\n");

        /* Add more content with varying em-dash density */
        for (int para = 0; para < 50; para++) {
            fprintf(f, "Paragraph %d: Technology continues to advance at an unprecedented pace. ", para);
            if (para % 3 == 0) {
                fprintf(f, "This trend — which shows no sign of slowing — has transformed society. ");
            }
            if (para % 5 == 0) {
                fprintf(f, "The implications are far-reaching — affecting education, healthcare, and commerce — ");
                fprintf(f, "in ways we're only beginning to understand. ");
            }
            fprintf(f, "Innovation drives progress forward, creating new possibilities and challenges ");
            fprintf(f, "for future generations to solve.\n\n");
        }
        fclose(f);
    }

    /* 2. Technical documentation with occasional em-dashes */
    snprintf(path, sizeof(path), "%s/technical/api_docs.txt", CORPUS_DIR);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "API Documentation — Version 2.0\n\n");
        fprintf(f, "## Introduction\n\n");
        fprintf(f, "This API provides access to our core services — authentication, data processing, ");
        fprintf(f, "and reporting. All endpoints use REST principles and return JSON responses.\n\n");

        fprintf(f, "## Authentication\n\n");
        fprintf(f, "All API requests require authentication via API keys. Keys can be obtained from ");
        fprintf(f, "the developer portal. Include your key in the Authorization header:\n\n");
        fprintf(f, "    Authorization: Bearer YOUR_API_KEY\n\n");

        fprintf(f, "## Rate Limiting\n\n");
        fprintf(f, "API calls are limited to 1000 requests per hour — this limit applies per API key. ");
        fprintf(f, "If you exceed this limit, you'll receive a 429 status code.\n\n");

        /* Add more technical content */
        for (int section = 0; section < 30; section++) {
            fprintf(f, "### Endpoint %d: /api/v2/resource%d\n\n", section, section);
            fprintf(f, "Method: GET\n");
            fprintf(f, "Description: Retrieves resource data\n");
            if (section % 7 == 0) {
                fprintf(f, "Note: This endpoint — unlike others — requires additional permissions.\n");
            }
            fprintf(f, "Parameters:\n");
            fprintf(f, "  - id: string (required)\n");
            fprintf(f, "  - format: string (optional, default: json)\n\n");
        }
        fclose(f);
    }

    /* 3. Source code with comments */
    snprintf(path, sizeof(path), "%s/code/sample_code.c", CORPUS_DIR);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "/*\n");
        fprintf(f, " * Sample C Code — Demonstration File\n");
        fprintf(f, " * This file contains various functions to test em-dash handling\n");
        fprintf(f, " * in source code contexts.\n");
        fprintf(f, " */\n\n");

        fprintf(f, "#include <stdio.h>\n");
        fprintf(f, "#include <stdlib.h>\n");
        fprintf(f, "#include <string.h>\n\n");

        fprintf(f, "/* Function to process data — handles edge cases carefully */\n");
        fprintf(f, "int process_data(const char* input, size_t len) {\n");
        fprintf(f, "    // Check input — NULL pointers are not allowed\n");
        fprintf(f, "    if (input == NULL) {\n");
        fprintf(f, "        return -1;\n");
        fprintf(f, "    }\n\n");

        fprintf(f, "    /* Main processing loop — optimized for performance */\n");
        fprintf(f, "    for (size_t i = 0; i < len; i++) {\n");
        fprintf(f, "        // Process each byte\n");
        fprintf(f, "        if (input[i] == '\\0') {\n");
        fprintf(f, "            break;  // Found terminator — stop processing\n");
        fprintf(f, "        }\n");
        fprintf(f, "    }\n\n");

        fprintf(f, "    return 0;  // Success — all data processed\n");
        fprintf(f, "}\n\n");

        /* Add more functions */
        for (int func = 0; func < 20; func++) {
            fprintf(f, "/* Helper function %d", func);
            if (func % 4 == 0) {
                fprintf(f, " — performs validation");
            }
            fprintf(f, " */\n");
            fprintf(f, "void helper_%d(void) {\n", func);
            fprintf(f, "    printf(\"Helper %d called\\n\");\n", func);
            fprintf(f, "}\n\n");
        }
        fclose(f);
    }

    /* 4. Mixed Unicode content */
    snprintf(path, sizeof(path), "%s/mixed/unicode_text.txt", CORPUS_DIR);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "Mixed Language Document — 多言語文書\n\n");

        fprintf(f, "This document contains text in multiple languages — testing Unicode handling.\n\n");

        fprintf(f, "English: The quick brown fox — a classic phrase — jumps over the lazy dog.\n");
        fprintf(f, "French: Le renard brun rapide — une phrase classique — saute par-dessus le chien paresseux.\n");
        fprintf(f, "German: Der schnelle braune Fuchs — ein klassischer Satz — springt über den faulen Hund.\n");
        fprintf(f, "Spanish: El rápido zorro marrón — una frase clásica — salta sobre el perro perezoso.\n\n");

        fprintf(f, "Japanese: 速い茶色のキツネ — 古典的なフレーズ — は怠惰な犬を飛び越えます。\n");
        fprintf(f, "Chinese: 快速的棕色狐狸 — 经典短语 — 跳过懒狗。\n");
        fprintf(f, "Korean: 빠른 갈색 여우 — 고전적인 문구 — 게으른 개를 뛰어넘습니다.\n\n");

        fprintf(f, "Mathematics: ∀x ∈ ℝ, x² ≥ 0 — a fundamental inequality — holds universally.\n");
        fprintf(f, "Symbols: ™ © ® § ¶ † ‡ — special characters — require proper encoding.\n\n");

        /* Add more mixed content */
        fprintf(f, "Emojis and special characters:\n");
        fprintf(f, "😀 😃 😄 — happy faces — express emotions\n");
        fprintf(f, "🌍 🌎 🌏 — earth globes — represent our planet\n");
        fprintf(f, "♠ ♣ ♥ ♦ — card suits — used in games\n\n");

        fprintf(f, "Currency symbols: $ € £ ¥ — various currencies — used worldwide\n");
        fprintf(f, "Arrows: → ← ↑ ↓ — directional indicators — show movement\n");

        fclose(f);
    }

    /* 5. Dense em-dash file for stress testing */
    snprintf(path, sizeof(path), "%s/mixed/dense_emdash.txt", CORPUS_DIR);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "Dense Em-dash Test File\n\n");
        fprintf(f, "This—file—contains—many—em-dashes—to—test—performance—under—stress.\n");
        fprintf(f, "Every—word—is—separated—by—an—em-dash—instead—of—spaces.\n");
        fprintf(f, "This—pattern—is—unusual—but—tests—the—algorithm—thoroughly.\n\n");

        for (int i = 0; i < 100; i++) {
            fprintf(f, "Line—%d—contains—multiple—em-dashes—for—testing—", i);
            fprintf(f, "performance—with—high—density—patterns—that—stress—");
            fprintf(f, "the—SIMD—implementation—and—boundary—handling.\n");
        }
        fclose(f);
    }
}

/* Load corpus files from a category */
static corpus_category_t* load_corpus_category(const char* category) {
    corpus_category_t* cat = calloc(1, sizeof(corpus_category_t));
    cat->category = category;

    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", CORPUS_DIR, category);

    DIR* dir = opendir(dir_path);
    if (!dir) {
        return cat;  /* Empty category */
    }

    /* Count files first */
    struct dirent* entry;
    size_t file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            file_count++;
        }
    }

    if (file_count == 0) {
        closedir(dir);
        return cat;
    }

    /* Allocate file array */
    cat->files = calloc(file_count, sizeof(corpus_file_t));
    rewinddir(dir);

    /* Load files */
    size_t idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < file_count) {
        if (entry->d_name[0] == '.') continue;

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        /* Get file size */
        struct stat st;
        if (stat(file_path, &st) != 0 || st.st_size < MIN_FILE_SIZE ||
            st.st_size > MAX_FILE_SIZE) {
            continue;
        }

        /* Load file content */
        FILE* f = fopen(file_path, "rb");
        if (!f) continue;

        corpus_file_t* file = &cat->files[idx];
        file->name = strdup(entry->d_name);
        file->size = st.st_size;
        file->content = malloc(file->size + 1);

        size_t read = fread(file->content, 1, file->size, f);
        fclose(f);

        if (read != file->size) {
            free(file->content);
            free(file->name);
            continue;
        }

        file->content[file->size] = '\0';

        /* Count em-dashes and calculate density */
        file->emdash_count = count_emdashes(file->content, file->size);
        file->density = (file->emdash_count * 1024.0) / file->size;

        cat->total_size += file->size;
        cat->total_emdashes += file->emdash_count;

        idx++;
    }

    cat->file_count = idx;
    if (cat->total_size > 0) {
        cat->avg_density = (cat->total_emdashes * 1024.0) / cat->total_size;
    }

    closedir(dir);
    return cat;
}

/* Free corpus category */
static void free_corpus_category(corpus_category_t* cat) {
    if (!cat) return;

    for (size_t i = 0; i < cat->file_count; i++) {
        free(cat->files[i].name);
        free(cat->files[i].content);
    }
    free(cat->files);
    free(cat);
}

/* Benchmark a single file */
static void benchmark_file(corpus_file_t* file, int runs) {
    size_t output_capacity = file->size + 1024;
    char* output = malloc(output_capacity);
    char* naive_output = malloc(output_capacity);

    /* Warmup */
    for (int i = 0; i < WARMUP_RUNS; i++) {
        size_t output_len;
        dashem_remove(file->content, file->size, output, output_capacity, &output_len);
    }

    /* Benchmark dashem */
    double total_time = 0.0;
    size_t output_len = 0;

    for (int i = 0; i < runs; i++) {
        double start = get_time_us();
        dashem_remove(file->content, file->size, output, output_capacity, &output_len);
        double end = get_time_us();
        total_time += (end - start);
    }

    double avg_time = total_time / runs;

    /* Benchmark naive */
    double naive_total = 0.0;
    size_t naive_len = 0;

    for (int i = 0; i < runs; i++) {
        double start = get_time_us();

        /* Naive implementation */
        size_t in_idx = 0, out_idx = 0;
        while (in_idx < file->size) {
            if (in_idx + 3 <= file->size &&
                (unsigned char)file->content[in_idx] == 0xE2 &&
                (unsigned char)file->content[in_idx+1] == 0x80 &&
                (unsigned char)file->content[in_idx+2] == 0x94) {
                in_idx += 3;
            } else {
                naive_output[out_idx++] = file->content[in_idx++];
            }
        }
        naive_len = out_idx;

        double end = get_time_us();
        naive_total += (end - start);
    }

    double naive_avg = naive_total / runs;

    /* Verify correctness */
    bool correct = (output_len == naive_len &&
                   memcmp(output, naive_output, output_len) == 0);

    /* Calculate metrics */
    double throughput_gbps = (file->size / (1024.0 * 1024.0 * 1024.0)) /
                            (avg_time / 1000000.0);
    double speedup = naive_avg / avg_time;

    /* Output results */
    printf("  %-30s %8zu %6zu %5.2f | %8.1f μs | %6.2f GB/s | %6.2fx | %s\n",
           file->name, file->size, file->emdash_count, file->density,
           avg_time, throughput_gbps, speedup,
           correct ? "PASS" : "FAIL");

    free(output);
    free(naive_output);
}

/* Benchmark a category */
static void benchmark_category(corpus_category_t* cat) {
    if (!cat || cat->file_count == 0) {
        printf("Category '%s': No files found\n\n", cat->category);
        return;
    }

    printf("Category: %s\n", cat->category);
    printf("  Files: %zu, Total size: %.2f KB, Avg density: %.2f em-dash/KB\n",
           cat->file_count, cat->total_size / 1024.0, cat->avg_density);
    printf("  %-30s %8s %6s %5s | %10s | %10s | %8s | %6s\n",
           "File", "Size", "Em-dash", "/KB", "Time", "Throughput", "Speedup", "Valid");
    printf("  %s\n", "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─"
           "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─"
           "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─"
           "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─"
           "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─"
           "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─" "─");

    for (size_t i = 0; i < cat->file_count; i++) {
        benchmark_file(&cat->files[i], BENCHMARK_RUNS);
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    printf("dash-em Real-World Corpus Benchmark\n");
    printf("====================================\n");
    printf("Implementation: %s\n", dashem_implementation_name());
    printf("Version: %s\n", dashem_version());
    printf("CPU Features: 0x%08X\n\n", dashem_detect_cpu_features());

    /* Ensure corpus exists */
    generate_sample_corpus();

    /* Categories to test */
    const char* categories[] = {"prose", "technical", "code", "mixed"};
    size_t num_categories = sizeof(categories) / sizeof(categories[0]);

    /* Benchmark each category */
    for (size_t i = 0; i < num_categories; i++) {
        corpus_category_t* cat = load_corpus_category(categories[i]);
        benchmark_category(cat);
        free_corpus_category(cat);
    }

    /* Summary statistics */
    printf("Summary\n");
    printf("=======\n");
    printf("Corpus directory: %s\n", CORPUS_DIR);
    printf("Warmup runs: %d\n", WARMUP_RUNS);
    printf("Benchmark runs: %d per file\n", BENCHMARK_RUNS);
    printf("\nNote: Generate more corpus files in %s/ for comprehensive testing.\n", CORPUS_DIR);

    return 0;
}