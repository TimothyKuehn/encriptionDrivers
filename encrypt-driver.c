#include <stdio.h>
#include <stdlib.h>
#include "encrypt-module.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

// Booleans to control reader and encryptor threads
bool can_run_read = true;
bool can_run_encrypt = true;

// Flag for reset
bool reset_flag = 0;

// Size of the buffers (set at runtime)
int read_buffer_size;
int encrypted_buffer_size;

// Set up the mutexes for reseting the reader
pthread_mutex_t reset_reader_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t encryptor_mutex = PTHREAD_MUTEX_INITIALIZER;

// Set up all of the pthread signaling conds
pthread_cond_t reset_subprocess_complete = PTHREAD_COND_INITIALIZER;
pthread_cond_t reader_subprocess_complete = PTHREAD_COND_INITIALIZER;
pthread_cond_t encryptor_subprocess_complete = PTHREAD_COND_INITIALIZER;
pthread_cond_t reader_paused = PTHREAD_COND_INITIALIZER;
pthread_cond_t reset_completed = PTHREAD_COND_INITIALIZER;

// Buffer struct to hold all of the data about our 3 access buffer
typedef struct
{
    char *buffer;
    int producer_index;
    int count_index;
    int consumer_index;
    int num_of_count_items;
    int num_of_consume_items;
} Buffer;

// Create read and encrypted buffer
Buffer read_buffer;
Buffer encrypted_buffer;

// Fill the API requirements for resetting
void reset_requested()
{
    pthread_mutex_lock(&reset_reader_mutex);
    // Let the reader know we are pausing it
    reset_flag = true;
    // Wait for acknowledgement
    pthread_cond_wait(&reader_paused, &reset_reader_mutex);
    // Call Log API
    log_counts();
}

void reset_finished()
{
    // Start reader again
    reset_flag = false;
    pthread_cond_signal(&reset_completed);
    // Back to our regularly scheduled program
    pthread_mutex_unlock(&reset_reader_mutex);
}

// Thread to read from the file into our reader buffer
void *reader_thread(void *v)
{
    printf("Starting Reader Thread\n");
    char c;

    // While we have more to read
    while ((c = read_input()) != EOF)
    {
        // Don't allow reset mid cycle
        pthread_mutex_lock(&reset_reader_mutex);

        // If we are resetting
        if (reset_flag == true)
        {
            pthread_mutex_lock(&reader_mutex);
            read_buffer.buffer[read_buffer.producer_index] = c;
            read_buffer.producer_index = (read_buffer.producer_index + 1) % read_buffer_size;

            read_buffer.num_of_count_items++;
            read_buffer.num_of_consume_items++;

            while (read_buffer.num_of_count_items > 1 || read_buffer.num_of_consume_items > 1 || encrypted_buffer.num_of_count_items > 1 || encrypted_buffer.num_of_consume_items > 1)
            {
                pthread_cond_wait(&reset_subprocess_complete, &reader_mutex);
            }
            pthread_mutex_unlock(&reader_mutex);
            pthread_cond_signal(&reader_paused);
            pthread_cond_wait(&reset_completed, &reset_reader_mutex);
        }

        // If the buffer is full, wait until it isn't
        while (read_buffer.num_of_count_items == read_buffer_size || read_buffer.num_of_consume_items == read_buffer_size)
        {
            pthread_cond_wait(&reader_subprocess_complete, &reset_reader_mutex);
        }

        // Read new item into our buffer 
        read_buffer.buffer[read_buffer.producer_index] = c;
        // Circular buffering
        read_buffer.producer_index = (read_buffer.producer_index + 1) % read_buffer_size;
        pthread_mutex_unlock(&reset_reader_mutex);
        pthread_mutex_lock(&reader_mutex);
        // Keep track of how much space in the buffer for each consumer
        read_buffer.num_of_count_items++;
        read_buffer.num_of_consume_items++;
        pthread_mutex_unlock(&reader_mutex);
    }
    // End condition
    can_run_read = false;
    printf("Finished Reader Thread\n");

    return NULL;
}

