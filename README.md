This is a modern refactoring of the simple "*storage allocator*" found in the classic K&R TCPL book.

It retains the simplicity and efficiency of the original, adding a few improvements :
- Localised, encapsulated state :
  
  The K&R allocator uses a global freelist, so there is **one** *global* allocator.
  Instead, we load/store the freelist and other state in `struct allocator`s, so there may be any number
  of independent, scope-bound allocator *objects*.
- Custom memory pools :

  Where the original obtains memory from the OS through a syscall,
  here the *user* adds memory region(s) to an allocator object via `allocator_add()`.
- Safe, correct & portable :
  
  This allocator is thread-safe using lightweight syncronisation techniques - spinlocks composed of C11 atomics.
  
  It also checks for integer overflow and treats this as an allocation error and handles alignment
  in a machine-independant way.
  In fact, the library is *virtually freestanding*.
- Queryable :
  
  Like most modern allocators, this one too can query the *real size* of an allocation, with `allocator_allocsz()`.
  It also allows for querying each block's size in the freelist using `allocator_for_blocks()`. The K&R allocator is, in contrast, entirely opaque.
