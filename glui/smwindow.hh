// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#ifndef SPECTMORPH_WINDOW_HH
#define SPECTMORPH_WINDOW_HH

#include "smwidget.hh"
#include "pugl/pugl.h"
#include <memory>

namespace SpectMorph
{

struct CairoGL;

struct Window : public Widget
{
  PuglView                 *view;
  cairo_t                  *cr;
  std::unique_ptr<CairoGL>  cairo_gl;

  Window (int width, int height, PuglNativeWindow parent = 0);
  virtual ~Window();

  std::vector<Widget *> crawl_widgets();
  void on_display();
};

}

#endif