#include <ogui/rgba_image.h>
#include "png.h"
#include <xstring.h>
#include <ogui/constants.h>
#include <ogui/manager.h>

namespace OGUI {

typedef unsigned char byte;

void read_from_input_stream(png_structp png_ptr, unsigned char* buffer, png_size_t len)
{
  void* ioptr=png_get_io_ptr(png_ptr);
  if (ioptr)
  {
    std::istream* is=reinterpret_cast<std::istream*>(ioptr);
    is->read((char*)buffer,len);
  }
}

struct struct_cleaner
{
  png_structp m_read_png_ptr,m_write_png_ptr;
  png_infop   m_info_ptr;
  FILE*       m_fp;
  struct_cleaner(png_structp read_png_ptr, png_structp write_png_ptr) 
  : m_read_png_ptr(read_png_ptr)
  , m_write_png_ptr(write_png_ptr)
  , m_info_ptr(0)
  , m_fp(0) 
  {}
  ~struct_cleaner()
  {
    if (m_read_png_ptr) png_destroy_read_struct(&m_read_png_ptr,&m_info_ptr,0);
    if (m_write_png_ptr) png_destroy_write_struct(&m_write_png_ptr,&m_info_ptr);
    if (m_fp) fclose(m_fp);
  }
};

bool Image::load_from_file(const char* path)
{
  return load_png(std::ifstream(path,std::ios::in|std::ios::binary));
}

bool Image::save_to_file(const char* path)
{
  {
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return false;
    struct_cleaner cleaner(0,png_ptr);
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;
    cleaner.m_fp=fp;
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) return false;
    cleaner.m_info_ptr=info_ptr;
    if (setjmp(png_jmpbuf(png_ptr))) return false;
    png_init_io(png_ptr, fp);
    if (setjmp(png_jmpbuf(png_ptr))) return false;
    png_set_IHDR(png_ptr, info_ptr, m_Width, m_Height,
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) return false;

    std::vector<png_bytep> rows(m_Height);
    for(unsigned y=0;y<m_Height;++y) rows[y]=get_row(y);
    png_write_image(png_ptr, &rows[0]);
    if (setjmp(png_jmpbuf(png_ptr))) return false;
    png_write_end(png_ptr, NULL);
  }
  return true;
}

bool Image::load_png(std::istream& is)
{
  if (is.fail()) return false;
  byte header[8];
  is.read((char*)header,8);
  if (!png_check_sig(header,8)) return false;
  png_structp png_ptr = NULL;
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr) return false;
  struct_cleaner cleaner(png_ptr,0);
  png_infop info_ptr = NULL;
  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr) return false;
  cleaner.m_info_ptr=info_ptr;
  png_set_read_fn(png_ptr, &is, read_from_input_stream);
  png_set_sig_bytes(png_ptr,8);
  png_read_info(png_ptr, info_ptr);

  png_uint_32 width=0,height=0,pitch=0;
  int bpp=0,color_type=-1;
  png_uint_32 rc = png_get_IHDR(png_ptr, info_ptr,&width,&height,&bpp,&color_type,0,0,0);
  pitch=png_get_rowbytes(png_ptr, info_ptr);
  if (color_type!=PNG_COLOR_TYPE_RGB && color_type!=PNG_COLOR_TYPE_RGBA) return false;
  buffer_type row;
  if (color_type==PNG_COLOR_TYPE_RGB) row.resize(pitch);
  if (rc==1)
  {
    initialize(width,height);
    for(unsigned y=0;y<height;++y)
    {
      byte* ptr=get_row(y);
      if (color_type==PNG_COLOR_TYPE_RGB)
      {
        png_read_row(png_ptr, (png_bytep)(&row[0]), 0);
        for(unsigned x=0;x<width;++x)
        {
          for(int i=0;i<3;++i) ptr[x*4+i]=row[x*3+i];
          ptr[x*4+3]=255;
        }
      }
      else
        png_read_row(png_ptr, (png_bytep)ptr, 0);
    }
  }
  return true;
}

Image Image::cut(Rect r)
{
  r.intersect(get_rect());
  if (!r.valid()) 
    throw (xstring("Invalid rect: {} in image size: {}") << r << get_rect().get_size());
  Image res(r.get_width(),r.get_height());
  res.paste(*this,r);
  return res;
}


bool Image::prepare_copy(const Image& src, Rect& src_rect, Rect& dst_rect, const Point& dst_pt)
{
  if (!src_rect.valid()) src_rect=src.get_rect();
  else src_rect.intersect(src.get_rect());
  dst_rect=src_rect;
  dst_rect.move_to(dst_pt);
  dst_rect.intersect(get_rect());
  if (dst_rect.get_size() != src_rect.get_size())
  {
    Point tl=(dst_rect.top_left() - dst_pt)+src_rect.top_left();
    src_rect=Rect(tl,tl+dst_rect.get_size());
  }
  return dst_rect.valid();
}

