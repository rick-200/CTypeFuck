# CTypeFuck
The type definition of nested function pointer in C/C++ is anti-human, so I create CTypeFuck.

Randomly generate an extremely complicated C/C ++ type definition.
```
usage: ctypefuck <depth> [flags]
depth: recursive depth
flages:
-no-cv : do not generate const and volidate modifier
-only-int : restrict type except function pointer to int
-param <max_count> : set maxium parameter amount for generated function pointer
-out <file_path> : set output stream to a specific file
```