
/*****
      img_jpeg_v1.h -
      Public declarations for the JPEG file support.  Also includes support
      for our in-memory image data structure.

      Public Interface:
        JPEG_isa - test if file is in JPEG format
        JPEG_read - read an image from disk
        JPEG_write - write an image to disk
        alloc_rgbimage - allocate our internal image storage
        free_rgbimage - release image memory
        read_rgb - get a pixel in the image
        write_rgb - set a pixel

      Required Libraries:
        libjpeg

      c @parthsarthiprasad
*****/

#ifndef _IMGJPEG
#define _IMGJPEG 1


/*** Data Types ***/

typedef unsigned char uchar;


/*** Data Structures ***/

/* a color image with RGB planes stored separately */
typedef struct __rgbimage {
  int ncol;                             /* width (number columns) of image */
  int nrow;                             /* height (number rows) of image */
  int npix;                             /* number pixels = w * h */
  uchar *r;                             /* red plane */
  uchar *g;                             /* green plane */
  uchar *b;                             /* blue plane */
} _rgbimage, *rgbimage;

extern "C" {
#include "jpeglib.h"
}
/*** External Functions ***/

/* test if a file is in JPEG format, returning true if so, 0 if not
     fname - name of file to read
*/
extern int JPEG_isa(const char *);

/* read a JPEG image, converting it to an rgbimage
     fname - name of file to read (if NULL, use stdin)
     img - pointer to image to create and read (frees old if non-NULL)
   returns < 0 on error
   modifies img
*/
extern int JPEG_read(const char *, _rgbimage **);

/* write an rgbimage to disk in JPEG format
     fname - name of file to write to (if NULL, use stdout)
     img - image to write
   returns < 0 on error
*/
extern int JPEG_write(const char *, _rgbimage * , int);

/* allocate an image in our format, initializing contents to 0
     img - image to create (frees old if non-NULL)
     ncol, nrow - size of image
   returns < 0 on error
   modifies img
*/
extern int alloc_rgbimage(_rgbimage **, int, int);

/* release memory for an image
     img - image to free
   modifies img (set to NULL when done)
*/
extern void free_rgbimage(rgbimage **);

/* get pixel's RGB values
     img - image
     x, y - pixel coordinates
     r, g, b - color planes (all modified)
*/
extern int read_rgb(rgbimage *, int, int, uchar *, uchar *, uchar *);

/* change a pixel's RGB values
     img - image
     x, y - pixel coordinates
     r, g, b - color values
*/
extern int write_rgb(_rgbimage *, int, int, uchar, uchar, uchar);


#endif   /* _IMGJPEG */
