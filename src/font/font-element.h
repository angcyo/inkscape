// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Basic font-family, font-style info.
 *
 * This class stores information about fonts in a non-GUI dependent way
 * (but in a way convient for use in the GUI).
 *
 * Copyright (C) 2022-2025 Michael Kowalski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef FONT_ELEMENT_H
#define FONT_ELEMENT_H

#include <glibmm/i18n.h>
#include <glibmm/object.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>

#include "font/font-info.h"
#include "font/font-utils.h"

namespace Inkscape {

Glib::ustring get_font_icon(FontInfo const &font, bool missing_font = false)
{
    if (missing_font) {
        return "missing-element-symbolic";
    } else if (font.variable_font) {
        return ""; // add icon for variable fonts?
    } else if (font.synthetic) {
        return "generic-font-symbolic";
    }
    return {};
}

Glib::ustring get_alt_name(Glib::ustring const &fontspec)
{
    static Glib::ustring sans = "sans-serif";
    if (fontspec.find(sans) != Glib::ustring::npos) {
        auto end = fontspec[sans.size()];
        if (end == 0 || end == ' ' || end == ',') {
            return _("Sans Serif") + fontspec.substr(sans.size());
        }
    }
    return fontspec; // use font spec verbatim
}

// Gio models require Glib object-based elements,
// we use this class to keep track of all fonts.
class FontElement : public Glib::Object
{
    enum class Type
    {
        Font,   // all fonts when not sorting by family
        Family, // when sorting by family, this is a font that represents a family (a node in a tree)
        Style   // when sorting by family, this is one of the "style" fonts in a family (a leaf in a tree)
    } _type;

public:
    static Glib::RefPtr<FontElement> create_font(FontInfo const &font)
    {
        return Glib::make_refptr_for_instance<FontElement>(new FontElement({}, font, {}, Type::Font));
    }

    static Glib::RefPtr<FontElement> create_style(FontInfo const &font)
    {
        return Glib::make_refptr_for_instance<FontElement>(new FontElement({}, font, {}, Type::Style));
    }

    static Glib::RefPtr<FontElement> create_family(FontInfo const &font, std::vector<FontInfo> family)
    {
        return Glib::make_refptr_for_instance<FontElement>(new FontElement(std::move(family), font, {}, Type::Family));
    }

    static Glib::RefPtr<FontElement> create_injected_font(FontInfo const &font, Glib::ustring alt_spec, bool is_missing)
    {
        auto element =
            Glib::make_refptr_for_instance<FontElement>(new FontElement({}, font, std::move(alt_spec), Type::Font));
        element->_missing_font = is_missing;
        element->_injected = true;
        return element;
    }

    static Glib::RefPtr<FontElement> create_placeholder()
    {
        auto element = Glib::make_refptr_for_instance<FontElement>(new FontElement({}, {}, {}, Type::Font));
        element->_placeholder = true;
        return element;
    }

    Glib::ustring icon_name() const { return get_font_icon(_font, _missing_font); } // Defined in ui/widget/font-list.cpp
    Glib::ustring icon_tooltip() const
    {
        if (_missing_font) {
            // this font is not installed / not available
            return _("This font is missing");
        } else if (_font.synthetic) {
            // this is an alias for some fallback font (ex: 'Serif', 'Sans') and/or faux style
            return _("This is an alias or synthetic font");
        }
        return {};
    }
    FontInfo const &font() const { return _font; }
    Glib::ustring const &get_alt_spec() const { return _alt_fontspec; }
    bool is_present() const
    {
        // true if this font is installed
        return _font.ff != nullptr;
    }
    bool is_family() const { return _type == Type::Family; }
    bool is_injected() const { return _injected; }
    void clear_injected()
    {
        _injected = false;
        _placeholder = true;
    }
    bool is_placeholder() const { return _placeholder; }
    std::vector<FontInfo> const &family() const { return _family; }
    // get markup for a full font name
    Glib::ustring get_full_name_markup() const
    {
        auto name = get_font_name(Type::Font);
        return Glib::ustring::compose("<small>%1</small>", name);
    }
    // get markup for a font name
    Glib::ustring get_name_markup() const
    {
        auto name = get_font_name(_type);
        return Glib::ustring::compose("<small>%1</small>", name);
    }
    Glib::ustring get_name_tooltip() const { return get_font_name(_type); }
    // get markup for a font badge - number of styles in a family
    Glib::ustring get_badge_markup() const
    {
        if (has_badge_markup_content()) {
            // count
            return Glib::ustring::compose("<small>  %1  </small>", _family.size());
        }
        // Return a similarly shaped string but without the number, to keep general label sizes the
        // same (GtkListViews need all items to be same height, and if this label is entirely
        // hidden, it has an effect on the name/badge row height)
        return "<small> </small>";
    }
    bool has_badge_markup_content() const { return _family.size() > 1; }
    // get markup to render font preview
    Glib::ustring get_sample_markup(int font_size_percent, Glib::ustring sample_text)
    {
        // if no sample text given, then render font name
        auto text = Glib::Markup::escape_text(
            sample_text.empty() ? get_font_name(_type == Type::Family ? _type : Type::Font) : sample_text);

        auto &alt = _alt_fontspec;
        auto font_desc =
            Glib::Markup::escape_text(is_present() ? Inkscape::get_font_description(_font.ff, _font.face).to_string()
                                                   : (alt.empty() ? "sans-serif" : alt));
        auto alpha = _missing_font ? "60%" : "100%";
        return Glib::ustring::format("<span allow_breaks='false' alpha='", alpha, "' size='", font_size_percent,
                                     "%' font='", font_desc, "'>", text, "</span>");
    }

private:
    FontElement(std::vector<FontInfo> family, FontInfo font, Glib::ustring alt, Type type)
        : _font(std::move(font))
        , _family(std::move(family))
        , _type(type)
        , _alt_fontspec(std::move(alt))
    {}

    Glib::ustring get_font_name(Type type) const
    {
        auto present = is_present();
        Glib::ustring name;
        Glib::RefPtr<Pango::FontFace> empty;

        switch (type) {
            case Type::Font:
                // full font name: family + style
                name = Glib::Markup::escape_text(present ? Inkscape::get_full_font_name(_font.ff, _font.face)
                                                         : get_alt_name(_alt_fontspec));
                break;
            case Type::Family:
                // font family name only
                name = Glib::Markup::escape_text(present ? Inkscape::get_full_font_name(_font.ff, empty)
                                                         : get_alt_name(_alt_fontspec));
                break;
            case Type::Style:
                // font style only
                name = Glib::Markup::escape_text(_font.face->get_name());
                break;
            default:
                assert(false);
                break;
        }
        return name;
    }

    FontInfo _font;
    std::vector<FontInfo> _family;
    // empty element to keep space in a store
    bool _placeholder = false;
    // if font is not present in a system, then show "missing font" icon
    bool _missing_font = false;
    // injected element - always show it first, regardless of filters or sorting order
    bool _injected = false;
    // this is a fontspec of the missing font
    Glib::ustring _alt_fontspec;
};

} // namespace Inkscape

#endif // FONT_ELEMENT_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
