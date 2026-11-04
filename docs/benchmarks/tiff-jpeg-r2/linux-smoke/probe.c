#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
int main(int argc,char** argv) {
  if(argc!=3) return 2;
  TIFF* t=TIFFOpen(argv[1],"r"); if(!t) return 3;
  uint32_t w,h,bw,bh; uint16_t c,bits;
  TIFFGetField(t,TIFFTAG_IMAGEWIDTH,&w); TIFFGetField(t,TIFFTAG_IMAGELENGTH,&h);
  TIFFGetField(t,TIFFTAG_SAMPLESPERPIXEL,&c); TIFFGetField(t,TIFFTAG_BITSPERSAMPLE,&bits);
  if(w!=39 || h!=35 || (c!=1 && c!=3) || bits!=8) return 4;
  int tiled=TIFFIsTiled(t);
  if(tiled) {TIFFGetField(t,TIFFTAG_TILEWIDTH,&bw);TIFFGetField(t,TIFFTAG_TILELENGTH,&bh);}
  else {bw=w;TIFFGetField(t,TIFFTAG_ROWSPERSTRIP,&bh);}
  unsigned char* result=calloc((size_t)w*h,c);
  unsigned char* block=malloc(tiled?TIFFTileSize(t):TIFFStripSize(t));
  if(!result || !block) return 5;
  for(uint32_t y=0;y<h;y+=bh) for(uint32_t x=0;x<w;x+=bw) {
    tmsize_t n=tiled?TIFFReadEncodedTile(t,TIFFComputeTile(t,x,y,0,0),block,-1):
      TIFFReadEncodedStrip(t,TIFFComputeStrip(t,y,0),block,-1);
    if(n<0) return 6;
    for(uint32_t row=0;row<bh && y+row<h;++row) {
      uint32_t count=w-x<bw?w-x:bw;
      memcpy(result+((size_t)(y+row)*w+x)*c,block+(size_t)row*bw*c,(size_t)count*c);
    }
  }
  FILE* f=fopen(argv[2],"wb"); if(!f) return 7;
  size_t length=(size_t)w*h*c;
  int ok=fwrite(result,1,length,f)==length && fclose(f)==0;
  free(result);free(block);TIFFClose(t);return ok?0:8;
}
