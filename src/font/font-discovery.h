// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FONT_DISCOVERY_H
#define FONT_DISCOVERY_H

#include <functional>
#include <glibmm/ustring.h>
#include <memory>
#include <sigc++/signal.h>
#include <vector>
#include <pangomm.h>
#include "async/operation-stream.h"
#include <sigc++/scoped_connection.h>
#include "libnrtype/font-factory.h"
#include "font/font-info.h"
#include "util/statics.h"

namespace Inkscape {

class FontDiscovery : public Util::EnableSingleton<FontDiscovery, Util::Depends<FontFactory>>
{
public:
    using FontsPayload = std::shared_ptr<const std::vector<std::vector<FontInfo>>>;
    using MessageType = Async::Msg::Message<FontsPayload, double, Glib::ustring, std::vector<FontInfo>>;

    sigc::scoped_connection connect_to_fonts(std::function<void (const MessageType&)> fn);

private:
    FontDiscovery();
    friend class EnableSingleton;

    FontsPayload _fonts;
    sigc::scoped_connection _connection;
    Inkscape::Async::OperationStream<FontsPayload, double, Glib::ustring, std::vector<FontInfo>> _loading;
    sigc::signal<void (const MessageType&)>_events;
};

// Use font factory and cached font details to return a list of all fonts available to Inkscape
std::vector<FontInfo> get_all_fonts();

// change order
void sort_fonts(std::vector<FontInfo>& fonts, FontOrder order, bool sans_first);

// sort font families
void sort_font_families(std::vector<std::vector<FontInfo>>& fonts, bool sans_first);

// get regular font from the family
const FontInfo& get_family_font(const std::vector<FontInfo>& family);
FontInfo& get_family_font(std::vector<FontInfo>& family);

// combine font style, weight, stretch and other traits to come up with a value
// that can be used to order font faces within the same family
int get_font_style_order(const Pango::FontDescription& desc);

} // namespace Inkscape

#endif // FONT_DISCOVERY_H
