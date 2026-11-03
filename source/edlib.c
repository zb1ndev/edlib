#include <edlib.h>

internal void _ed_piece_free(ed_piece_t* piece) {

    if (piece == NULL)
        return;

    if (piece->lo_data != NULL)
        free(piece->lo_data);
    *piece = empty_ed_piece_t;

}

internal inline int8_t _ed_append_line_offset(ed_piece_t* piece, uint64_t offset) {

    if (piece->lo_data_count + 1 >= piece->lo_data_capacity) {

        uint64_t* temp = realloc(piece->lo_data, piece->lo_data_capacity << 1);
        if (temp == NULL) {
            fprintf(stderr, "error: _ed_append_line_offset: realloc\n");
            return -1;
        }

        piece->lo_data_capacity <<= 1;
        piece->lo_data = temp;

    }

    piece->lo_data[piece->lo_data_count++] = offset;
    return 0;

}

internal int8_t _ed_piece_buffer_reserve(ed_piece_buffer_t* piece_buffer, uint64_t needed) {

    if (needed <= piece_buffer->piece_capacity)
        return 0;

    uint64_t capacity = piece_buffer->piece_capacity == 0
        ? piece_buffer->piece_capacity 
        : ED_DEFAULT_PIECE_CAPACITY;

    while (capacity < needed)
        capacity <<= 1;

    ed_piece_t* temp = realloc(piece_buffer->pieces, sizeof(ed_piece_t) * capacity);
    if (temp == NULL) {
        fprintf(stderr, "error: _ed_piece_buffer_reserve: realloc\n");
        return -1;
    }

    piece_buffer->pieces         = temp;
    piece_buffer->piece_capacity = capacity;

    return 0;

}

internal int8_t _ed_piece_compute_lines(ed_piece_buffer_t* piece_buffer, ed_piece_t* piece) {

    uint64_t size       = piece->length;
    const char* data    = piece->data;
    const char* stop    = data + size;

    // @note this is just a fast line count approximation
    piece->lo_data_capacity = (size >> 5) + (size & 31);
    if (piece->lo_data_capacity == 0)
        piece->lo_data_capacity = 5;

    if ((piece->lo_data = malloc(piece->lo_data_capacity << 3)) == NULL) {
        fprintf(stderr, "error: _ed_piece_compute_lines: malloc\n");
        return -1;
    }

    char* walker = data;
    piece->lo_data_count = 0;

    while (walker < stop) {

        char* found = memchr(walker, '\n', (uint64_t)(stop - walker));
        if (found == NULL)
            break;

        if (_ed_append_line_offset(piece, (uint64_t)(found - data)) < 0)
            return -1;    
        walker = found + 1;

    }

    return 0;

}

internal int8_t _ed_piece_buffer_split(ed_piece_buffer_t* piece_buffer, uint64_t offset, uint64_t* index_out) {

    if (offset == 0) {
        *index_out = 0;
        return 0;
    }

    // @note index_out should be the right piece index, 
    // since the left should only ever shrink. It is also
    // worth noting that line offsets are relative to the 
    // piece, they are not global.

    uint64_t position = 0; 
    for (uint64_t i = 0; i < piece_buffer->piece_count; i++) {

        uint64_t piece_length = piece_buffer->pieces[i].length;

        if (offset < position + piece_length) {

            uint64_t pr_offset  = offset - position; // @note piece relative offset
            ed_piece_t* left    = &piece_buffer->pieces[i];

            if (_ed_piece_buffer_reserve(piece_buffer, piece_buffer->piece_count + 1) < 0)
                return -1;
            left->length = offset - position;

            ed_piece_t right = (ed_piece_t) {
                .length = (position + piece_length) - offset,
                .data = left->data + left->length,
            };

            uint64_t current_lo = 0;
            while (current_lo++ < left->lo_data_count) {
                if (left->lo_data[current_lo] <= left->length)
                    continue;
                left->lo_data_count = current_lo;
                right.lo_data = &left->lo_data[current_lo];
                right.lo_data_count = left->lo_data_count - current_lo;
                break;
            }
            
            memmove(&piece_buffer->pieces[i + 2], &piece_buffer->pieces[i + 1], sizeof(ed_piece_t) * (piece_buffer->piece_count - (i + 1)));
            piece_buffer->pieces[i + 1] = right;
            piece_buffer->piece_count++;

            *index_out = i + 1;
            return 0;

        }

        position += piece_length;

        if (offset == position) {
            *index_out = i + 1;
            return 0;
        }

    }    

    fprintf(stderr, "error: _ed_piece_buffer_split: offset out of range\n");
    return -1;

}


