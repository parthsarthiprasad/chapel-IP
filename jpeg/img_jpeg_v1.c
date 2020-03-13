
/*****
      img_jpeg_v1.c -
      Get and put JPEG files from/to disk.  Also includes routines to support
      our in-memory image data structure.

      Public Interface:
        JPEG_isa - test if file is in JPEG format
        JPEG_read - read an image from disk
        JPEG_write - write an image to disk
        alloc_rgbimage - allocate an image in our format
        free_rgbimage - release an image
        read_rgb - retrieve the value of a pixel
        write_rgb - set the value of a pixel

      c 2015-2018 Primordial Machine Vision Systems, Inc.
*****/

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <errno.h>
#include <string.h>

#include "img_jpeg_v1.h"


/**** Macros ****/

/***
    RETONERR:  If the previous function call returned an error code (< 0),
               exit the function, returning the code.  Assumes the code has
               been assigned to a local variable 'retval'.
***/
#define RETONERR     { if (retval < 0) { return retval; }}

/***
    CLEANUPONERR:  If the previous function call returned an error code (< 0),
                   jump to the end of the function to clean up any locally
                   allocated storage.  Assumes the error code has been assigned
                   to a local variable 'retval' and that there is a label
                   'cleanup' to jump to.
***/
#define CLEANUPONERR   { if (retval < 0) { goto cleanup; }}



/**** JPEG Functions ****/

/***
    JPEG_isa:  Read the header of the file and see if it's in JPEG format.
    args:     fname - name of file to check
    returns:   true if fname in PNG format, 0 if not
               < 0 on failure (value depends on error)
***/
int JPEG_isa(const char *fname) {
  FILE *fin;                            /* file handle to read from */
  png_byte header[8];                   /* PNG file verification */
  struct jpeg_error_mgr jerr                        /* error message */
  int isjpeg;                            /* true if PNG file */
  char * buffer;                        // to read start index 
  uint8_t * soi;
  int retval;
  
  int marker;
  int markerSize;
  int four;
/*
  for jfif and exif support
  UInt16 soi = br.ReadUInt16();  // Start of Image (SOI) marker (FFD8)
                UInt16 marker = br.ReadUInt16(); // JFIF marker (FFE0) EXIF marker (FFE1)
                UInt16 markerSize = br.ReadUInt16(); // size of marker data (incl. marker)
                UInt32 four = br.ReadUInt32(); // JFIF 0x4649464a or Exif  0x66697845

                Boolean isJpeg = soi == 0xd8ff && (marker & 0xe0ff) == 0xe0ff;
                Boolean isExif = isJpeg && four == 0x66697845;
                Boolean isJfif = isJpeg && four == 0x4649464a;
  */

  /* For Windows, make this "rb". */
  fin = fopen(fname, "r");
  if (NULL == fin) {
    errmsg = strerror(errno);
    printf("can't open file %s to read: %s\n", fname, errmsg);
    return 0;
  }

  /* Verify is a jpeg. */
  fread(&soi,2,1,fin);
  //retval = fread(&header, 1, 8, fin);
  retval = (soi[0] == 0xff && soi[1] == 0xd8) ? 0 : -1;
  if (!retval) {
    printf("only read %d header bytes from %s\n", retval, fname);
    return 0;
  }

  isjpeg = !retval

  retval = fclose(fin);
  if (0 != retval) {
    errmsg = strerror(errno);
    printf("problem closing %s: %s\n", fname, errmsg);
    return -1;
  }

  return isjpeg;
}

