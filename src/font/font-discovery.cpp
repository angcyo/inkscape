// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Michael Kowalski
 *
 * Copyright (C) 2022-2024 Michael Kowalski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "font-discovery.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <cairo-ft.h>
#include <cairomm/surface.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <glibmm/ustring.h>
#include <pango/pango-fontmap.h>
#include <pangomm/fontdescription.h>
#include <pangomm/fontmap.h>
#include <sigc++/connection.h>
#include <sigc++/scoped_connection.h>

#include "inkscape-application.h"
#include "async/progress.h"
#include "font/font-utils.h"
#include "io/resource.h"

// Used to find out if font is fixed width, oblique, etc.
#include "libnrtype/font-factory.h"
#include "libnrtype/font-instance.h"

namespace filesystem = std::filesystem;

namespace Inkscape {

// Attempt to estimate how heavy given typeface is by drawing some capital letters and counting
// black pixels (alpha channel). This is imperfect, but reasonable proxy for font weight, as long
// as Pango can instantiate correct font.
double calculate_font_weight(const Pango::FontDescription& desc_in, double caps_height) {
    auto desc = desc_in;

    // pixmap with enough room for a few characters; the rest will be cropped
    auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, 128, 64);
    auto context = Cairo::Context::create(surface);
    auto layout = Pango::Layout::create(context);
    const char* txt = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    layout->set_text(txt);
    auto size = 22 * PANGO_SCALE;
    if (caps_height > 0) {
        size /= caps_height;
    }
    desc.set_size(size);
    layout->set_font_description(desc);
    context->move_to(1, 1);
    layout->show_in_cairo_context(context);
    surface->flush();

    auto pixels = surface->get_data();
    auto width = surface->get_width();
    auto stride = surface->get_stride() / width;
    auto height = surface->get_height();
    double sum = 0;
    for (auto y = 0; y < height; ++y) {
        for (auto x = 0; x < width; ++x) {
            sum += pixels[3]; // read alpha
            pixels += stride;
        }
    }
    auto weight = sum / (width * height);
    return weight;
}

// calculate width of a A-Z string to try to measure average character width
double calculate_font_width(const Pango::FontDescription& desc_in) {
    auto desc = desc_in;
    auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, 1, 1);
    auto context = Cairo::Context::create(surface);
    auto layout = Pango::Layout::create(context);
    const char* txt = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    layout->set_text(txt);
    desc.set_size(72 * PANGO_SCALE);
    layout->set_font_description(desc);
    // layout->show_in_cairo_context(context);
    Pango::Rectangle ink, rect;
    layout->get_extents(ink, rect);
    return static_cast<double>(ink.get_width()) / PANGO_SCALE / strlen(txt);
}

// calculate value to order font's styles
int get_font_style_order(const Pango::FontDescription& desc) {
    return
        static_cast<int>(desc.get_weight())  * 1'000'000 +
        static_cast<int>(desc.get_style())   * 10'000 +
        static_cast<int>(desc.get_stretch()) * 100 +
        static_cast<int>(desc.get_variant());
}

// sort fonts in-place by name using lexicographical order; if 'sans_first' is true place "Sans" font first
void sort_fonts_by_name(std::vector<FontInfo>& fonts, bool sans_first) {
    std::sort(begin(fonts), end(fonts), [=](const FontInfo& a, const FontInfo& b) {
        auto na = a.ff->get_name();
        auto nb = b.ff->get_name();
        if (sans_first) {
            bool sans_a = a.synthetic && a.ff->get_name() == "Sans";
            bool sans_b = b.synthetic && b.ff->get_name() == "Sans";
            if (sans_a != sans_b) {
                return sans_a;
            }
        }
        // check family names first
        if (na != nb) {
            // lexicographical order:
            return na < nb;
            // alphabetical order:
            //return na.raw() < nb.raw();
        }
        return get_font_style_order(a.face->describe()) < get_font_style_order(b.face->describe());
    });
}

// sort fonts in requested order, in-place
void sort_fonts(std::vector<FontInfo>& fonts, FontOrder order, bool sans_first) {
    switch (order) {
        case FontOrder::ByName:
        case FontOrder::ByFamily:
            sort_fonts_by_name(fonts, sans_first);
            break;

        case FontOrder::ByWeight:
            // there are many repetitions for weight, due to font substitutions, so sort by name first
            sort_fonts_by_name(fonts, sans_first);
            std::stable_sort(begin(fonts), end(fonts), [](const FontInfo& a, const FontInfo& b) { return a.weight < b.weight; });
            break;

        case FontOrder::ByWidth:
            sort_fonts_by_name(fonts, sans_first);
            std::stable_sort(begin(fonts), end(fonts), [](const FontInfo& a, const FontInfo& b) { return a.width < b.width; });
            break;

        default:
            g_warning("Missing case in sort_fonts");
            break;
    }
}

