# intro

forked from Kanma/BLPConverter

add branch - windows which can compile on windows with visual studio

# compile

## linux/mac
for linux/mac, use master branch

## windows
for windows, use windows branch(verified under vs2022)

As the FreeImage static lib(link library dependencies ON) is more than 100MB, it has been compressed into a zip file. Please decompress `dependencies/lib/FreeImage/x64/static.zip` to the directory where the lib/h file resides in the folder `dependencies/lib/FreeImage/x64/static`

# about

What did I do to make this project compile successfully under windows?

First of all, the author has already mentioned that he only guarantees builds pass under mac and linux.
When I opened the project with vs2022, some build errors occurred.
Based on the error message, it is speculated that the problem may come from the freeimage dependency library.
Since I am not a professional C/C++ developer,
it is difficult for me to fix the compatibility issues with the freeimage dependencies in this project under windows.
In the end, I decided to use the latest freeimage library precompiled file (.dll/.lib)

First, I downloaded the x64 dll/lib file provided by freeimage and tried to link it in this project.
I succeeded, but the executable required an external dynamic link library file (dll) to run

To get a single executable, I used vs2022 for a static library build of the freeimage source code.
I ran into some tough issues while cmake building this project (after all, I'm really not a professional C/C++ developer),
but it took me a day to fix everything. Finally, I got what I wanted: an executable file available in windows