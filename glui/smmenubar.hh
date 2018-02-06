// Licensed GNU LGPL v3 or later: http://www.gnu.org/licenses/lgpl.html

#ifndef SPECTMORPH_MENUBAR_HH
#define SPECTMORPH_MENUBAR_HH

#include "smdrawutils.hh"
#include "smmath.hh"

namespace SpectMorph
{

struct MenuItem
{
  std::string text;
  std::function<void()> m_callback;
  double sx, ex, sy;

  void
  set_callback (const std::function<void()>& callback)
  {
    m_callback = callback;
  }
};

struct Menu
{
  std::vector<std::unique_ptr<MenuItem>> items;

  std::string title;
  double sx, ex;
  MenuItem *
  add_item (const std::string& text)
  {
    MenuItem *m;
    items.emplace_back (m = new MenuItem());
    m->text = text;
    return m;
  }
};

struct MenuBar : public Widget
{
  std::vector<std::unique_ptr<Menu>> menus;
  int selected_item = -1;
  int active_menu = -1;
  int selected_menu_item = -1;
  std::unique_ptr<ComboBoxMenu> current_menu;

  MenuBar (Widget *parent)
    : Widget (parent, 0, 0, 100, 100)
  {
  }
  Menu *
  add_menu (const std::string& title)
  {
    Menu *menu = new Menu();
    menu->title = title;
    menus.emplace_back (menu);
    return menu;
  }
  void
  draw (cairo_t *cr) override
  {
    DrawUtils du (cr);

    double space = 2;
    cairo_set_source_rgba (cr, 0.3, 0.3, 0.3, 1);
    du.round_box (0, space, width, height - 2 * space, 1, 5, true);
    cairo_set_source_rgba (cr, 1, 1, 1, 1);

    double tx = 16;
    for (int item = 0; item < menus.size(); item++)
      {
        auto& menu_p = menus[item];

        du.bold = true;

        double start = tx;
        double tw = du.text_width (menu_p->title);
        double end = tx + tw;
        double sx = start - 16;
        double ex = end + 16;

        if (item == selected_item)
          {
            cairo_set_source_rgba (cr, 1, 0.6, 0.0, 1);
            du.round_box (sx, space, ex - sx, height - 2 * space, 1, 5, true);

            cairo_set_source_rgba (cr, 0, 0, 0, 1); /* black text */
          }
        else
          {
            cairo_set_source_rgba (cr, 1, 1, 1, 1); /* white text */
          }

        du.text (menu_p->title, start, 0, width - 10, height);
        menu_p->sx = sx;
        tx = end + 32;
        menu_p->ex = ex;

        if (active_menu == item)
          {
            Menu *menu = menus[item].get();

            double max_text_width = 0;

            du.bold = false;
            for (size_t i = 0; i < menu->items.size(); i++)
              max_text_width = std::max (max_text_width, du.text_width (menu->items[i]->text));

            const double menu_height = menu->items.size() * 16 + 16;

            cairo_set_source_rgba (cr, 0.3, 0.3, 0.3, 1);
            du.round_box (sx, height + space, max_text_width + 32, menu_height, 1, 5, true);

            double starty = height + space + 8;
            for (size_t i = 0; i < menu->items.size(); i++)
              {
                if (selected_menu_item == i)
                  {
                    cairo_set_source_rgba (cr, 1, 0.6, 0.0, 1);
                    du.round_box (sx + 4, starty, max_text_width + 24, 16, 1, 5, true);

                    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
                  }
                else
                  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);

                du.text (menu->items[i]->text, sx + 16, starty, max_text_width, 16, TextAlign::LEFT);
                menu->items[i]->sx = sx;
                menu->items[i]->ex = sx + max_text_width + 32;
                menu->items[i]->sy = starty;
                starty += 16;
              }
          }
      }
  }
  bool
  clipping() override
  {
    return active_menu < 0;
  }
  void
  motion (double x, double y) override
  {
    selected_item = active_menu;
    selected_menu_item = -1;

    for (size_t i = 0; i < menus.size(); i++)
      {
        if (x >= menus[i]->sx && x < menus[i]->ex && y >= 0 && y < height)
          {
            selected_item = i;
            if (active_menu >= 0)
              active_menu = selected_item;
          }
      }

    if (active_menu >= 0)
      {
        Menu *menu = menus[active_menu].get();

        for (int i = 0; i < menu->items.size(); i++)
          {
            MenuItem *item = menu->items[i].get();

            if (x >= item->sx && x < item->ex && y >= item->sy && y < item->sy + 16)
              selected_menu_item = i;
          }
      }
  }
  void
  mouse_press (double mx, double my) override
  {
    if (selected_item < 0 || (active_menu >= 0 && selected_menu_item < 0))
      {
        window()->set_menu_widget (nullptr);
        selected_item = -1;
        active_menu = -1;
        return;
      }
    window()->set_menu_widget (this);
    active_menu = selected_item;
  }
  void
  mouse_release (double mx, double my) override
  {
    if (active_menu >= 0 && selected_menu_item >= 0)
      {
        MenuItem *item = menus[active_menu]->items[selected_menu_item].get();

        if (item->m_callback)
          item->m_callback();

        window()->set_menu_widget (nullptr);
        active_menu = -1;
        selected_item = -1;
        selected_menu_item = -1;
      }
  }
  void
  leave_event() override
  {
    selected_item = -1;
  }
};

}

#endif