void *input_counter_thread(void *v)
{
    printf("Starting Input Counter Thread\n");\

    // While reader is not finished
    while (can_run_read == true)
    {
        // When we have stuff to count
        while (read_buffer.num_of_count_items > 0)
        {
            // Read the location in read buffer and count it
            char c = read_buffer.buffer[read_buffer.count_index];
            read_buffer.count_index = (read_buffer.count_index + 1) % read_buffer_size;
            count_input(c);
            pthread_mutex_lock(&reader_mutex);
            // Keep track of how many items we have read
            read_buffer.num_of_count_items--;
            pthread_mutex_unlock(&reader_mutex);
            pthread_cond_signal(&reader_subprocess_complete);
        }
        // Let reset and reader know we are done with everything on our plate
        pthread_cond_signal(&reader_subprocess_complete);
        pthread_cond_signal(&reset_subprocess_complete);
    }
    // Finish thread
    printf("Finished Input Counter Thread\n");
    return NULL;
}

void *encryptor_thread(void *v)
{
    printf("Starting Encryptor Thread\n");
    // Space for return from the encrypt
    int encrypted_char;
    // While we are not done
    while (can_run_read == true)
    {
        // While there are things to encrypt
        while (read_buffer.num_of_consume_items > 0)
        {
            // Get the char
            char c = read_buffer.buffer[read_buffer.consumer_index];
            read_buffer.consumer_index = (read_buffer.consumer_index + 1) % read_buffer_size;

            // Encrypt the char
            encrypted_char = encrypt(c);

            pthread_mutex_lock(&reader_mutex);
            // Keep track of how many items in the reader buffer
            read_buffer.num_of_consume_items--;

            // If any of the encrypt threads are backlogged, wait
            while (encrypted_buffer.num_of_count_items == encrypted_buffer_size || encrypted_buffer.num_of_consume_items == encrypted_buffer_size)
            {
                pthread_cond_wait(&encryptor_subprocess_complete, &reader_mutex);
            }
            pthread_mutex_unlock(&reader_mutex);

            // Add the encrypted char to the encrypted buffer
            encrypted_buffer.buffer[encrypted_buffer.producer_index] = encrypted_char;
            encrypted_buffer.producer_index = (encrypted_buffer.producer_index + 1) % encrypted_buffer_size;

            pthread_mutex_lock(&encryptor_mutex);
            // Keep track of slots in the encrypted buffer
            encrypted_buffer.num_of_count_items++;
            encrypted_buffer.num_of_consume_items++;
            pthread_mutex_unlock(&encryptor_mutex);
            pthread_cond_signal(&reader_subprocess_complete);
        }
        // Let the reader and reset know that you have finished
        pthread_cond_signal(&reset_subprocess_complete);
        pthread_cond_signal(&reader_subprocess_complete);
    }
    
    // End the second half threads
    can_run_encrypt = false;
    // Finished
    printf("Finished Encryptor Thread\n");
    return NULL;
}

void *output_counter_thread(void *v)
{
    printf("Starting Output Counter Thread\n");
    // While encryptor is not finished
    while (can_run_encrypt == true)
    {
        // While we have things to process
        while (encrypted_buffer.num_of_count_items > 0)
        {
            // Read the buffer
            char c = encrypted_buffer.buffer[encrypted_buffer.count_index];
            encrypted_buffer.count_index = (encrypted_buffer.count_index + 1) % encrypted_buffer_size;
            // Count the item from buffer
            count_output(c);
            pthread_mutex_lock(&encryptor_mutex);
            // Keep track of available slots
            encrypted_buffer.num_of_count_items--;
            pthread_mutex_unlock(&encryptor_mutex);
            // Let encryptor know you are done
            pthread_cond_signal(&encryptor_subprocess_complete);
        }
        // Let reset know you are done
        pthread_cond_signal(&reset_subprocess_complete);
    }
    printf("Finished Output Counter Thread\n");
    return NULL;
}

