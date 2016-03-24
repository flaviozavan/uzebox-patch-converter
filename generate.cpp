#include <vector>
#include <cstdint>
#include <algorithm>
#include "generate.h"
#include "waves.h"
#include "step_table.h"

bool write_wave(const char *fn, const std::vector<int8_t> &data) {
  const uint32_t subchunk2_size = data.size() & 1? data.size()+1 : data.size();
  const uint32_t chunk_size = subchunk2_size + 36;
  const uint32_t sample_rate = SAMPLE_RATE;
  FILE *out;
  out = fn? fopen(fn, "wb") : stdout;
  if (!out)
    return false;

  /* ChunkID */
  fprintf(out, "RIFF");
  /* ChunkSize */
  putc((chunk_size) & 0xff, out);
  putc((chunk_size>>8) & 0xff, out);
  putc((chunk_size>>16) & 0xff, out);
  putc((chunk_size>>24) & 0xff, out);
  /* Format */
  fprintf(out, "WAVE");
  /* Subchunk1ID */
  fprintf(out, "fmt ");
  /* Subchunk1Size*/
  putc(16, out);
  putc(0, out);
  putc(0, out);
  putc(0, out);
  /* AudioFormat */
  putc(1, out);
  putc(0, out);
  /* NumChannels */
  putc(1, out);
  putc(0, out);
  /* SampleRate */
  putc((sample_rate) & 0xff, out);
  putc((sample_rate>>8) & 0xff, out);
  putc((sample_rate>>16) & 0xff, out);
  putc((sample_rate>>24) & 0xff, out);
  /* ByteRate */
  putc((sample_rate) & 0xff, out);
  putc((sample_rate>>8) & 0xff, out);
  putc((sample_rate>>16) & 0xff, out);
  putc((sample_rate>>24) & 0xff, out);
  /* BlockAlign */
  putc(1, out);
  putc(0, out);
  /* BitsPerSample */
  putc(8, out);
  putc(0, out);
  /* Subchunk2ID */
  fprintf(out, "data");
  /* Subchunk2Size */
  putc((subchunk2_size) & 0xff, out);
  putc((subchunk2_size>>8) & 0xff, out);
  putc((subchunk2_size>>16) & 0xff, out);
  putc((subchunk2_size>>24) & 0xff, out);
  /* Data */
  for (auto d : data) {
    int dd = d;
    dd += 128;
    putc(dd, out);
  }

  /* Padding */
  if (data.size() & 1)
    putc(0, out);

  if (fn)
    fclose(out);

  return true;
}

bool generate_wave(const std::vector<uint8_t> &patch,
    std::vector<int8_t> &data) {
  data.clear();
  int8_t note = 80;
  uint16_t next_sample = 0;
  uint8_t note_volume = DEFAULT_VOLUME;
  uint8_t envelope_volume = 0xff;
  int8_t envelope_step = 0;
  int wave = 0;
  uint8_t tremolo_level = 0;
  uint8_t tremolo_rate = 24;
  uint8_t tremolo_pos = 0;
  uint8_t loop_count = 0;
  uint8_t slide_speed = 0x10;
  int16_t slide_step = 0;
  int8_t slide_note = 0;
  bool sliding = false;
  uint16_t track_step = 0;
  data.clear();

  for (size_t i = 0; i < patch.size(); i += 3) {
    for (int delay = patch[i]; delay; delay--) {
      int16_t e_vol = envelope_volume + envelope_step;
      e_vol = std::max((int16_t) 0, std::min((int16_t) 0xff, e_vol));
      envelope_volume = e_vol;

      if (sliding) {
        track_step += slide_step;
        uint16_t t_step = step_table[(int) slide_note];

        if ((slide_step > 0 && track_step >= t_step)
            || (slide_step < 0 && track_step <= t_step)) {
          track_step = t_step;
          sliding = false;
        }
      }

      uint16_t vol = note_volume;
      if (note_volume && envelope_volume) {
        vol = ((vol*envelope_volume)+0x100) >> 8;

        /* Assumes the master volume is 0xff, no calculation needed */

        if (tremolo_level > 0) {
          uint8_t t = ((uint8_t *) waves[0])[tremolo_pos];
          t -= 128;
          uint16_t t_vol = (tremolo_level*t)+0x100;
          t_vol >>= 8;
          vol = ((vol*(0xff-t_vol)) + 0x100) >> 8;
        }
      }
      else
        vol = 0;

      tremolo_pos += tremolo_rate;

      for (int j = 0; j < SAMPLES_PER_FRAME; j++) {
        int8_t sample = waves[wave][next_sample>>8];
        next_sample += track_step;
        int16_t v16 = (int16_t) sample * vol;
        /* Signed extention */
        int8_t v8 = v16 / 256;
        data.push_back(v8);
      }
    }

    if (patch[i+1] == PATCH_END || patch[i+1] == PC_NOTE_CUT)
      break;

    int current;
    int target;
    switch (patch[i+1]) {
      case PC_ENV_SPEED:
        envelope_step = ((int8_t *) patch.data())[i+2];
        break;

      /* TODO */
      case PC_NOISE_PARAMS:
        break;

      case PC_WAVE:
        wave = patch[i+2];
	if (wave >= NUM_WAVES) {
	  fprintf(stderr, "Command %lu: Invalid wave\n", i/3);
	  return false;
	}
        break;

      case PC_NOTE_UP:
        note += ((int8_t *) patch.data())[i+2];
	if (note > 126 || note < 0) {
	  fprintf(stderr, "Command %lu: Invalid note\n", i/3);
	  return false;
	}
        track_step = step_table[(int) note];
        break;

      case PC_NOTE_DOWN:
        note -= ((int8_t *) patch.data())[i+2];
	if (note > 126 || note < 0) {
	  fprintf(stderr, "Command %lu: Invalid note\n", i/3);
	  return false;
	}
        track_step = step_table[(int) note];
        break;

      /* TODO */
      case PC_NOTE_HOLD:
        break;

      case PC_ENV_VOL:
        envelope_volume = patch[i+2];
        break;

      case PC_PITCH:
        note = ((int8_t *) patch.data())[i+2];
	if (note > 126 || note < 0) {
	  fprintf(stderr, "Command %lu: Invalid note\n", i/3);
	  return false;
	}
        track_step = step_table[(int) note];
        sliding = false;
        break;

      case PC_TREMOLO_LEVEL:
        tremolo_level = patch[i+2];
        break;

      case PC_TREMOLO_RATE:
        tremolo_rate = patch[i+2];
        break;

      case PC_SLIDE:
        current = step_table[(int) note];
        slide_note = note + ((int8_t *) patch.data())[i+2];
	if (slide_note > 126 || slide_note < 0) {
	  fprintf(stderr, "Command %lu: Invalid slide note\n", i/3);
	  return false;
	}
        target = step_table[(int) slide_note];
        slide_step = std::max(1, (target-current)/slide_speed);
        track_step += slide_step;
        break;

      case PC_SLIDE_SPEED:
        slide_speed = patch[i+2];
        break;

      case PC_LOOP_END:
        if (loop_count <= 0)
          break;
        else if (patch[i+2] > 0) {
          i -= 3*(i+1);
        }
        else {
          do {
            i -= 3;
          } while(i >= 4 && patch[i+1] != PC_LOOP_START);
        }
        break;

      case PC_LOOP_START:
        loop_count = patch[i+2];

      default:
        break;
    }
  }

  return true;
}
