// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Basic font info.
 *
 */

#ifndef FONT_INFO_H
#define FONT_INFO_H

namespace Inkscape {

enum class FontOrder {
    _First = 0,
    ByName = 0,
    ByWeight,
    ByWidth,
    ByFamily,
    _Last = ByFamily
};

struct FontInfo {
    Glib::RefPtr<Pango::FontFamily> ff;
    Glib::RefPtr<Pango::FontFace> face;
    Glib::ustring variations;    // pango-style font variations (if any)
    Glib::ustring fontspec;      // Typically output of Pango::FontDescription::to_string()
    double weight = 0;           // proxy for font weight - how black it is
    double width = 0;            // proxy for font width - how compressed/extended it is
    unsigned short family_kind = 0; // OS/2 family class     UNUSED
    bool monospaced = false;     // fixed-width font         UNUSED
    bool oblique = false;        // italic or oblique font   UNUSED
    bool variable_font = false;  // this is variable font
    bool synthetic = false;      // this is an alias, like "Sans" or "Monospace"

    bool operator == (const FontInfo&) const = default;
};

} // namespace Inkscape

#endif // FONT_INFO_H

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
