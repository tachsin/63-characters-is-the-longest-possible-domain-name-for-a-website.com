// gcc main.c ./.libs/libdogecoin.a -I./include/dogecoin -L./.libs -L/opt/homebrew/opt/libevent/lib -L/opt/homebrew/opt/libunistring/lib -ldogecoin -levent -lunistring -lpthread -o main && ./main
// gcc main.c ./.libs/libdogecoin.a -I./include/dogecoin -L./.libs -ldogecoin -lpthread -levent -lunistring -o main && ./main

#include "libdogecoin.h"
#include <stdio.h>
#include <regex.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#define NUM_THREADS 4  // Define the number of threads here

// Global variables
volatile bool match_found = false;
volatile long total_wallets_generated = 0;
pthread_mutex_t lock;
pthread_mutex_t count_lock;
FILE *output_file;
FILE *log_file;

// Function to check if the address starts with the pattern "D63[cC][hH][a4A]"
int matches_regex(const char *address) {
    regex_t regex;
    int ret;

    // Compile the regular expression to match addresses starting with "D63[cC][hH][a4A]"
    ret = regcomp(&regex, "^D63[cC][hH][a4A]", REG_EXTENDED | REG_NOSUB);
    if (ret) {
        fprintf(stderr, "Could not compile regex\n");
        return 0;
    }

    // Execute the regex match
    ret = regexec(&regex, address, 0, NULL, 0);
    regfree(&regex);

    return (ret == 0);  // Return 1 if match is found, otherwise 0
}

// Thread function to generate addresses until a match is found
void* generate_matching_address(void* thread_id) {
    int addressLen = P2PKHLEN;
    MNEMONIC seed_phrase;
    char address[addressLen];

    while (!match_found) {  // Keep generating until a match is found
        // Generate a new mnemonic and address
        generateRandomEnglishMnemonic("256", seed_phrase);
        getDerivedHDAddressFromMnemonic(0, 0, "0", seed_phrase, NULL, address, false);

        // Increment wallet count
        pthread_mutex_lock(&count_lock);
        total_wallets_generated++;
        pthread_mutex_unlock(&count_lock);

        // Lock to check and set match_found
        pthread_mutex_lock(&lock);
        if (matches_regex(address) && !match_found) {
            match_found = true;
            printf("Thread %ld found a match!\n", (long)thread_id);
            printf("Matching mnemonic: %s\n", seed_phrase);
            printf("Matching address: %s\n", address);

            // Write the matching mnemonic and address to the file
            if (output_file) {
                fprintf(output_file, "Matching mnemonic: %s\n", seed_phrase);
                fprintf(output_file, "Matching address: %s\n", address);
                fflush(output_file);  // Ensure data is written immediately
            }
        }
        pthread_mutex_unlock(&lock);

        // Exit if a match is already found
        if (match_found) break;
    }

    pthread_exit(NULL);
}

// Logging thread to print stats every 10 seconds
void* log_stats(void* arg) {
    time_t start_time = time(NULL);
    while (!match_found) {
        sleep(10);
        time_t current_time = time(NULL);
        double elapsed_time = difftime(current_time, start_time);

        pthread_mutex_lock(&count_lock);
        long wallets_generated = total_wallets_generated;
        pthread_mutex_unlock(&count_lock);

        double wallets_per_second = wallets_generated / elapsed_time;

        if (log_file) {
            fprintf(log_file, "Elapsed time: %.0f seconds\n", elapsed_time);
            fprintf(log_file, "Total wallets generated: %ld\n", wallets_generated);
            fprintf(log_file, "Wallets/second: %.2f\n\n", wallets_per_second);
            fflush(log_file);  // Ensure data is written immediately to the log file
        }
    }
    pthread_exit(NULL);
}

int main() {
    // Initialize the mutexes
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&count_lock, NULL);

    // Initialize the ECC context once at the beginning
    dogecoin_ecc_start();

    // Open file to store the matching wallet
    output_file = fopen("matching_wallet.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Could not open matching_wallet.txt for writing\n");
        return 1;
    }

    // Open file for logging stats
    log_file = fopen("main.log", "w");
    if (!log_file) {
        fprintf(stderr, "Could not open main.log for writing\n");
        fclose(output_file);
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    pthread_t logger_thread;

    // Create logger thread
    if (pthread_create(&logger_thread, NULL, log_stats, NULL)) {
        fprintf(stderr, "Error creating logger thread\n");
        fclose(output_file);
        fclose(log_file);
        return 1;
    }

    // Create wallet generation threads
    for (long i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, generate_matching_address, (void*)i)) {
            fprintf(stderr, "Error creating thread %ld\n", i);
            fclose(output_file);
            fclose(log_file);
            return 1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Wait for logger thread to complete
    pthread_join(logger_thread, NULL);

    // Stop ECC context once after threads finish
    dogecoin_ecc_stop();

    // Clean up
    fclose(output_file);
    fclose(log_file);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&count_lock);
    printf("Finished.\n");

    return 0;
}