int8_t ed_piece_buffer_load(ed_piece_buffer_t* piece_buffer, const char* data, uint64_t data_size) {

    if (piece_buffer == NULL) {
        fprintf(stderr, "error: ed_piece_buffer_load: invalid arguments\n");
        return -1;
    }

    piece_buffer->text_data         = data;
    piece_buffer->text_data_size    = data_size;
    piece_buffer->piece_capacity    = ED_DEFAULT_PIECE_CAPACITY;

    if ((piece_buffer->pieces = malloc(piece_buffer->piece_capacity * sizeof(ed_piece_t))) == NULL) {
        fprintf(stderr, "error: ed_piece_buffer_load: malloc\n");
        return -1;
    }

    if (data_size > 0) {

        ed_piece_t piece        = empty_ed_piece_t;
        piece.length            = data_size;
        piece.data              = (char*)data;

        if (_ed_piece_compute_lines(piece_buffer, &piece) < 0) {
            piece_buffer->pieces         = NULL;
            piece_buffer->piece_capacity = 0;
            free(piece_buffer->pieces);
            return -1;
        }

        piece_buffer->pieces[piece_buffer->piece_count++] = piece;

    }

    return 0;

}

int64_t ed_piece_buffer_length(ed_piece_buffer_t* piece_buffer) {

    if (piece_buffer == NULL) {
        fprintf(stderr, "error: ed_piece_buffer_length: invalid arguments\n");
        return -1;
    }

    int64_t result = 0;
    for (uint64_t i = 0; i < piece_buffer->piece_count; i++)
        result += piece_buffer->pieces[i].length;
    return result;

}

int64_t ed_piece_buffer_line_count(ed_piece_buffer_t* piece_buffer) {

    if (piece_buffer == NULL) {
        fprintf(stderr, "error: ed_piece_buffer_length: invalid arguments\n");
        return -1;
    }

    int64_t result = 0;
    for (uint64_t i = 0; i < piece_buffer->piece_count; i++)
        result += piece_buffer->pieces[i].lo_data_count;
    return result;

}

int8_t ed_piece_buffer_change(ed_piece_buffer_t* piece_buffer, ed_change_t* change) {

    uint64_t buffer_length = ed_piece_buffer_length(piece_buffer);

    if (piece_buffer == NULL || change->start > change->end || change->end > buffer_length) {
        fprintf(stderr, "error: ed_piece_buffer_replace: invalid arguments\n");
        return -1;
    }

    // @note nothing to do
    if (change->start == change->end && change->action_data_size == 0)
        return 0;

    ed_piece_t inserted = (ed_piece_t) {
        .data   = change->action_data,
        .length = change->action_data_size,
    };

    if (_ed_piece_compute_lines(piece_buffer, &inserted) < 0)
        return -1;

    uint64_t    start_piece_index,
                end_piece_index;
    
    if (_ed_piece_buffer_reserve(piece_buffer, piece_buffer->piece_count + 3)   < 0
     || _ed_piece_buffer_split(piece_buffer, change->start, &start_piece_index) < 0
     || _ed_piece_buffer_split(piece_buffer, change->end,   &end_piece_index)   < 0
    ) return -1;

    // @note clear data in the region we are changing
    for (uint64_t i = start_piece_index; i < end_piece_index; i++)
        _ed_piece_free(&piece_buffer->pieces[i]);
    
    if (change->action_data_size == 0) {
    
        printf("3\n");

        // @note since this is a deletion, we shift the pieces to the right back over to the left
        memmove(&piece_buffer->pieces[start_piece_index], &piece_buffer->pieces[end_piece_index], sizeof(ed_piece_t) * (piece_buffer->piece_count - end_piece_index));
        piece_buffer->piece_count -= (end_piece_index - start_piece_index);
        return 0;

    }

    // @note since this is an insertion, we shift the pieces to the right back over to the left, leaving room for our insertion
    memmove(&piece_buffer->pieces[start_piece_index + 1], &piece_buffer->pieces[end_piece_index], sizeof(ed_piece_t) * (piece_buffer->piece_count - end_piece_index));
    piece_buffer->piece_count -= (end_piece_index - start_piece_index) - 1;
    piece_buffer->pieces[start_piece_index] = inserted;

    return 0;

}

int8_t ed_piece_buffer_apply(ed_piece_buffer_t* piece_buffer, uint64_t offset, char* out, uint64_t out_size) {

    uint64_t buffer_length = ed_piece_buffer_length(piece_buffer);

    if (piece_buffer == NULL || out == NULL || offset > buffer_length) {
        fprintf(stderr, "error: ed_piece_buffer_apply: invalid arguments");
        return -1;
    } 

    uint64_t    position    = 0,
                end         = offset + out_size;

    for (uint64_t i = 0; i < piece_buffer->piece_count && position < end; i++) {

        const ed_piece_t*   piece   = &piece_buffer->pieces[i];
        uint64_t            length  = piece->length;

        uint64_t first = offset > position 
            ? offset - position 
            : 0;

        uint64_t last = end - position < length 
            ? end - position 
            : length;

        if (first < last) {
            memcpy(out, piece->data + first, last - first);
            out += last - first;
        }

        position += length;

    }

    return 0;

}

int8_t ed_piece_buffer_unload(ed_piece_buffer_t* piece_buffer) {

    if (piece_buffer == NULL) {
        fprintf(stderr, "error: ed_piece_buffer_unload: invalid arguments\n");
        return -1;
    }

    for (uint64_t i = 0; i < piece_buffer->piece_count; i++)
        _ed_piece_free(&piece_buffer->pieces[i]);

    if (piece_buffer->pieces != NULL)
        free(piece_buffer->pieces);

    *piece_buffer = empty_ed_piece_buffer_t;
    return 0;

}