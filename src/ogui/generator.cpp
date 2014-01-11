#include "stdafx.h"

namespace OGUI {

void parse_xml_value(const xstring& xml_value, Orientation& value)
{
  xstring v = xml_value;
  v.to_upper();
  if (v == "VERTICAL") value = ORIENTATION_VERTICAL;
  else
  if (v == "HORIZONTAL") value = ORIENTATION_HORIZONTAL;
  else
    throw xstring("Invalid orientation value: " + xml_value);
}

void parse_xml_value(const xstring& xml_value, Alignment& value)
{
  int res = 0;
  xstring_tokenizer st(xml_value, " ,;|");
  while (st.has_more_tokens())
  {
    xstring t = st.get_next_token().to_upper();
    if (t == "LEFT")    res |= ALIGN_LEFT;
    if (t == "RIGHT")   res |= ALIGN_RIGHT;
    if (t == "TOP")     res |= ALIGN_TOP;
    if (t == "BOTTOM")  res |= ALIGN_BOTTOM;
    if (t == "HCENTER") res |= ALIGN_HCENTER;
    if (t == "VCENTER") res |= ALIGN_VCENTER;
    if (t == "CENTER")  res |= ALIGN_CENTER;
  }
  value = Alignment(res);
}

void parse_xml_value(const xstring& xml_value, xstring& value)
{
  value = xml_value;
}

void parse_xml_value(const xstring& xml_value, int& value)
{
  if (xml_value == "TIGHT") value = LAYOUT_TIGHT;
  else
  if (xml_value == "EXPAND") value = LAYOUT_EXPAND;
  else
    value = xml_value.as_int();
}

void parse_xml_value(const xstring& xml_value, unsigned& value)
{
  if (xml_value.begins_with("0x"))
    value = as_hex(xml_value);
  else
    value = xml_value.as_int();
}

void parse_xml_value(const xstring& xml_value, double& value)
{
  value = xml_value.as_double();
}

void parse_xml_value(const xstring& xml_value, float& value)
{
  value = float(xml_value.as_double());
}

void parse_xml_value(const xstring& xml_value, Point& value)
{
  xstring_tokenizer st(xml_value, ",");
  parse_xml_value(st.get_next_token(), value.x);
  parse_xml_value(st.get_next_token(), value.y);
}

void parse_xml_value(const xstring& xml_value, Rect& value)
{
  xstring_tokenizer st(xml_value, ",");
  value.left = st.get_next_token().as_int();
  value.top = st.get_next_token().as_int();
  value.right = st.get_next_token().as_int();
  value.bottom = st.get_next_token().as_int();
}

void parse_xml_value(const xstring& xml_value, bool& value)
{
  value = (xml_value == "true");
}

Widget* get_root_widget(widget_ptr ptr)
{
  Widget* w = ptr->get_parent();
  if (!w) return ptr.get();
  while (w)
  {
    Widget* next = w->get_parent();
    if (!next) break;
    w = next;
  }
  return w;
}

class ForwardReferences
{
public:
  static ForwardReferences* instance()
  {
    static std::unique_ptr<ForwardReferences> ptr(new ForwardReferences);
    return ptr.get();
  }

  void add_forward_reference(std::function<void()> func)
  {
    m_Functions.push_back(func);
  }

  void execute()
  {
    for (auto& f : m_Functions)
      f();
    m_Functions.clear();
  }

private:
  friend struct std::default_delete<ForwardReferences>;
  ForwardReferences() {}
  ~ForwardReferences() {}
  ForwardReferences(const ForwardReferences&) {}
  ForwardReferences& operator= (const ForwardReferences&) { return *this; }
  typedef std::function<void()> forward_func;
  typedef std::vector<forward_func> func_vec;
  func_vec m_Functions;
};


template<class WIDGET>
class TWidgetGenerator
{
public:
  typedef std::shared_ptr<WIDGET> pointer;
  static pointer create() { return pointer(WIDGET::create()); }

  static void set_value(pointer widget, void (WIDGET::*set_func)(widget_ptr ptr), const xstring& xml_value)
  {
    ForwardReferences::instance()->add_forward_reference([widget, set_func, xml_value]() {
      widget_ptr res = get_root_widget(widget)->search(xml_value);
      if (res)
      {
        WIDGET* w = widget.get();
        (w->*set_func)(res);
      }
      else throw xstring("Widget '" + xml_value + "' not found.");
    });
  }

  template<class T>
  static void set_value(pointer widget, void (WIDGET::*set_func)(const T&), const xstring& xml_value)
  {
    T value;
    parse_xml_value(xml_value, value);
    WIDGET* w = widget.get();
    (w->*set_func)(value);
  }
};

#include <ogui/mapping.inl>

widget_ptr load_widget_xml(xml_element_ptr el)
{
  widget_ptr w = widget_parse(el);
  if (!w) return w;
  if (el->has_attribute("layout"))
  {
    xstring layout = el->get_attribute("layout");
    if (layout == "VerticalLayout") w->set_layout(layout_ptr(new VerticalLayout));
    else
    if (layout == "HorizontalLayout") w->set_layout(layout_ptr(new HorizontalLayout));
    else
    if (layout.begins_with("GridLayout"))
    {
      xstring_tokenizer st(layout, " =.:;,");
      st.get_next_token(); // GridLayout
      int cols = st.get_next_token().as_int();
      if (cols == 0) cols = 1;
      w->set_layout(layout_ptr(new GridLayout(cols)));
    }
    else
      w->set_layout(layout_ptr());
  }
  return w;
}

widget_ptr load_sublayout_xml(xml_element_ptr cur_root)
{
  bool colors_pushed = false;
  skin_colors override_colors(SKIN_COLOR_LAST, 0);
  for (auto it = cur_root->attr_begin(); it != cur_root->attr_end(); ++it)
  {
    xstring name = it->first;
    if (name.begins_with("SKIN_COLOR_"))
    {
      if (!colors_pushed) OGUIManager::instance()->push_color_state();
      colors_pushed = true;
      SkinColors sc = get_skin_color_id(name);
      unsigned value;
      parse_xml_value(it->second, value);
      OGUIManager::instance()->set_skin_color(sc, value);
      override_colors[sc] = value;
    }
  }
  widget_ptr wroot = load_widget_xml(cur_root);
  if (wroot)
  {
    if (colors_pushed) wroot->set_colors_overrides(override_colors);
    for (auto el : *cur_root)
    {
      widget_ptr child = load_sublayout_xml(el);
      if (child) wroot->add_child(child);
    }
  }
  if (colors_pushed) OGUIManager::instance()->pop_color_state();
  return wroot;
}

widget_ptr load_layout_xml(xml_element_ptr root)
{
  widget_ptr res = load_sublayout_xml(root);
  ForwardReferences::instance()->execute();
  return res;
}

} // namespace OGUI