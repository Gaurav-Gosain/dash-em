{
  "targets": [
    {
      "target_name": "dashem",
      "sources": [
        "dashem_addon.c",
        "../../src/dashem.c"
      ],
      "include_dirs": [
        "../../src"
      ],
      "cflags": [
        "-march=native",
        "-O3",
        "-Wall",
        "-Wextra"
      ],
      "cflags_cc": [
        "-march=native",
        "-O3",
        "-Wall",
        "-Wextra"
      ],
      "conditions": [
        [
          "OS=='win'",
          {
            "msbuild_settings": {
              "VCCLCompilerTool": {
                "AdditionalOptions": ["/arch:AVX2"]
              }
            }
          }
        ]
      ]
    }
  ]
}
