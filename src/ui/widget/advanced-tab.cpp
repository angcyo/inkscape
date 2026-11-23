// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Advanced Tab for F&S
 */
/* Authors:
 *   Ayan Das <ayandazzz@outlook.com>
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "advanced-tab.h"

#include <glibmm/i18n.h>

#include "colors/document-cms.h"
#include "colors/manager.h"
#include "colors/spaces/cms.h"
#include "desktop-style.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "recolor-art-manager.h"
#include "style.h"
#include "ui/builder-utils.h"

namespace Inkscape::UI::Widget {

AdvancedTab::AdvancedTab()
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , _builder(create_builder("advanced-tab.ui"))
    , _box(&get_widget<Gtk::Box>(_builder, "recolor_box"))
{
    append(get_widget<Gtk::Box>(_builder, "advanced-tab-root"));
    this->set_vexpand(true);

    _recolor_widget = Gtk::make_managed<RecolorArt>();
    _recolor_widget->set_margin_top(8);
    _box->append(*_recolor_widget);
}

void AdvancedTab::setDesktop(SPDesktop *desktop)
{
    _desktop = desktop;

    if (!_pause_updates) {
        _recolor_widget->showForSelection(_desktop);
        _needs_update = false;
    } else {
        _needs_update = true;
    }
}

void AdvancedTab::on_map()
{
    _pause_updates = false;

    Gtk::Widget::on_map();

    // Upstream widget requires a refresh on map, and needs refactoring, see on_unmap below..
    _recolor_widget->showForSelection(_desktop);
    _needs_update = false;
}

void AdvancedTab::on_unmap()
{
    _pause_updates = true;
    Gtk::Widget::on_unmap();

    // Upstream widget expects this on unmap, it's not good and needs refactoring.
    _recolor_widget->showForSelection(nullptr);
}

} // namespace Inkscape::UI::Widget

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
