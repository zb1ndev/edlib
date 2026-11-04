#if !defined(EDLIB_H)
#define EDLIB_H
#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

#if !defined(internal)
    #define internal                                static
#endif // internal

#if !defined(local_persist)
    #define local_persist                           static
#endif // local_persist

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/*  A single piece within a piece table data structure representing a segment of text.
    @param start The starting offset or index.
    @param end The ending offset or index.
    @param action_data_size The size of the associated action data in bytes.
    @param action_data Pointer to the action data buffer.
    @param lo_data Pointer to the line offset data array.
    @param lo_data_count The number of elements currently in the line offset array.
    @param lo_data_capacity The maximum capacity of the line offset array.
*/
typedef struct ed_piece_t ed_piece_t;
struct ed_piece_t {

    uint64_t            length;
    char*               data;

    uint64_t*           lo_data;                    // @note only gets computed once during initialization. line offsets are relative to their piece.
    uint64_t            lo_data_count,
                        lo_data_capacity;
    
    #define ED_PIECE_ORIGINAL                       (-1LL)

};
#define empty_ed_piece_t                            (ed_piece_t) {0}

/*  Represents a single change or edit action performed on the text buffer.
    @param start The starting offset of the change range.
    @param end The ending offset of the change range.
    @param action_data_size The size of the change action data in bytes.
    @param action_data Pointer to the change data buffer.
*/
typedef struct ed_change_t ed_change_t;
struct ed_change_t {

    uint64_t            start,                      // @note offset relative to original data is equal to (end - start)
                        end;
                        
    uint64_t            action_data_size;           // @note a value of 0 and `action_data` being `NULL` denotes a deletion.
    char*               action_data;

};
#define empty_ed_change_t                           (ed_change_t) {0}

/*  A piece table-based text buffer structure supporting efficient insertions, deletions, and undo/redo operations.
    @param text_data Pointer to the original immutable text data source.
    @param text_data_size The size in bytes of the original text data.
    @param pieces Pointer to the array of text pieces.
    @param piece_count The number of active pieces in the table.
    @param piece_capacity The total allocated capacity for pieces.
    @param changes Pointer to the change log array for undo/redo tracking.
    @param change_count The total number of recorded changes.
    @param change_capacity The total allocated capacity for the change log.
    @param change_cursor The current position in the change log representing the undo/redo boundary.
*/
typedef struct ed_piece_buffer_t  ed_piece_buffer_t;
struct ed_piece_buffer_t {

    const char*         text_data;                  // @note does not get touched or changed ever.
    uint64_t            text_data_size;             // @note does not get touched or changed ever.

    ed_piece_t*         pieces;                     // @note needs to be ordered, maybe a sparse set?
    uint64_t            piece_count, 
                        piece_capacity;

    #define ED_DEFAULT_PIECE_CAPACITY               (128UL)

};
#define empty_ed_piece_buffer_t                     (ed_piece_buffer_t) {0}

/*  Loads initial raw data into the piece buffer.
    @param piece_buffer A pointer to the piece buffer you want to load data into.
    @param data A pointer to the raw text data source.
    @param data_size The size in bytes of the provided text data.
    @returns `-1` on failure and `0` on success.
*/
int8_t                  ed_piece_buffer_load        (ed_piece_buffer_t* piece_buffer, const char* data, uint64_t data_size);

/*  Returns the length of the data represented by the pieces in the piece buffer.
    @param piece_buffer A pointer to the piece buffer you want to get the length of.
    @returns `-1` on failure and the length on success.
*/
int64_t                 ed_piece_buffer_length      (ed_piece_buffer_t* piece_buffer);

/*  Returns the number of lines in the piece buffer.
    @param piece_buffer A pointer to the piece buffer you want to get the line count of.
    @returns `-1` on failure and the line count on success.
*/
int64_t                 ed_piece_buffer_line_count  (ed_piece_buffer_t* piece_buffer);

/*  Returns the global offset of the line in the piece buffer.
    @param piece_buffer A pointer to the piece buffer you want to get the line offset of.
    @param line_number The line number you want to get the global offset of.
    @returns `-1` on failure and the line count on success.
*/
int64_t                 ed_piece_buffer_get_line    (ed_piece_buffer_t* piece_buffer, uint64_t line_number);

/*  Applies a change (insertion or deletion) to the piece buffer across a specified range.
    @param piece_buffer A pointer to the piece buffer you want to modify.
    @param change A pointer to an ed_change_t containing the change data you want to apply. 
    @returns `-1` on failure and `0` on success.
*/
int8_t                  ed_piece_buffer_change      (ed_piece_buffer_t* piece_buffer, ed_change_t* change);

/*  Applies or extracts text data from the piece buffer starting at a specific offset into an output buffer.
    @param piece_buffer A pointer to the piece buffer you want to read from.
    @param offset The target offset within the piece buffer.
    @param out A pointer to the destination buffer where the result will be copied.
    @param out_size The maximum size in bytes of the output buffer.
    @returns `-1` on failure and `0` on success.
*/
int8_t                  ed_piece_buffer_apply       (ed_piece_buffer_t* piece_buffer, uint64_t offset, char* out, uint64_t out_size);

/*  Deinitializes and frees all allocated resources associated with the piece buffer.
    @param piece_buffer A pointer to the piece buffer you want to unload.
    @returns `-1` on failure and `0` on success.
*/
int8_t                  ed_piece_buffer_unload      (ed_piece_buffer_t* piece_buffer);

#if defined(__cplusplus)
}
#endif // __cplusplus
#endif // EDLIB_H
