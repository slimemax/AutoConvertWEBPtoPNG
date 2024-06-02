#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <webp/decode.h>
#include <webp/encode.h>
#include <png.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#define MAX_THREADS 4
#define FILENAME_LENGTH 11
#define MAX_FILENAME_LENGTH 256
#define LOG_FILE "processed_files.log"
#define OUTPUT_DIR "Dalle3Pngs"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t stop_flag = 0;
char **processed_files = NULL;
size_t processed_files_count = 0;
size_t processed_files_capacity = 10;

void generate_random_string(char *str, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_len = sizeof(charset) - 1;
    for (size_t i = 0; i < length; i++) {
        str[i] = charset[rand() % charset_len];
    }
    str[length] = '\0';
}

int is_new_file(const char *filepath) {
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        perror("stat");
        return 0;
    }

    time_t now = time(NULL);
    struct tm *now_tm = localtime(&now);
    now_tm->tm_hour = 0;
    now_tm->tm_min = 0;
    now_tm->tm_sec = 0;
    now_tm->tm_mday -= 1;
    time_t yesterday_start = mktime(now_tm);

    return difftime(file_stat.st_mtime, yesterday_start) > 0;
}

bool is_already_processed(const char *filename) {
    for (size_t i = 0; i < processed_files_count; i++) {
        if (strcmp(processed_files[i], filename) == 0) {
            return true;
        }
    }
    return false;
}

void mark_as_processed(const char *webp_filename, const char *png_filename) {
    pthread_mutex_lock(&mutex);
    if (processed_files_count == processed_files_capacity) {
        processed_files_capacity *= 2;
        processed_files = realloc(processed_files, processed_files_capacity * sizeof(char *));
    }
    processed_files[processed_files_count] = strdup(webp_filename);
    processed_files_count++;
    
    // Log the processed file
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "%s %s\n", webp_filename, png_filename);
        fclose(log_file);
    } else {
        perror("fopen");
    }
    
    pthread_mutex_unlock(&mutex);
}

void load_processed_files() {
    FILE *log_file = fopen(LOG_FILE, "r");
    if (log_file) {
        char webp_filename[MAX_FILENAME_LENGTH];
        char png_filename[MAX_FILENAME_LENGTH];
        
        while (fscanf(log_file, "%255s %255s", webp_filename, png_filename) == 2) {
            if (processed_files_count == processed_files_capacity) {
                processed_files_capacity *= 2;
                processed_files = realloc(processed_files, processed_files_capacity * sizeof(char *));
            }
            processed_files[processed_files_count] = strdup(webp_filename);
            processed_files_count++;
        }
        
        fclose(log_file);
    }
}

int convert_webp_to_png(const char *webp_filename, const char *png_filename) {
    FILE *webp_file = fopen(webp_filename, "rb");
    if (!webp_file) {
        perror("fopen");
        return 1;
    }

    fseek(webp_file, 0, SEEK_END);
    size_t webp_file_size = ftell(webp_file);
    fseek(webp_file, 0, SEEK_SET);

    uint8_t *webp_data = (uint8_t *)malloc(webp_file_size);
    if (!webp_data) {
        perror("malloc");
        fclose(webp_file);
        return 1;
    }

    fread(webp_data, 1, webp_file_size, webp_file);
    fclose(webp_file);

    int width, height;
    uint8_t *raw_data = WebPDecodeRGBA(webp_data, webp_file_size, &width, &height);
    free(webp_data);
    if (!raw_data) {
        fprintf(stderr, "WebPDecodeRGBA failed\n");
        remove(webp_filename); // Delete the corrupted file
        return 1;
    }

    FILE *png_file = fopen(png_filename, "wb");
    if (!png_file) {
        perror("fopen");
        WebPFree(raw_data);
        return 1;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "png_create_write_struct failed\n");
        fclose(png_file);
        WebPFree(raw_data);
        return 1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "png_create_info_struct failed\n");
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(png_file);
        WebPFree(raw_data);
        return 1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during png creation\n");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(png_file);
        WebPFree(raw_data);
        return 1;
    }

    png_init_io(png_ptr, png_file);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < height; y++) {
        png_write_row(png_ptr, raw_data + y * width * 4);
    }

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(png_file);
    WebPFree(raw_data);

    return 0;
}

void *convert_file(void *arg) {
    char *webp_filename = (char *)arg;
    char random_string[FILENAME_LENGTH];
    generate_random_string(random_string, FILENAME_LENGTH - 1);

    char new_filename[MAX_FILENAME_LENGTH];
    snprintf(new_filename, sizeof(new_filename), OUTPUT_DIR "/%s.png", random_string);

    if (convert_webp_to_png(webp_filename, new_filename) == 0) {
        printf("Converted %s to %s\n", webp_filename, new_filename);
        mark_as_processed(webp_filename, new_filename);
    } else {
        fprintf(stderr, "Failed to convert %s\n", webp_filename);
        // Delete the corrupted file (already done in convert_webp_to_png)
    }

    free(webp_filename);
    return NULL;
}

void *monitor_folder(void *arg) {
    (void)arg;  // Unused parameter

    // Create the output directory if it doesn't exist
    struct stat st = {0};
    if (stat(OUTPUT_DIR, &st) == -1) {
        mkdir(OUTPUT_DIR, 0700);
    }

    while (!stop_flag) {
        struct dirent *entry;
        DIR *dp = opendir(".");

        if (dp == NULL) {
            perror("opendir");
            pthread_exit(NULL);
        }

        while ((entry = readdir(dp))) {
            if (stop_flag) break;
            if (strstr(entry->d_name, ".webp") != NULL && is_new_file(entry->d_name) && !is_already_processed(entry->d_name)) {
                pthread_mutex_lock(&mutex);
                char *filename = strdup(entry->d_name);
                pthread_mutex_unlock(&mutex);

                pthread_t thread_id;
                pthread_create(&thread_id, NULL, convert_file, filename);
                pthread_detach(thread_id);
            }
        }

        closedir(dp);
        sleep(10); // Check the folder every 10 seconds
    }
    return NULL;
}

void handle_signal(int signal) {
    stop_flag = 1;
}

int main() {
    srand((unsigned int)time(NULL));
    signal(SIGINT, handle_signal); // Register signal handler for SIGINT

    processed_files = malloc(processed_files_capacity * sizeof(char *));
    
    load_processed_files();
    
    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_folder, NULL);

    // Run the monitor thread as a daemon
    pthread_join(monitor_thread, NULL);

    // Clean up
    for (size_t i = 0; i < processed_files_count; i++) {
        free(processed_files[i]);
    }
    free(processed_files);

    return 0;
}

