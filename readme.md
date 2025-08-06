## Overview
This program implements a multi-threaded system for processing, encrypting, and writing data using bounded buffers. This is completed with only one file in the "encrypt-driver.c"

## Threads and Behavior
1. **Reader Thread**: Reads characters from an input file into a bounded buffer.
2. **Input Counter Thread**: Counts characters in the input buffer.
3. **Encryptor Thread**: Encrypts characters from the input buffer and writes them to an encrypted buffer.
4. **Output Counter Thread**: Counts characters in the encrypted buffer.
5. **Writer Thread**: Writes encrypted characters to an output file.
6. **Reset Functionality**: Allows pausing and resetting of the threads for controlled operations.

## Program Flow
1. Initialize buffers based on user-specified sizes.
2. Create threads to handle file reading, processing, encryption, and writing.
3. Synchronize threads using mutexes and condition variables to ensure proper flow and data consistency.
4. Log counts of processed characters.
5. Clean up resources before program termination.

### Compilation
Using make:
```bash
make
```
or with plain gcc:
```bash
gcc -o encryption_program encryption_program.c -pthread
```

### Execution
```bash
./encryption_program <input_file> <output_file> <log_file>
```

### Example
```bash
./encryption_program input.txt output.txt log.txt
```

### Input Parameters
1. **Input Buffer Size**: Must be greater than 1.
2. **Output Buffer Size**: Must be greater than 1.

The program will prompt the user to enter these sizes at runtime.

## File Structure
- **Input File**: The file containing plain text to be processed.
- **Output File**: The file where encrypted text will be written.
- **Log File**: The file for logging character counts during the process.

## Synchronization Details
- **Mutexes**: Used to lock critical sections for thread-safe access to shared buffers.
- **Condition Variables**: Used for signaling between threads to coordinate buffer availability and completion of tasks.

## Cleanup
- Buffers are dynamically allocated and freed after program completion to prevent memory leaks.
- All threads are joined to ensure proper termination.
