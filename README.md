# Fileless Saving

A single header c89 library for saving data without using any external files.
It achieves this by overwritting global variables in the executable file while it is running.
If you are curious on how this works and don't want to go through the code, I plan on writting a more detailed document explaining how to get around
the roadblocks put in place to prevent exactly this.

## Disclamer
This is not a useful project. You should never use this. This was simply made to see if it could be done.

## Limitations
At the moment (and probably forever), you can not save pointers (this includes strings). You also can't save data with a different size as the original data.
You can kind of get around this by allocating more room than you need, and simply using the larger buffer.

This project only works on linux using ELF32 and ELF64 at the moment, though I'd like to add Windows PE files as well. (If that is even possible lol)
