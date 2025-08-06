#ifndef ENCRYPT_H
#define ENCRYPT_H


void reset_requested();

void reset_finished();

void init(char *inputFileName, char *outputFileName, char *logFileName);
int read_input();
void write_output(int c);
void log_counts();
int encrypt(int c);
void count_input(int c);
void count_output(int c);
int get_input_count(int c);
int get_output_count(int c);
int get_input_total_count();
int get_output_total_count();

#endif // ENCRYPT_H

