{
  "targets": [
    {
      "target_name": "shm_reader",
      "sources": ["shm_reader.cpp"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalOptions": ["/std:c++17", "/EHsc"],
              "ExceptionHandling": "1"
            }
          },
          "libraries": []
        }]
      ]
    }
  ]
}