const FontInfo& get_family_font(const std::vector<FontInfo>& family) {
    assert(!family.empty());
    auto it = std::ranges::find_if(family, [](auto& fam) {
        return fam.face &&
            (fam.face->get_name().raw().find("Regular") != std::string::npos ||
             fam.face->get_name().raw().find("Normal")  != std::string::npos);
    });
    if (it != end(family)) {
        return *it;
    }
    return family.front();
}

FontInfo& get_family_font(std::vector<FontInfo>& family) {
    return const_cast<FontInfo&>(get_family_font(const_cast<const std::vector<FontInfo>&>(family)));
}

void sort_font_families(std::vector<std::vector<FontInfo>>& fonts, bool sans_first) {
    std::sort(begin(fonts), end(fonts), [=](const auto& a, const auto& b) {
        auto& f1 = get_family_font(a);
        auto& f2 = get_family_font(b);

        auto na = f1.ff->get_name();
        auto nb = f2.ff->get_name();
        if (sans_first) {
            bool sans_a = f1.synthetic && f1.ff->get_name() == "Sans";
            bool sans_b = f2.synthetic && f2.ff->get_name() == "Sans";
            if (sans_a != sans_b) {
                return sans_a;
            }
        }
        // lexicographical order:
        return na < nb;
        // alphabetical order:
        //return na.raw() < nb.raw();
    });
}

// Font cache is a text file that stores under each font name some of its metadata, like average weight and height,
// as well as flags (monospaced, variable, oblique, synthetic font). It is kept to speed up font metadata discovery.
const char font_cache[] = "font-cache.ini";
const char cache_header[] = "@font-cache@";
constexpr auto cache_version = 1.0;
enum FontCacheFlags : int {
    Normal = 0,
    Monospace = 0x01,
    Oblique   = 0x02,
    Variable  = 0x04,
    Synthetic = 0x08,
};

void save_font_cache(const std::vector<std::vector<FontInfo>>& fonts) {
    auto keyfile = Glib::KeyFile::create();

    keyfile->set_double(cache_header, "version", cache_version);
    Glib::ustring weight("weight");
    Glib::ustring width("width");
    Glib::ustring ffamily("family");
    Glib::ustring fontflags("flags");

    for (auto&& family : fonts) {
        for (auto&& font : family) {
            auto group = font.fontspec;
            int flags = FontCacheFlags::Normal;
            if (font.monospaced) {
                flags |= FontCacheFlags::Monospace;
            }
            if (font.oblique) {
                flags |= FontCacheFlags::Oblique;
            }
            if (font.variable_font) {
                flags |= FontCacheFlags::Variable;
            }
            if (font.synthetic) {
                flags |= FontCacheFlags::Synthetic;
            }
            keyfile->set_double(group, weight, font.weight);
            keyfile->set_double(group, width, font.width);
            keyfile->set_integer(group, ffamily, font.family_kind);
            keyfile->set_integer(group, fontflags, flags);
        }
    }

    std::string filename = Glib::build_filename(Inkscape::IO::Resource::profile_path(), font_cache);
    keyfile->save_to_file(filename);
}

std::unordered_map<std::string, FontInfo> load_cached_font_info() {
    std::unordered_map<std::string, FontInfo> info;

    try {
        auto keyfile = Glib::KeyFile::create();
        std::string filename = Glib::build_filename(Inkscape::IO::Resource::profile_path(), font_cache);

#ifdef G_OS_WIN32
        bool exists = filesystem::exists(filesystem::u8path(filename));
#else
        bool exists = filesystem::exists(filesystem::path(filename));
#endif

        if (exists && keyfile->load_from_file(filename)) {

            auto ver = keyfile->get_double(cache_header, "version");
            if (std::abs(ver - cache_version) > 0.0001) return info;

            Glib::ustring weight("weight");
            Glib::ustring width("width");
            Glib::ustring family("family");
            Glib::ustring fontflags("flags");

            for (auto&& group : keyfile->get_groups()) {
                if (group == cache_header) continue;

                FontInfo font;
                auto flags = keyfile->get_integer(group, fontflags);
                if (flags & FontCacheFlags::Monospace) {
                    font.monospaced = true;
                }
                if (flags & FontCacheFlags::Oblique) {
                    font.oblique = true;
                }
                if (flags & FontCacheFlags::Variable) {
                    font.variable_font = true;
                }
                if (flags & FontCacheFlags::Synthetic) {
                    font.synthetic = true;
                }
                font.weight = keyfile->get_double(group, weight);
                font.width = keyfile->get_double(group, width);
                font.family_kind = keyfile->get_integer(group, family);

                info[group.raw()] = font;
            }
        }
    }
    catch (Glib::Error &error) {
        std::cerr << G_STRFUNC << ": font cache not loaded - " << error.what() << std::endl;
    }

    return info;
}

std::vector<FontInfo> get_all_fonts() {
    std::vector<FontInfo> fonts;
    return fonts;
}

