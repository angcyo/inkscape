// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Font selection utilities, mostly dealing with fontspecs.
 */
/*
 * Authors:
 *   See Git history
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <map>
#include <string>
#include <vector>

#include <glibmm/ustring.h>
#include <pangomm/fontdescription.h>
#include <pangomm/fontface.h>
#include <pangomm/fontfamily.h>

namespace Inkscape {

// -----------  fontspec ----------

// fontspec is essentially what Pango::FontDescription::to_string() returns.

// Construct a fontspec from component strings.
Glib::ustring get_fontspec(const Glib::ustring& family, const Glib::ustring& face, const Glib::ustring& variations);

// Construct a fontspec from component strings (without variations). TEST
Glib::ustring get_fontspec(const Glib::ustring& family, const Glib::ustring& face);

// Construct a fontspec from Pango variables. Empty string if no font family. TEST
Glib::ustring get_fontspec(const Glib::RefPtr<Pango::FontFamily>& ff, const Glib::RefPtr<Pango::FontFace>& face, const Glib::ustring& variations);

// Remove variatins from fontspec. TEST
Glib::ustring get_fontspec_without_variants(const Glib::ustring& fontspec);

// Pass fontspec to and back from Pango to get the fontspec in canonical form.
Glib::ustring canonize_fontspec(Glib::ustring const &fontspec);


// ---------- Pango::FontDescription -----------

// The following functions create or use Pango::FontDescription.

// Construct a Pango::FontDescription from Pango variables. TEST
Pango::FontDescription get_font_description(const Glib::RefPtr<Pango::FontFamily>& ff, const Glib::RefPtr<Pango::FontFace>& face);

// Extract style from Pango FontDescription. TEST
Glib::ustring get_face_style(const Pango::FontDescription& desc);


//    ------- Utilities  -------

// Get name from Pango face and family as recorded in font (as far as Pango allows it).
// Differs from get_fontspec() only in that there is no comma between family and face.
Glib::ustring get_full_font_name(Glib::RefPtr<Pango::FontFamily> ff, Glib::RefPtr<Pango::FontFace> face);

// Get file paths for face source.
std::vector<std::string> get_font_files(Glib::RefPtr<Pango::FontFace> face);

// Returns a map of 'tag' => 'value' from a variations string.
std::map<std::string, std::string> parse_variations(const char* variations);

} // namespace Inkscape

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
