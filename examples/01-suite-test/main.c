#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "edlib.h"

// Macros defining the change and position
#define EDIT_START          0
#define EDIT_END            0
#define EDIT_ACTION_TEXT    "MODIFIED_CONTENT_CHUNK\n"

#define VBUF_SIZE           (4 * 1024 * 1024)
#define CHUNK_SIZE          (1 * 1024 * 1024)

// Helper function to calculate elapsed time in milliseconds
static double get_elapsed_ms(struct timespec* start, struct timespec* end) {
    return (end->tv_sec - start->tv_sec) * 1000.0 + (end->tv_nsec - start->tv_nsec) / 1000000.0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> [-r | --read-only]\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    int read_only = 0;

    // Check for read-only flag
    if (argc >= 3) {
        if (strcmp(argv[2], "-r") == 0 || strcmp(argv[2], "--read-only") == 0) {
            read_only = 1;
        }
    }

    struct timespec ts_start, ts_end;

    // --- STEP 1: Open, Stat, mmap, and posix_madvise ---
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file for reading");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("Failed to get file stats");
        close(fd);
        return 1;
    }
    uint64_t file_size = (uint64_t)st.st_size;

    if (file_size == 0) {
        fprintf(stderr, "File is empty.\n");
        close(fd);
        return 1;
    }

    char* mapped_data = (char*)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped_data == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }

    if (posix_madvise(mapped_data, file_size, POSIX_MADV_SEQUENTIAL) != 0) {
        perror("posix_madvise failed");
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    printf("[Timing] Step 1 (Open, mmap & madvise): %.3f ms\n", get_elapsed_ms(&ts_start, &ts_end));


    // --- STEP 2: Load into edlib and apply edit ---
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    ed_piece_buffer_t piece_buffer = empty_ed_piece_buffer_t;
    piece_buffer.piece_capacity = ED_DEFAULT_PIECE_CAPACITY;
    piece_buffer.pieces = malloc(sizeof(ed_piece_t) * piece_buffer.piece_capacity);
    if (!piece_buffer.pieces) {
        perror("Failed to allocate pieces array");
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    if (ed_piece_buffer_load(&piece_buffer, mapped_data, file_size) < 0) {
        fprintf(stderr, "Failed to load piece buffer from mmap data.\n");
        free(piece_buffer.pieces);
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    ed_change_t change = {
        .start            = EDIT_START,
        .end              = EDIT_END,
        .action_data_size = strlen(EDIT_ACTION_TEXT),
        .action_data      = (char*)EDIT_ACTION_TEXT
    };

    if (ed_piece_buffer_change(&piece_buffer, &change) < 0) {
        fprintf(stderr, "Failed to apply change to piece buffer.\n");
        ed_piece_buffer_unload(&piece_buffer);
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    printf("[Timing] Step 2 (Edlib Load & Change):   %.3f ms\n", get_elapsed_ms(&ts_start, &ts_end));


    // --- STEP 3 (Optional / Read-Only mode): Read whole buffer into memory without writing ---
    if (read_only) {
        clock_gettime(CLOCK_MONOTONIC, &ts_start);

        int64_t total_length = ed_piece_buffer_length(&piece_buffer);
        if (total_length < 0) {
            fprintf(stderr, "Failed to get piece buffer length.\n");
        } else {
            char* memory_buffer = malloc(total_length);
            if (!memory_buffer) {
                perror("Failed to allocate memory buffer");
            } else {
                if (ed_piece_buffer_apply(&piece_buffer, 0, memory_buffer, total_length) < 0) {
                    fprintf(stderr, "Failed to apply piece buffer into memory.\n");
                }
                free(memory_buffer);
            }
        }

        // Cleanup input resources
        ed_piece_buffer_unload(&piece_buffer);
        munmap(mapped_data, file_size);
        close(fd);

        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        printf("[Timing] Step 3 (Read into memory only): %.3f ms\n", get_elapsed_ms(&ts_start, &ts_end));
        printf("Read-only mode enabled: File '%s' was NOT modified or written to.\n", filename);
        return 0;
    }

    // --- STEP 4: Setup Temp File, vbuf expansion, and Chunked Writing ---
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    char temp_template[1024];
    snprintf(temp_template, sizeof(temp_template), "%s.tmp.XXXXXX", filename);
    
    int temp_fd = mkstemp(temp_template);
    if (temp_fd < 0) {
        perror("Failed to create temporary file");
        ed_piece_buffer_unload(&piece_buffer);
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    FILE* outfile = fdopen(temp_fd, "w");
    if (!outfile) {
        perror("Failed to fdopen temporary file");
        close(temp_fd);
        unlink(temp_template);
        ed_piece_buffer_unload(&piece_buffer);
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    char* vbuf = (char*)malloc(VBUF_SIZE);
    if (!vbuf) {
        perror("Failed to allocate vbuf memory");
        fclose(outfile);
        unlink(temp_template);
        ed_piece_buffer_unload(&piece_buffer);
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    if (setvbuf(outfile, vbuf, _IOFBF, VBUF_SIZE) != 0) {
        perror("setvbuf failed");
        free(vbuf);
        fclose(outfile);
        unlink(temp_template);
        ed_piece_buffer_unload(&piece_buffer);
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    int64_t total_length = ed_piece_buffer_length(&piece_buffer);
    if (total_length < 0) {
        fprintf(stderr, "Failed to get piece buffer length.\n");
        free(vbuf);
        fclose(outfile);
        unlink(temp_template);
        ed_piece_buffer_unload(&piece_buffer);
        munmap(mapped_data, file_size);
        close(fd);
        return 1;
    }

    char chunk_buffer[CHUNK_SIZE];
    uint64_t offset = 0;
    int write_success = 1;

    while (offset < (uint64_t)total_length) {

        uint64_t bytes_to_read = CHUNK_SIZE;
        if (offset + bytes_to_read > (uint64_t)total_length) {
            bytes_to_read = (uint64_t)total_length - offset;
        }

        if (ed_piece_buffer_apply(&piece_buffer, offset, chunk_buffer, bytes_to_read) < 0) {
            fprintf(stderr, "Failed to apply piece buffer at offset %llu.\n", (unsigned long long)offset);
            write_success = 0;
            break;
        }

        size_t written = fwrite(chunk_buffer, 1, bytes_to_read, outfile);
        if (written != bytes_to_read) {
            perror("fwrite failed or incomplete");
            write_success = 0;
            break;
        }

        offset += bytes_to_read;

    }

    // Clean up input resources
    ed_piece_buffer_unload(&piece_buffer);
    munmap(mapped_data, file_size);
    close(fd);

    // Flush and close temp file
    fflush(outfile);
    free(vbuf);
    fclose(outfile);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    printf("[Timing] Step 4 (Chunked Write & vbuf):  %.3f ms\n", get_elapsed_ms(&ts_start, &ts_end));

    if (!write_success) {
        unlink(temp_template);
        fprintf(stderr, "Failed to write updated file.\n");
        return 1;
    }


    // --- STEP 5: Atomic Rename ---
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    if (rename(temp_template, filename) < 0) {
        perror("Failed to rename temporary file to target");
        unlink(temp_template);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    printf("[Timing] Step 5 (Atomic Rename):       %.3f ms\n", get_elapsed_ms(&ts_start, &ts_end));

    printf("Successfully updated file '%s'!\n", filename);
    return 0;
}