/***
    PNG_read:  Bring in a PNG image.  See the libpng manpage for what we're
               doing.  Stored internally as an rgbimage.  Alpha channel is
               ignored.
    args:      fname - name of file with image, if NULL take from stdin
               img - image read (if non-NULL, will free old image)
    returns:   0 if successful
               < 0 on failure (value depends on error)
    modifies:  img
***/
int JPEG_read (const char * fname , rgbimage **img)
{
  FILE * fin;		/* source file */
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct my_error_mgr jerr;
  /* More stuff */
  
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */
	int w;
	int h;
	int numChannels;
	int x, y, xy;                         /* pixel coordinates/index */
  int i;
  int retval;

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */
  //  uint8_t * soi;
  //  bool isjpeg;

  //  fin = NULL;
  //  if (NULL = fname){
  //      fin = stdin;
  //  }else{
  //      /* For Windows, make this "rb". */
  //      fin = fopen(fname,"r");
  //      if(NULL==fin){
  //       errmsg = strerror(errno);
  //       printf("can't open file %s to read: %s\n", fname, errmsg);
  //       return -1;
  //      }
  //  }
  //   /* Verify is a jpeg. */
  // fread(&soi,2,1,fin);
  // //retval = fread(&header, 1, 8, fin);
  // retval = (soi[0] == 0xff && soi[1] == 0xd8) ? 0 : -1;
  // if (!retval) {
  //   printf("only read %d header bytes from %s\n", retval, fname);
  //   return 0;
  // }
  // isjpeg = !retval

	// if (!isjpeg) {
  //   printf("%s is not in PNG format\n", fname);
  //   retval = -1;
  //   goto cleanup;
  // }

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(&cinfo);
    fclose(fin);
    return 0;
  }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */

  jpeg_stdio_src(&cinfo, fin);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.txt for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  	//width of the output
	w= cinfo.output_width;
	h=cinfo.output_height;
	numChannels=cinfo.num_components;
	/* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;

	
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
    put_scanline_someplace(buffer[0], row_stride);
  }

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */

	retval = alloc_rgbimage(img, w, h);
	CLEANUPONERR;
	if (1 == nchan) {
	  /* Note this correctly reads 1-channel greyscale, where r == g == b. */
    for (y=0, xy=0; y<h; y++) {
      for (x=0; x<w; x++, xy++) {
        (*img)->r[xy] = rows[y][x];
        (*img)->g[xy] = rows[y][x];
        (*img)->b[xy] = rows[y][x];
      }
    }
  } else if ((3 == nchan) || (4 == nchan)) {
    for (y=0, xy=0; y<h; y++) {
      for (x=0, i=0; x<w; x++, xy++, i+=nchan) {
        (*img)->r[xy] = rows[y][i];
        (*img)->g[xy] = rows[y][i+1];
        (*img)->b[xy] = rows[y][i+2];
      }
    }
  } else {
    printf("PNG: do not support %d channels\n", nchan);
    retval = -1;
    goto cleanup;
  }

  retval = 0;
  
	cleanup:
	jpeg_destroy_decompress(&cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
	 if(NULL != fin){
		 retval = fclose(fin);
		 if (0 != retval) {
      errmsg = strerror(errno);
      printf("problem closing %s: %s\n", fname, errmsg);
      retval = -1;
    }
	 }
  


  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */
	
 
  /* And we're done! */
  return 1;
}