void Image::paste(const Image& src, Rect src_rect, const Point& dst_pt)
{
  Rect dst_rect;
  if (prepare_copy(src,src_rect,dst_rect,dst_pt))
  {
    int n=dst_rect.get_width()*4;
    int sy=src_rect.top;
    int sx=src_rect.left;
    for(int y=dst_rect.top;y<dst_rect.bottom;++y,++sy)
    {
      const byte* src_row=src.get_row(sy)+(sx*4);
      byte* dst_row=get_row(y)+(dst_rect.left*4);
      std::copy(src_row,src_row+n,dst_row);
    }
  }
}

inline void blend(const byte* src, byte* dst, const Transform& trans)
{
  for(int i=0;i<4;++i)
  {
    int s=int(src[3])*int(trans.transform(src[i],i)) + int(255-src[3])*int(dst[i]);
    s/=255;
    dst[i]=byte(s<0?0:(s>255?255:s));
  }
}

inline void blend(const byte* src, byte* dst, float src_alpha, float dst_alpha)
{
  for(int i=0;i<4;++i)
  {
    int s=int(src_alpha*src[i] + dst_alpha*dst[i]);
    dst[i]=byte(s<0?0:(s>255?255:s));
  }
}


void Image::blend_channel(const Image& src, Rect src_rect, const Point& dst_pt, const Transform& trans)
{
  Rect dst_rect;
  if (prepare_copy(src,src_rect,dst_rect,dst_pt))
  {
    int n=dst_rect.get_width();
    int sy=src_rect.top;
    int sx=src_rect.left;
    for(int y=dst_rect.top;y<dst_rect.bottom;++y,++sy)
    {
      const byte* src_row=src.get_row(sy)+(sx*4);
      byte* dst_row=get_row(y)+(dst_rect.left*4);
      for(int i=0;i<n;++i)
      {
        blend(src_row+4*i,dst_row+4*i,trans);
      }
    }
  }
}

void Image::blend_constant(const Image& src, Rect src_rect, const Point& dst_pt, float src_alpha, float dst_alpha)
{
  Rect dst_rect;
  if (prepare_copy(src,src_rect,dst_rect,dst_pt))
  {
    int n=dst_rect.get_width();
    int sy=src_rect.top;
    int sx=src_rect.left;
    for(int y=dst_rect.top;y<dst_rect.bottom;++y,++sy)
    {
      const byte* src_row=src.get_row(sy)+(sx*4);
      byte* dst_row=get_row(y)+(dst_rect.left*4);
      for(int i=0;i<n;++i)
      {
        blend(src_row+4*i,dst_row+4*i,src_alpha,dst_alpha);
      }
    }
  }
}

void Image::fill(Rect rect, unsigned color)
{
  rect.intersect(get_rect());
  for(int y=rect.top;y<rect.bottom;++y)
  {
    byte* row=get_row(y);
    unsigned* urow=reinterpret_cast<unsigned*>(row);
    std::fill_n(urow+rect.left,rect.get_width(),color);
    ++row;
  }
}

void filter(Image& image, const Rect& rect, const float* coef)
{
  Image res(rect.get_size());
  for(int y=rect.top;y<(rect.bottom-2);++y)
  {
    unsigned* dst=res.get_urow(y-rect.top+1);
    const unsigned* rows[3];
    for(int i=0;i<3;++i) rows[i]=image.get_urow(y+i);
    for(int x=rect.left;x<(rect.right-2);++x)
    {
      fColor color;
      for(int i=0;i<3;++i)
      {
        for(int j=0;j<3;++j)
        {
          color.add(rows[i][x+j],coef[i*3+j]);
        }
      }
      dst[x-rect.left+1]=color.compose();
    }
  }
  Rect src=res.get_rect();
  src.inflate(-1);
  image.paste(res,src,rect.top_left()+Point(1,1));
}

void draw_button(Image& target, const Rect& rect, bool sunk, unsigned bg_color, unsigned hilight, unsigned lolight)
{
  target.fill(bg_color);
  unsigned colors[] = { hilight, lolight };
  int c=(sunk?1:0);
  target.fill(Rect(0,0,rect.get_width(),1),colors[c]);
  target.fill(Rect(0,0,1,rect.get_height()-1),colors[c]);
  target.fill(Rect(0,rect.get_height()-1,rect.get_width(),rect.get_height()),colors[c^1]);
  target.fill(Rect(rect.get_width()-1,1,rect.get_width(),rect.get_height()),colors[c^1]);
}



} // namespace OGUI
