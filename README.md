# Findstuff
Searches for text or files in the current directory or all directories based on different flags in command line. Returns location of the file/files that contain the text or file.

## How to use:
Use -f flag to only search for certain file types (only works if searching for a file)
Use -s flag to search in all directories.

./findstuff "test" -s
Searches for all files that contain the text "test" in all directories.

./findstuff "for"
Searches for all files that contain the text "for" in current directory.

./findstuff program1 -f:c
Searches for all files named program1 that is a c file.

./findstuff program2
Searches for all files named program2 (can be any type of file)
