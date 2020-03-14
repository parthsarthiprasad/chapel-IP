
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

      @parthsarthiprasad
*****/

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <errno.h>
#include <string.h>
#include <setjmp.h>

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

#define JPEG_write(...) JPEG_write_default((f_args){__VA_ARGS__});
/*
 * ERROR HANDLING:
 *
 * The JPEG library's standard error handler (jerror.c) is divided into
 * several "methods" which you can override individually.  This lets you
 * adjust the behavior without duplicating a lot of code, which you might
 * have to update with each future release.
 *
 * Our example here shows how to override the "error_exit" method so that
 * control is returned to the library's caller when a fatal error occurs,
 * rather than calling exit() as the standard error_exit method does.
 *
 * We use C's setjmp/longjmp facility to return control.  This means that the
 * routine which calls the JPEG library must first execute a setjmp() call to
 * establish the return point.  We want the replacement error_exit to do a
 * longjmp().  But we need to make the setjmp buffer accessible to the
 * error_exit routine.  To do this, we make a private extension of the
 * standard JPEG error handler object.  (If we were using C++, we'd say we
 * were making a subclass of the regular error handler.)
 *
 * Here's the extended error handler struct:
 */
//struct for wrapper funtion JPEG_read
typedef struct {
const char * fname; 
rgbimage *img;
 int quality;
} f_args;

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}



/**** JPEG Functions ****/

int JPEG_write_wrapper(const char * fname, rgbimage *img, int quality );

/***
    JPEG_isa:  Read the header of the file and see if it's in JPEG format.
    args:     fname - name of file to check
    returns:   true if fname in PNG format, 0 if not
               < 0 on failure (value depends on error)
***/
int JPEG_isa(const char *fname) {
  FILE *fin;                            /* file handle to read from */
  struct jpeg_error_mgr jerr  ;                   /* error message */
  int isjpeg;                            /* true if PNG file */
  char * buffer;                        // to read start index 
  int * soi;
  char* errmsg;
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

  isjpeg = !retval;

  retval = fclose(fin);
  if (0 != retval) {
    errmsg = strerror(errno);
    printf("problem closing %s: %s\n", fname, errmsg);
    return -1;
  }

  return isjpeg;
}

/***
    JPEG_read:  Bring in a PNG image.  See the libpng manpage for what we're
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
	int x, y, xy=0;                         /* pixel coordinates/index */
  int i;
  int retval;
  char * errmsg;
  

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
  //outputcomponents contain numchannels*row
  retval = alloc_rgbimage(img, w, h);
  CLEANUPONERR;
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


	if (1 == numChannels) {
	  /* Note this correctly reads 1-channel greyscale, where r == g == b. */
      
      for (x=0; x<w; x++, xy++) {
        (*img)->r[xy] = buffer[0][x];
        (*img)->g[xy] = buffer[0][x];
        (*img)->b[xy] = buffer[0][x];
      }
    
  } else if ((3 == numChannels) || (4 == numChannels)) {
    
      for (x=0, i=0; x<w; x++, xy++, i+=numChannels) {
        (*img)->r[xy] = buffer[0][i];
        (*img)->g[xy] = buffer[0][i+1];
        (*img)->b[xy] = buffer[0][i+2];
      }
  } else {
    printf("JPEG: do not support %d channels\n", numChannels);
    retval = -1;
    goto cleanup;
  }

    /* Assume put_scanline_someplace wants a pointer and sample count. */
    //put_scanline_someplace(buffer[0], row_stride);
  }

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */



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
// wrapper function to provide default value 
int JPEG_write_default( f_args in){
    const char * fname = in.fname;
    rgbimage *img = in.img;
    int quality = in.quality?in.quality:75;
    return JPEG_write_wrapper(fname,img,quality);
}

/***
    PNG_write:  Copy a image to disk.  See the libpng manpage for the flow
                here.
    args:       fname - name of file to write to, if NULL use stdout
                img - image to save
    returns:   0 if successful
               < 0 on failure (value depends on error)
***/

int JPEG_write_wrapper(const char * fname, rgbimage *img, int quality )
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE * fout;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */
	int retval=-1;
  char * errmsg;
  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
  if ((fout = fopen(fname, "wb")) == NULL) {
    errmsg = strerror(errno);
   
    printf("can't open %s: %s\n", fname, errmsg);
    return -1;
		goto cleanup;
  }
  jpeg_stdio_dest(&cinfo, fout);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = img->ncol; 	/* image width and height, in pixels */
  cinfo.image_height = img->nrow;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = cinfo.image_width * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    //row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
    row_pointer[0] = img->r+cinfo.next_scanline*row_stride;
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  
  /* Step 6: Finish compression */

  cleanup:

	jpeg_finish_compress(&cinfo);
	retval=0;
  /* After finish_compress, we can close the output file. */
  if (NULL != fout) {
    retval = fclose(fout);
    if (0 != retval) {
      errmsg = strerror(errno);
      printf("problem closing %s: %s\n", fname, errmsg);
      retval = -1;
    }
  }
	

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  /* And we're done! */
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
