### Overview
A small program that extract the raw pixel data from a .png file. For serious work use stb_image!

### Usage
```
$ ./xpngToBin path-to-input/file.png
    -> this creates path-to-input/texture_file.bin
```
or you might want to specify output folder/file,
```
$ ./xpngToBin path-to-input/file.png -o path-to-output/
    -> this creates path-to-output/texture_file.bin
$ ./xpngToBin path-to-input/file.png -o path-to-output/out.bin
    -> this creates path-to-output/out.bin
```