void *writer_thread(void *v)
{
    printf("Starting Writer Thread\n");
    // While the encryptor is not done
    while (can_run_encrypt == true)
    {
        // While there are things to process
        while (encrypted_buffer.num_of_consume_items > 0)
        {
            // Get the char from the encrypted buffer
            char c = encrypted_buffer.buffer[encrypted_buffer.consumer_index];
            encrypted_buffer.consumer_index = (encrypted_buffer.consumer_index + 1) % encrypted_buffer_size;
            // Output
            write_output(c);
            pthread_mutex_lock(&encryptor_mutex);
            // Keep track of how many slots are open in the buffer
            encrypted_buffer.num_of_consume_items--;
            pthread_mutex_unlock(&encryptor_mutex);
            pthread_cond_signal(&encryptor_subprocess_complete);
        }
        
        // Let the reset know writer is done
        pthread_cond_signal(&reset_subprocess_complete);
    }
    // Finished
    printf("Finished Writer Thread\n");
    return NULL;
}


void init_buffer(Buffer* buffer, int size)
{
    // Malloc the space for our buffer
    buffer->buffer = malloc(size * sizeof(char));
    if (buffer->buffer == NULL)
    {
        perror("Failed to allocate memory for buffer");
        exit(EXIT_FAILURE);
    }
    // Set all internal struct values to 0
    buffer->producer_index = 0;
    buffer->count_index = 0;
    buffer->consumer_index = 0;
    buffer->num_of_count_items = 0;
    buffer->num_of_consume_items = 0;
}

void teardown_buffer(Buffer* buffer)
{
    free(buffer->buffer);
}

int main(int argc, char *argv[])
{
     if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <input file> <output file> <log file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Retrieve file names from command-line arguments
    char *inputFile = argv[1];
    char *outputFile = argv[2];
    char *logFile = argv[3];

    // Call the init function with the filenames
    init(inputFile, outputFile, logFile);

    // Check the size of the input buffer
    printf("Enter input buffer size (N > 1): ");
    if (scanf("%d", &read_buffer_size) != 1 || read_buffer_size <= 1)
    {
        fprintf(stderr, "Invalid input buffer size. Must be greater than 1.\n");
        return EXIT_FAILURE;
    }

    // Check the size of the encrypt buffer
    printf("Enter output buffer size (M > 1): ");
    if (scanf("%d", &encrypted_buffer_size) != 1 || encrypted_buffer_size <= 1)
    {
        fprintf(stderr, "Invalid output buffer size. Must be greater than 1.\n");
        return EXIT_FAILURE;
    }

    // Display the buffer sizes for confirmation
    printf("Input Buffer Size: %d\n", read_buffer_size);
    printf("Output Buffer Size: %d\n", encrypted_buffer_size);

    // Initialize the buffers to the correct size
    init_buffer(&read_buffer, read_buffer_size);
    init_buffer(&encrypted_buffer, encrypted_buffer_size);

    // Initialize local threads and identifiers
    pthread_t reader_thread_l;
    pthread_t input_counter_thread_l;
    pthread_t encryptor_thread_l;
    pthread_t output_counter_thread_l;
    pthread_t writer_thread_l;

    // Create the local threads 
    pthread_create(&reader_thread_l, NULL, reader_thread, NULL);
    pthread_create(&input_counter_thread_l, NULL, input_counter_thread, NULL);
    pthread_create(&encryptor_thread_l, NULL, encryptor_thread, NULL);
    pthread_create(&output_counter_thread_l, NULL, output_counter_thread, NULL);
    pthread_create(&writer_thread_l, NULL, writer_thread, NULL);

    // Finish the threads
    pthread_join(reader_thread_l, NULL);
    pthread_join(input_counter_thread_l, NULL);
    pthread_join(encryptor_thread_l, NULL);
    pthread_join(output_counter_thread_l, NULL);
    pthread_join(writer_thread_l, NULL);

    printf("End of file reached.\n");

    log_counts();

    // Free our Mallocs from before finishing main
    teardown_buffer(&read_buffer);
    teardown_buffer(&encrypted_buffer);
}

