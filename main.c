#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <direct.h>
#include <windows.h>

#include "stb/stb_vorbis.c"

#define DR_WAV_IMPLEMENTATION
#include "dr_libs/dr_wav.h"

#define MKDIR(p) _mkdir(p)
#define DEV_NULL "nul"
#define PATH_SEP "\\"

#define MAX_HEADERS 128

#define INPUT_FILE   "vvvvvvmusic.vvv"
#define TEMP_DIR     "temp"
#define MUSIC_DIR    "music"

#define AT3_BITRATE  "64"
#define AT3CONV_EXE  "at3conv"

static char g_wav_dir[512];

typedef struct {
    char name[48];
    int32_t start_UNUSED;
    int32_t size;
    uint8_t valid;
} resourceheader;

static int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

static int ogg_to_wav(const char* ogg_path, const char* wav_path) {
    int channels = 0;
    int sample_rate = 0;
    short* output = NULL;

    int samples = stb_vorbis_decode_filename(ogg_path, &channels, &sample_rate, &output);
    if (samples < 0 || output == NULL) {
        fprintf(stderr, "  stb_vorbis failed to decode %s\n", ogg_path);
        return -1;
    }

    drwav wav;
    drwav_data_format fmt;
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = (drwav_uint16) channels;
    fmt.sampleRate = (drwav_uint32) sample_rate;
    fmt.bitsPerSample = 16;

    if (!drwav_init_file_write(&wav, wav_path, &fmt, NULL)) {
        fprintf(stderr, "  dr_wav failed to open %s for writing\n", wav_path);
        free(output);
        return -1;
    }

    drwav_write_pcm_frames(&wav, (drwav_uint64) samples, output);
    drwav_uninit(&wav);
    free(output);

    return 0;
}

static int mkdir_p(const char* path) {
    char tmp[512];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) {
        return -1;
    }

    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = '\0';
    }

    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            MKDIR(tmp);
            *p = sep;
        }
    }
    if (MKDIR(tmp) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static const char* basename_of(const char* path) {
    const char* p = strrchr(path, '/');
    return p ? p + 1 : path;
}

static void strip_ext(char* name) {
    char* dot = strrchr(name, '.');
    if (dot) {
        *dot = '\0';
    }
}

static int run_cmd(const char* cmd) {
    printf("    $ %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "    ! command returned %d\n", ret);
    }
    return ret;
}

static void chdir_to_exe_dir(void) {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) {
        fprintf(stderr, "Could not determine exe path\n");
        return;
    }
    char* last = strrchr(buf, '\\');
    if (last) {
        *last = '\0';
    }
    if (_chdir(buf) != 0) {
        fprintf(stderr, "Could not chdir to %s\n", buf);
    } else {
        printf("Working directory: %s\n", buf);
    }
}

