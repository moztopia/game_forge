#ifndef WRITER_H
#define WRITER_H

void write_csv_header(const char* filename, const char* game_header, int append);
void write_csv_row(const char* filename, 
                   const char* difficulty, 
                   unsigned int seed, 
                   double score, 
                   const char* game_data);

#endif // WRITER_H
