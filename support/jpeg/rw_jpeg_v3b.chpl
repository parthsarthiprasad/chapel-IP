
/*****
        rw_jpeg_v3b.chpl -
        Program that reads a JPEG file from disk, prints the RGB values found
        at one pixel, changes it to 1, 2, 3, and writes the change to disk.
        This version validates the config constants and uses a variadic 
        procedure to handle cleaning up the C-side allocations on error.

        Call:
          rw_jpeg_v3b
            --inname=<file>    file to read from
            --outname=<file>   file to write to
            --x=<#>            x coordinate of pixel to print
            --y=<#>            y coordinate of pixel to print

        c @parthsarthiprasad
*****/

use Help;

/* Command line arguments. */
config const inname : string;           /* name of file to read */
config const outname : string;          /* file to create with modded pixel */
config const x : c_int = -1;            /* pixel to change */
config const y : c_int = -1;            /* pixel to change */

/* The C image data structure. */
extern class rgbimage {
  var ncol : c_int;                     /* width (columns) of image */
  var nrow : c_int;                     /* height (rows) of image */
  var npix : c_int;                     /* number pixels = w * h */
  var r : c_ptr(c_uchar);               /* red plane */
  var g : c_ptr(c_uchar);               /* green plane */
  var b : c_ptr(c_uchar);               /* blue plane */
}

/* Our variables */
var rgb : rgbimage;                     /* the image we read */
var xy : int(32);                       /* 1D index of x, y coord */
var retval : c_int;                     /* return value with error code */

/* External img_jpeg linkage. */
extern proc JPEG_read(fname : c_string, ref img : rgbimage) : c_int;
extern proc JPEG_write(fname : c_string, img : rgbimage) : c_int;
extern proc free_rgbimage(ref img : rgbimage) : void;
extern proc JPEG_isa(fname : c_string) : c_int;
/* The rest of the interface we don't use now. */
/*
extern proc alloc_rgbimage(ref img : rgbimage, 
                           ncol : c_int, nrow : c_int) : c_int;
extern proc read_rgb(img : rgbimage, x, y : c_int, 
                     ref r, ref g, ref b : c_uchar) : c_int;
extern proc write_rgb(img : rgbimage, x, y : c_int, r, g, b : c_uchar) : c_int;
*/


/***
    usage - Print an error message along with the system help, then exit.
    args:   msg - message to print
***/
proc usage(msg : string) {

  writeln("\nERROR");
  writeln("  ", msg);
  printUsage();
  halt();
}

/***
    end_onerr:  Check the error code; if OK (>= 0) do nothing.  Else release
                any objects passed as additional arguments - anything can
                be passed and its type will determine the action that needs
                to be done - and exit with an non-zero error value.
    args:       retval - error code/return to value for exit
                inst - variable list of instances to free
***/
proc end_onerr(retval : int, inst ...?narg) : void {

  if (0 <= retval) then return;

  /* Note we skip the argument if we don't know how to clean it up.  The
     writefs are just to show which paths we execute. */
  for param i in 1..narg {
    if (inst(i).type == rgbimage) then {
      writef("freeing rgb instance\n");
      free_rgbimage(inst(i)); 
    } else if isClass(inst(i)) then {
      writef("freeing class instance\n");
      delete inst(i);
    }
  }
  exit(1);
}



/**** Test Class ****/

/* This is a simple class to show end_onerr deletes it correctly. */

class dummyclass {
  var tmp1 : int(32);
  var tmp2 : int(64);

  proc deinit() {
    writef("removing instance of dummyclass %i, %i\n", tmp1, tmp2);
  }
}

var dummy = new dummyclass(5,6);



/**** Top Level ****/

/* First sanity check the arguments, then read the image, get the pixel 
   requested, change it, and write it back out.  Finally we need to free 
   the allocation made in JPEG_read. */

if (x < 0) then
  usage("missing --x or value < 0");
if (y < 0) then
  usage("missing --y or value < 0");
if ("" == inname) then
  usage("missing --inname");
if (!JPEG_isa(inname.c_str())) then
  usage("input file not a JPEG picture");
if ("" == outname) then
  usage("missing --outname");

retval = JPEG_read(inname.c_str(), rgb);
end_onerr(retval, rgb, dummy);

if (rgb.ncol <= x) {
  free_rgbimage(rgb);
  usage("--x (0-based) >= image width ");
}
if (rgb.nrow <= y) {
  free_rgbimage(rgb);
  usage("--y (0-based) >= image height ");
}

/* Now we can access the fields directly. */
xy = (y * rgb.ncol) + x;
writef("\nRead %4i x %4i JPEG image\n", rgb.ncol, rgb.nrow);
writef("At %4i,%4i      R %3u  G %3u  B %3u\n\n", x,y, 
       rgb.r(xy), rgb.g(xy), rgb.b(xy));

rgb.r(xy) = 1;
rgb.g(xy) = 2;
rgb.b(xy) = 3;

retval = JPEG_write(outname.c_str(), rgb);
end_onerr(retval, rgb, dummy);

free_rgbimage(rgb);