int main(void) {
    chdir_to_exe_dir();

    if (!file_exists(AT3CONV_EXE ".exe")) {
        fprintf(stderr,
            "\n"
            "========================================================\n"
            "  ERROR: AT3 encoder not found\n"
            "========================================================\n"
            "\n"
            "  %s.exe is missing from the current directory:\n"
            "\n"
            "  You need to put the AT3 encoder next to this program.\n"
            "  It must be named exactly: %s.exe\n"
            "\n"
            "  We cannot distribute it for legal reasons.\n"
            "  You have to obtain it yourself.\n"
            "\n"
            "========================================================\n",
            AT3CONV_EXE, AT3CONV_EXE, AT3CONV_EXE);
        goto done;
    }

    snprintf(g_wav_dir, sizeof(g_wav_dir), "%s" PATH_SEP "wav", TEMP_DIR);

    if (mkdir_p(TEMP_DIR) != 0) {
        fprintf(stderr, "Cannot create %s\n", TEMP_DIR);
        goto done;
    }
    if (mkdir_p(g_wav_dir) != 0) {
        fprintf(stderr, "Cannot create %s\n", g_wav_dir);
        goto done;
    }
    if (mkdir_p(MUSIC_DIR) != 0) {
        fprintf(stderr, "Cannot create %s\n", MUSIC_DIR);
        goto done;
    }

    FILE* f = fopen(INPUT_FILE, "rb");
    if (!f) {
        fprintf(stderr,
            "\n"
            "========================================================\n"
            "  ERROR: %s not found\n"
            "========================================================\n"
            "\n"
            "  Put %s next to this program.\n"
            "\n"
            "========================================================\n",
            INPUT_FILE, INPUT_FILE);
        goto done;
    }

    resourceheader headers[MAX_HEADERS];
    if (fread(headers, sizeof(headers), 1, f) != 1) {
        fprintf(stderr, "Failed to read headers\n");
        fclose(f);
        goto done;
    }

    long offset = sizeof(headers);
    int count = 0;
    int at3_count = 0;

    for (int i = 0; i < MAX_HEADERS; i++) {
        if (headers[i].valid != 1) {
            continue;
        }
        if (headers[i].size <= 0) {
            continue;
        }

        const char* base = basename_of(headers[i].name);
        if (base[0] == '\0') {
            offset += headers[i].size;
            continue;
        }

        char base_noext[256];
        strncpy(base_noext, base, sizeof(base_noext) - 1);
        base_noext[sizeof(base_noext) - 1] = '\0';
        strip_ext(base_noext);

        const char* ext = strrchr(base, '.');
        int is_ogg = (ext && _stricmp(ext, ".ogg") == 0);
        int is_wav = (ext && _stricmp(ext, ".wav") == 0);

        if (is_ogg) {
            unsigned char sig[4];
            fseek(f, offset, SEEK_SET);
            if (fread(sig, 1, 4, f) != 4 || memcmp(sig, "OggS", 4) != 0) {
                printf("[%d] %s (%d bytes) - not OggS, placeholder, skipping\n", ++count, headers[i].name, headers[i].size);
                offset += headers[i].size;
                continue;
            }
        } else if (is_wav) {
            unsigned char sig[4];
            fseek(f, offset, SEEK_SET);
            if (fread(sig, 1, 4, f) != 4 || memcmp(sig, "RIFF", 4) != 0) {
                printf("[%d] %s (%d bytes) - not RIFF, placeholder, skipping\n", ++count, headers[i].name, headers[i].size);
                offset += headers[i].size;
                continue;
            }
        }

        char ogg_path[512];
        char wav_path[512];
        char at3_path[512];
        snprintf(ogg_path, sizeof(ogg_path), "%s" PATH_SEP "%s", TEMP_DIR, base);
        snprintf(wav_path, sizeof(wav_path), "%s" PATH_SEP "%s.wav", g_wav_dir, base_noext);
        snprintf(at3_path, sizeof(at3_path), "%s" PATH_SEP "%s.at3", MUSIC_DIR, base_noext);

        printf("[%d] %s -> %s\n", ++count, headers[i].name, base);

        fseek(f, offset, SEEK_SET);

        FILE* out = fopen(ogg_path, "wb");
        if (!out) {
            perror("fopen out");
            offset += headers[i].size;
            continue;
        }

        char buf[65536];
        int32_t remaining = headers[i].size;
        while (remaining > 0) {
            size_t chunk = (remaining > (int32_t)sizeof(buf)) ? sizeof(buf) : (size_t)remaining;
            if (fread(buf, 1, chunk, f) != chunk) {
                fprintf(stderr, "Short read on %s\n", headers[i].name);
                break;
            }
            fwrite(buf, 1, chunk, out);
            remaining -= chunk;
        }
        fclose(out);
        offset += headers[i].size;

        if (is_ogg) {
            if (ogg_to_wav(ogg_path, wav_path) != 0) {
                fprintf(stderr, "  ogg_to_wav failed for %s\n", base);
                continue;
            }
            printf("    -> %s\n", wav_path);
        } else if (is_wav) {
            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "copy /Y \"%.400s\" \"%.400s\" >%s", ogg_path, wav_path, DEV_NULL);
            if (run_cmd(cmd) != 0) {
                fprintf(stderr, "  failed to copy %s\n", base);
                continue;
            }
        } else {
            printf("    (not ogg or wav, skipping conversion)\n");
            continue;
        }

        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "%s -e -br %s \"%.400s\" \"%.400s\"", AT3CONV_EXE, AT3_BITRATE, wav_path, at3_path);
        if (run_cmd(cmd) != 0) {
            fprintf(stderr, "  at3conv failed to convert %s\n", base);
        } else {
            printf("    -> %s\n", at3_path);
            at3_count++;
        }
    }

    fclose(f);
    printf("\nDone. Extracted %d files, encoded %d AT3 tracks.\n", count, at3_count);
    printf("  OGG:  %s\\\n", TEMP_DIR);
    printf("  WAV:  %s\\\n", g_wav_dir);
    printf("  AT3:  %s\\\n", MUSIC_DIR);

done:
    printf("\nPress Enter to close...");
    fflush(stdout);
    getchar();
    return 0;
}
