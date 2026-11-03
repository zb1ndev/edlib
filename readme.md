# edlib
edlib is a small text editing library built for fast loading, editing and rendering of large text files. The example provided is not the fastest and was mostly AI generated. This would be best matched with an io-uring setup and a parallelized write setup so the user wont notice lag. The provided example can open, load, edit, and save a 238 MiB file in about 300ms on my Ryzen 5 7640U. For comparison miniaudio, an almost 100,000 loc header only library is about 3.8 MiB. The library itself should be pretty cross-platform, it does use memchr which may perform differently on windows compared to the linux gcc implementation.

## Compilation
For linux, open `build-linux.sh` and change the `cc` variable value to the compiler you wish to use. Then run `build-linux.sh`. Once finished `libedlib.a` will appear in the `/build` directory.

## Usage
edlib does not load files itself, this does mean you will have to do file handling, but it also means that is can be used in cases where you dont need to interact with the file-system.

The following code loads `data` and `data_size` into the piece buffer. Note that edlib does not copy any file data you give it, nor does it copy change data. So make sure the data you are passing into edlib functions persists throughout its usage. 

```c
if (ed_piece_buffer_load(&piece_buffer, data, data_size) < 0) {}
```

The following code applies a change to the edlib piece buffer. This way of applying changes allows you to track them and easily create inverses that remove them for an undo/redo system.

```c
ed_change_t change = {
    .start            = 0,
    .end              = 0,
    .action_data_size = strlen("Hello World"),
    .action_data      = (char*)"Hello World"
};

if (ed_piece_buffer_change(&piece_buffer, &change) < 0) {}
```

The following code is pretty self explanatory, it just returns the full document length including changes.

```c
int64_t total_length = ed_piece_buffer_length(&piece_buffer);
```

The following code writes `buffer_size` bytes of the changed document from `offset` into `buffer`.

```c
if (ed_piece_buffer_apply(&piece_buffer, offset, buffer, buffer_size) < 0) {}
```

This call just cleans up the line offset data managed by edlib.

```c
ed_piece_buffer_unload(&piece_buffer);
```
