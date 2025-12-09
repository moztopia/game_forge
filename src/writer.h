#ifndef WRITER_H
#define WRITER_H

#include "board.h"

void write_csv_header(const char* filename);
void write_csv_row(const char* filename, board_t* board);

#endif // WRITER_H