/***
    PNG_write:  Copy a image to disk.  See the libpng manpage for the flow
                here.
    args:       fname - name of file to write to, if NULL use stdout
                img - image to save
    returns:   0 if successful
               < 0 on failure (value depends on error)
***/
int PNG_write(const char *fname, rgbimage *img) {
  FILE *fout;                           /* file handle to write to */
  png_structp ptr;                      /* internal reference to PNG data */
  png_infop info;                       /* picture information */
  png_byte *row;                        /* copy of image row to write */
  char *errmsg;                         /* error message */
  int x, y, xy;                         /* pixel coordinates/index */
  int i;
  int retval;

  fout = NULL;
  row = NULL;

  if (NULL == fname) {
    fout = stdout;
  } else {
    /* For Windows, "wb". */
    fout = fopen(fname, "w");
    if (NULL == fout) {
      errmsg = strerror(errno);
      printf("can't open file %s to write: %s\n", fname, errmsg);
      return -1;
    }
  }

  if (NULL == (row = (png_byte *) calloc(3 * img->ncol, sizeof(png_byte)))) {
    printf("can't allocate local row storage\n");
    retval = -1;
    goto cleanup;
  }

  ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (NULL == ptr) {
    printf("could not prepare PNG for write\n");
    retval = -1;
    goto cleanup;
  }

  info = png_create_info_struct(ptr);
  if (NULL == info) {
    printf("could not prepare PNG info\n");
    retval = -1;
    goto cleanup;
  }
    
  if (setjmp(png_jmpbuf(ptr))) {
    retval = -1;
    goto cleanup;
  }

  /* Prepare to write. */
  png_init_io(ptr, fout);
  png_set_IHDR(ptr, info, img->ncol, img->nrow, 8, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, 
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(ptr, info);

  for (y=0, xy=0; y<img->nrow; y++) {
    for (x=0, i=0; x<img->ncol; x++, xy++, i+=3) {
      row[i] = img->r[xy];
      row[i+1] = img->g[xy];
      row[i+2] = img->b[xy];
    }
    png_write_row(ptr, row);
  }

  png_write_end(ptr, info);

  retval = 0;

 cleanup:
  if (row) {
    free(row);
  }

  if (NULL != fout) {
    retval = fclose(fout);
    if (0 != retval) {
      errmsg = strerror(errno);
      printf("problem closing %s: %s\n", fname, errmsg);
      retval = -1;
    }
  }

  if (NULL != ptr) {
    if (NULL != info) {
      png_destroy_write_struct(&ptr, &info);
    } else {
      png_destroy_write_struct(&ptr, NULL);
    }
  }


  return retval;
}



/**** rgbimage Support ****/

/***
    alloc_rgbimage:  Reserve memory for our internal storage of a color image.
                     Pixels arrays are zeroed.  The ncol, nrow, and npix
                     fields are filled in.
    args:            img - structure to set up (first freed if non-NULL)
                     ncol, nrow - size of image
    returns:   0 if successful
               < 0 on failure (value depends on error)
    modifies:  img
***/
int alloc_rgbimage(rgbimage **img, int ncol, int nrow) {

  /* The 'free if non-NULL' means we don't try to resize the image, just
     start from scratch. */
  free_rgbimage(img);

  if (NULL == (*img = (rgbimage *) calloc(1, sizeof(rgbimage)))) {
    printf("can't allocate rgb image");
    return -1;
  }

  (*img)->ncol = ncol;
  (*img)->nrow = nrow;
  (*img)->npix = ncol * nrow;

  if (NULL == ((*img)->r = (uchar *) calloc(ncol * nrow, sizeof(uchar)))) {
    printf("can't allocate red plane");
    free(*img);
    *img = NULL;
    return -1;
  }

  if (NULL == ((*img)->g = (uchar *) calloc(ncol * nrow, sizeof(uchar)))) {
    printf("can't allocate green plane");
    free((*img)->r);
    free(*img);
    *img = NULL;
    return - 1;
  }

  if (NULL == ((*img)->b = (uchar *) calloc(ncol * nrow, sizeof(uchar)))) {
    printf("can't allocate blue plane");
    free((*img)->g);
    free((*img)->r);
    free(*img);
    *img = NULL;
    return - 1;
  }

  return 0;
}

/***
    free_rgbimage:  Release the memory stored with an image.
    args:           img - data structure to free
    modifies:  img (set to NULL when done)
***/
void free_rgbimage(rgbimage **img) {

  if (*img) {
    if ((*img)->r) {
      free((*img)->r);
    }
    if ((*img)->g) {
      free((*img)->g);
    }
    if ((*img)->b) {
      free((*img)->b);
    }
    free(*img);
    *img = NULL;
  }
}

/***
    read_rgb:  Get a pixel from the image.
    args:      img - image
               x, y - coordinates of pixel to read
               r, g, b - color planes (all equal if greyscale image)
    returns:   0 if successful
               < 0 on failure (value depends on error)
    modifies:  r, g, b
***/
int read_rgb(rgbimage *img, int x, int y, uchar *r, uchar *g, uchar *b) {
  int xy;                               /* pixel index */
  
  if ((x < 0) || (y < 0) || (img->ncol <= x) || (img->nrow <= y)) {
    printf("pixel %d,%4d is OOB (image size %d x %4d)\n", x,y, 
           img->ncol,img->nrow);
    return -1;
  }

  xy = (y * img->ncol) + x;
  *r = img->r[xy];
  *g = img->g[xy];
  *b = img->b[xy];

  return 0;
}

/***
    write_rgb:  Change a pixel in the image.
    args:       img - image
                x, y - coordinates of pixel to write to
                r, g, b - color planes
    returns:   0 if successful
               < 0 on failure (value depends on error)
    modifies:  img
***/
int write_rgb(rgbimage *img, int x, int y, uchar r, uchar g, uchar b) {
  int xy;                               /* pixel index */
  
  if ((x < 0) || (y < 0) || (img->ncol <= x) || (img->nrow <= y)) {
    printf("pixel %d,%4d is OOB (image size %d x %4d)\n", x,y, 
           img->ncol,img->nrow);
    return -1;
  }

  xy = (y * img->ncol) + x;
  img->r[xy] = r;
  img->g[xy] = g;
  img->b[xy] = b;

  return 0;
}
