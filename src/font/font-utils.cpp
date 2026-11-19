// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   See Git history
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "font-utils.h"

#include <iostream> // TEMP TEMP TEMP
#include <glibmm/regex.h>
#include <pango/pango-font.h>

namespace Inkscape {

Glib::ustring get_fontspec(const Glib::ustring& family, const Glib::ustring& face, const Glib::ustring& variations) {
    if (variations.empty()) {
        return face.empty() ? family : family + ", " + face;
    }
    else {
        auto desc = (face.empty() ? family : family + ", " + face) + " " + variations;
        return desc;
    }
}

Glib::ustring get_fontspec(const Glib::ustring& family, const Glib::ustring& face) {
    return get_fontspec(family, face, Glib::ustring());
}

Glib::ustring get_fontspec_without_variants(const Glib::ustring& fontspec) {
    auto at = fontspec.rfind('@');
    if (at != Glib::ustring::npos && at > 0) {
        // remove variations
        while (at > 0 && fontspec[at - 1] == ' ') at--; // trim spaces

        return fontspec.substr(0, at);
    }
    return fontspec;
}

Glib::ustring get_inkscape_fontspec(const Glib::RefPtr<Pango::FontFamily>& ff, const Glib::RefPtr<Pango::FontFace>& face, const Glib::ustring& variations) {
    if (!ff) return Glib::ustring();

    return get_fontspec(ff->get_name(), face ? get_face_style(face->describe()) : Glib::ustring(), variations);
}

// Pass fontspec to and back from Pango to get a the fontspec in canonical form. TEST
Glib::ustring canonize_fontspec(Glib::ustring const &fontspec)
{
    PangoFontDescription *descr = pango_font_description_from_string(fontspec.c_str());

    // CSS Font Module Level 4 dictates that "font-weight", "font-width' ("font-stretch"),
    // "font-style", and "font-optical-sizing" should be used rather than the
    // "font-variation-settings" axes 'wght', 'wdth', 'slnt'/'ital', or 'opsz'.
    // At the moment (May 2026), Pango only provides adequate support for doing so for
    // font weight.
    //
    // Pango font description does not order the axes. This can cause problems when
    // trying to match named instances. Reorder alphabetically here.
    std::string variations;

    const char* str = pango_font_description_get_variations(descr);
    if (str) {
        auto variations_map = parse_variations(str);
        for (auto [tag, value] : variations_map) {
            if (tag == "wght") {
                auto weight = std::stoi(value);
                pango_font_description_set_weight(descr, (PangoWeight)weight);
            } else if (tag == "ital" && value == "1") {
                pango_font_description_set_style(descr, PANGO_STYLE_ITALIC);
            } else {
                variations += tag;
                variations += "=";
                variations += value;
                variations += ",";
            }
        }

        if (variations.length() >= 1) { // Remove last comma and save
            variations.pop_back();
        }

        pango_font_description_set_variations(descr, variations.c_str());
    }

    gchar *canonized = pango_font_description_to_string(descr);
    Glib::ustring Canonized = canonized;
    g_free(canonized);
    pango_font_description_free(descr);

    // Pango canonized strings remove space after comma between family names. Put it back.
    // But don't add a space inside a 'font-variation-settings' declaration (this breaks Pango).
    size_t i = 0;
    while ((i = Canonized.find_first_of(",@", i)) != std::string::npos ) {
        if (Canonized[i] == '@') // Found start of 'font-variation-settings'.
            break;
        Canonized.replace(i, 1, ", ");
        i += 2;
    }

    return Canonized;
}


Pango::FontDescription get_font_description(const Glib::RefPtr<Pango::FontFamily>& ff, const Glib::RefPtr<Pango::FontFace>& face) {
    if (!face) return Pango::FontDescription("sans serif");

    auto desc = face->describe();
    desc.unset_fields(Pango::FontMask::SIZE);
    return desc;
}

Glib::ustring get_face_style(const Pango::FontDescription& desc) {
    Pango::FontDescription copy(desc);
    copy.unset_fields(Pango::FontMask::FAMILY);
    copy.unset_fields(Pango::FontMask::SIZE);
    auto str = copy.to_string();
    return str;
}


// Construct font name from Pango face and family as recorded in font, as far as Pango allows it.
// This differs from get_fontspec() only in that there is no comma between family and face.
Glib::ustring get_full_font_name(Glib::RefPtr<Pango::FontFamily> ff, Glib::RefPtr<Pango::FontFace> face) {
    if (!ff) return "";

    auto family = ff->get_name();
    auto face_name = face ? face->get_name() : Glib::ustring();
    auto name = face_name.empty() ? family : family + ' ' + face_name;
    return name;
}

// Returns a map of 'tag' => 'value' from a variations string. TEST
std::map<std::string, std::string> parse_variations(const char* variations)
{
    std::map<std::string, std::string> variations_map;

    auto regex = Glib::Regex::create("(\\w{4})=([-+]?\\d*\\.?\\d+([eE][-+]?\\d+)?)");
    Glib::MatchInfo matchInfo;

    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(",", variations);
    for (auto const &token : tokens) {
        regex->match(token, matchInfo);
        if (matchInfo.matches()) {
            auto tag   = matchInfo.fetch(1).raw();
            auto value = matchInfo.fetch(2).raw();
            variations_map[tag] = value; // This will alphabetize axes based on tag.
        }
    }

    return variations_map;
}

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
