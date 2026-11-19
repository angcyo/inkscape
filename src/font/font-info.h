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
    Glib::ustring variations;   // pango-style font variations (if any)
    double weight = 0;           // proxy for font weight - how black it is
    double width = 0;            // proxy for font width - how compressed/extended it is
    unsigned short family_kind = 0; // OS/2 family class
    bool monospaced = false;    // fixed-width font
    bool oblique = false;       // italic or oblique font
    bool variable_font = false; // this is variable font
    bool synthetic = false;     // this is an alias, like "Sans" or "Monospace"

    bool operator == (const FontInfo&) const = default;
};

} // namespace Inkscape

#endif // FONT_INFO_H


