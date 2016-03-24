#include <cstdio>
#include <map>
#include <vector>
#include <string>
#include "input.h"
#include "generate.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s PATCH_SOURCE [PATCH_NAME] [WAV_FILE]\n", argv[0]);
    fprintf(stderr, "  Omitting PATCH_NAME will cause all patch names to "
        "be echoed\n");
    fprintf(stderr, "  When WAV_FILE is not specified, "
        "it will be echoed to stdout\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s patches.inc patch03 | aplay\n", argv[0]);
    fprintf(stderr, "  %s intro.h initPatch kling.wav\n", argv[0]);

    return 1;
  }

  std::map<std::string, std::vector<uint8_t>> patches;
  if (!read_patches(argv[1], patches)) {
    fprintf(stderr, "Failed to parse %s\n", argv[1]);
    return 2;
  }

  if (argc < 3) {
    for (auto &p : patches)
      printf("%s\n", p.first.c_str());
    return 0;
  }

  std::string patch_name = argv[2];
  if (patches.find(patch_name) == patches.end()) {
    fprintf(stderr, "Patch %s not found in %s\n",
        patch_name.c_str(), argv[1]);
    return 3;
  }

  std::vector<int8_t> wave;
  generate_wave(patches[patch_name], wave);
  write_wave(argc >= 4? argv[3] : nullptr, wave);

  return 0;
}
