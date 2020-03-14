# chapel-IP

This directory is created to create an IP library for chapel using C and chapel-lang

### folder structure

   
    .
    ├── ...
    ├── support                    # file compatibility currentlt png,jpeg ()
    │   ├── jpeg                   # jpeg file support for rgb format    │
    │   │     ├── bin              # directory will contain executables
    │   │     ├── build            # object files that haven't been linked, and auto-generated dependencies
    │   │     ├── img_jpeg_vX.h    # Get and put JPEG files from/to disk.  
    │   │     │                      Also includes routines to support
    │   │     │                      our in-memory image data structure
    │   │     │                      X=1,2..... different versions of header file
    │   │     ├── img_jpeg_vX.c    # Subroutine calls to generate ,read and write jpeg file
    │   │     │                      and save data in rgb struct.
    │   │     │                      X=1,2..... different versions of c file.
    │   │     ├── Makefile         # compiler settings
    │   │     ├── testcases.chpl   # examples using img_jpeg_vX.c.
    │   │     └──testimage         # compiler settings    
    │   ├── png                    # jpeg file support for rgb format    │
    │   │     ├── bin              # directory will contain executables
    │   │     ├── build            # object files that haven't been linked, and auto-generated dependencies
    │   │     ├── img_png_vX.h     # Get and put png files from/to disk.  
    │   │     │                      Also includes routines to support
    │   │     │                      our in-memory image data structure
    │   │     │                      X=1,2..... different versions of header file
    │   │     ├── img_png_vX.c    # Subroutine calls to generate ,read and write png file
    │   │     │                      and save data in rgb struct.
    │   │     │                      X=1,2..... different versions of c file.
    │   │     ├── Makefile         # compiler settings
    │   │     ├── testcases.chpl   # examples using img_png_vX.c.
    │   │     └──testimage         # compiler settings    
    │   └── ...
    │   
    └── ...