std::shared_ptr<const std::vector<std::vector<FontInfo>>> get_all_fonts(Async::Progress<double, Glib::ustring, std::vector<FontInfo>>& progress) {
    auto result = std::make_shared<std::vector<std::vector<FontInfo>>>();
    auto& fonts = *result;
    auto cache = load_cached_font_info();

    std::vector<FontInfo> empty;
    progress.report_or_throw(0, "", empty);

    auto families = FontFactory::get().get_font_families();

    progress.throw_if_cancelled();
    bool update_cache = false;

    double counter = 0.0;
    for (auto ff : families) {
        bool synthetic_font = false;
        auto default_face = ff->get_face();
        if (default_face && default_face->is_synthesized()) {
            synthetic_font = true;
        }
        progress.report_or_throw(counter / families.size(), ff->get_name(), empty);
        std::vector<FontInfo> family;
        auto faces = ff->list_faces();
        std::set<std::string> styles;
        for (auto face : faces) {
            // skip synthetic faces of normal fonts, they pollute listing with fake entries,
            // but let entirely synthetic fonts in ("Sans", "Monospace", etc)
            if (!synthetic_font && face->is_synthesized()) continue;

            auto desc = face->describe();
            desc.unset_fields(Pango::FontMask::SIZE);
            auto variations = FontFactory::get().get_variations_string(face);
            desc.set_variations(variations);
            Glib::ustring fontspec = desc.to_string();
            fontspec = canonize_fontspec(fontspec);
            if (styles.count(fontspec)) {
                // This happens when:
                // - A user has both a variable font and the corresponding non-variable fonts installed.
                // - Multiple versions of the same font are installed differing by font-size.
                // - A font is divided into multiple files (e.g. HanaMin which has too many code points).
                // std::cerr << "get_all_fonts: duplicate fontspec! " << fontspec << std::endl;
                // for (auto file : get_font_files(face)) {
                //     std::cerr << "  " << file << std::endl;
                // }
                continue;
            }

            styles.insert(fontspec);

            FontInfo info = { ff, face, variations, fontspec };
            info.synthetic = synthetic_font;
            bool valid = false;

            auto it = cache.find(fontspec.raw());
            if (it == cache.end()) {
                // font not found in a cache; calculate metrics

                update_cache = true;

                double caps_height = 0.0;

                try {
                    auto font = FontFactory::get().create_face(desc.gobj());
                    if (!font) {
                        g_warning("Cannot load font %s", fontspec.c_str());
                    }
                    else {
                        valid = true;
                        info.monospaced = font->is_fixed_width();
                        info.oblique = font->is_oblique();
                        info.family_kind = font->family_class();
                        info.variable_font = !font->get_opentype_varaxes().empty();
                        caps_height = font->GetCapHeight();
                    }
                }
                catch (...) {
                    g_warning("Error loading font %s", fontspec.c_str());
                }
                info.weight = calculate_font_weight(desc, caps_height);
                info.width = calculate_font_width(desc);
            }
            else {
                // font in a cache already
                info = it->second;
                valid = true;
            }

            if (valid) {
                // If we got cached info, we need to save these details, since it's not in the FontInfo filled by the cache.
                info.ff = ff;
                info.face = face;
                info.variations = variations;
                info.fontspec = fontspec;

                family.emplace_back(info);
            }
            // std::cout << "get_all_fonts: FontInfo: "
            //           << " family: "     << std::setw(20) << info.ff->get_name()
            //           << " face: "       << std::setw(15) << info.face->get_name()
            //           << " variations: " << std::setw(10) << info.variations
            //           << " fontspec: "   << info.fontspec
            //           << std::endl;
        }
        if (!family.empty()) {
            fonts.push_back(family);
        }
        progress.report_or_throw(++counter / families.size(), "", family);
    }

    if (update_cache) {
        save_font_cache(fonts);
    }

    progress.report_or_throw(1, "", empty);

    return result;
}

FontDiscovery::FontDiscovery() {
    if (auto i = InkscapeApplication::instance()) {
        i->gio_app()->signal_shutdown().connect([this](){
            _loading.cancel();
        });
    }

    _connection = _loading.subscribe([this](const MessageType& msg) {
        if (auto result = Async::Msg::get_result(msg)) {
            // cache results
            _fonts = *result;
        }
        // propagate events
        _events.emit(msg);
    });
}

sigc::scoped_connection FontDiscovery::connect_to_fonts(std::function<void (const MessageType&)> fn) {

    sigc::scoped_connection con = static_cast<sigc::connection>(_events.connect(fn));

    if (!_fonts && !_loading.is_running()) {
        // load fonts async
        _loading.start(
            [=](Async::Progress<double, Glib::ustring, std::vector<FontInfo>>& p) { return get_all_fonts(p); }
        );
    }
    else if (_fonts) {
        // fonts already loaded
        _events.emit(Async::Msg::OperationResult<FontsPayload>{_fonts});
        _events.emit(Async::Msg::OperationFinished());
    }

    return con;
}

} // namespace

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
