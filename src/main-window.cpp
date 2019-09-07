#include <cstdlib>
#include <cwctype>
#include <queue>
#include <utility>

#pragma warning(push, 0)
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_BMP_Image.H>
#include <FL/Fl_Image_Surface.H>
#pragma warning(pop)

#include "version.h"
#include "utils.h"
#include "themes.h"
#include "widgets.h"
#include "modal-dialog.h"
#include "option-dialogs.h"
#include "preferences.h"
#include "config.h"
#include "image.h"
#include "tilemap.h"
#include "tileset.h"
#include "tile.h"
#include "main-window.h"
#include "icons.h"

#ifdef _WIN32
#include "resource.h"
#else
#include <unistd.h>
#include <X11/xpm.h>
#include "app-icon.xpm"
#endif

Main_Window::Main_Window(int x, int y, int w, int h, const char *) : Fl_Double_Window(x, y, w, h, PROGRAM_NAME),
	_tile_buttons(), _tilemap_file(), _tileset_files(), _recent_tilemaps(), _recent_tilesets(), _tilemap(), _tilesets(),
	_wx(x), _wy(y), _ww(w), _wh(h) {

	Tile_State::tilesets(&_tilesets);

	// Get global configs
	Tilemap_Format format_config = (Tilemap_Format)Preferences::get("format", Config::format());
	int grid_config = Preferences::get("grid", Config::grid());
	int rainbow_tiles_config = Preferences::get("rainbow", Config::rainbow_tiles());
	int attributes_config = Preferences::get("colors", Config::attributes());
	Config::format(format_config);
	Config::grid(!!grid_config);
	Config::rainbow_tiles(!!rainbow_tiles_config);
	Config::attributes(!!attributes_config);

	for (int i = 0; i < NUM_RECENT; i++) {
		_recent_tilemaps[i] = Preferences::get_string(Fl_Preferences::Name("recent-map%d", i));
		_recent_tilesets[i] = Preferences::get_string(Fl_Preferences::Name("recent-set%d", i));
	}

	// Populate window

	int wx = 0, wy = 0, ww = w, wh = h;

	// Initialize menu bar
	_menu_bar = new Fl_Menu_Bar(wx, wy, ww, 21);
	wy += _menu_bar->h();
	wh -= _menu_bar->h();

	// Initialize status bar
	_status_bar = new Toolbar(wx, wh+_menu_bar->h()-23, ww, 23);
	wh -= _status_bar->h();
	_tilemap_dimensions = new Status_Bar_Field(0, 0, text_width("Tilemap: 999 x 999", 8), 21, "");
	new Spacer(0, 0, 2, 21);
	int wgt_w = 0;
	for (int i = 0; i < NUM_FORMATS; i++) {
		const char *l = format_name((Tilemap_Format)i);
		int lw = text_width(l, 8);
		if (lw > wgt_w) { wgt_w = lw; }
	}
	_tilemap_format = new Status_Bar_Field(0, 0, wgt_w, 21, "");
	new Spacer(0, 0, 2, 21);
	_hover_id = new Status_Bar_Field(0, 0, text_width("ID: $99", 8), 21, "");
	new Spacer(0, 0, 2, 21);
	_hover_xy = new Status_Bar_Field(0, 0, text_width("X/Y (999, 999)", 8), 21, "");
	new Spacer(0, 0, 2, 21);
	_hover_landmark = new Status_Bar_Field(0, 0, text_width("Landmark (999, 999)", 8), 21, "");
	_status_bar->end();
	begin();

	// Main group
	int wgt_h = 22, win_m = 10, wgt_m = 4;
	_main_group = new Fl_Group(wx, wy, ww, wh);
	wx += win_m; ww -= win_m * 2;
	wy += win_m; wh -= win_m * 2;
	// Left group
	_left_group = new Fl_Group(wx, wy, ww-280, wh);
	int gx = _left_group->x(), gy = _left_group->y(), gw = _left_group->w(), gh = _left_group->h();
	_left_top_bar = new Fl_Group(gx, gy, gw, wgt_h);
	int bx = _left_top_bar->x(), by = _left_top_bar->y(), bw = _left_top_bar->w(), bh = _left_top_bar->h();
	wgt_w = text_width("Tilemap:", 4);
	Label *tilemap_heading = new Label(bx, by, wgt_w, bh, "Tilemap:");
	bx += tilemap_heading->w(); bw -= tilemap_heading->w();
	_new_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _new_tb->w(); bw -= _new_tb->w();
	_open_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _open_tb->w(); bw -= _open_tb->w();
	_save_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _save_tb->w(); bw -= _save_tb->w();
	_print_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _print_tb->w() + win_m; bw -= _print_tb->w() + win_m;
	int wgt_off = text_width("Format:", 2);
	bx += wgt_off; bw -= wgt_off;
	_format = new Dropdown(bx, by, bw, bh, "Format:");
	_left_top_bar->end();
	_left_top_bar->resizable(_format);
	gy += _left_top_bar->h() + wgt_m; gh -= _left_top_bar->h() + wgt_m;
	_left_group->begin();
	_left_bottom_bar = new Fl_Group(gx, gy, gw, wgt_h);
	bx = _left_bottom_bar->x(), by = _left_bottom_bar->y(), bw = _left_bottom_bar->w(), bh = _left_bottom_bar->h();
	wgt_off = wgt_h * 4 + wgt_m + 6 + text_width("Width:", 2) + text_width("999", 2);
	wgt_w = bw - wgt_off;
	_tilemap_name = new Label_Button(bx, by, wgt_w, bh);
	bx += wgt_w;
	_undo_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _undo_tb->w();
	_redo_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _redo_tb->w();
	new Fl_Box(bx, by, 2, wgt_h); bx += 2;
	new Spacer(bx, by, 2, wgt_h); bx += 2;
	new Fl_Box(bx, by, 2, wgt_h); bx += 2;
	wgt_off = text_width("Width:", 2);
	wgt_w = text_width("999", 2) + wgt_h;
	_tilemap_width = new Default_Spinner(bx+wgt_off, by, wgt_w, wgt_h, "Width:");
	bx += wgt_off + wgt_w + wgt_m; bw -= wgt_off + wgt_w + wgt_m;
	_resize_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _resize_tb->w();
	_left_bottom_bar->end();
	_left_bottom_bar->resizable(_tilemap_name);
	gy += _left_bottom_bar->h() + wgt_m; gh -= _left_bottom_bar->h() + wgt_m;
	_left_group->begin();
	_tilemap_scroll = new Workspace(gx, gy, gw, gh);
	_tilemap_scroll->end();
	_tilemap_scroll->resizable(NULL);
	_left_group->resizable(_tilemap_scroll);
	wx += _left_group->w() + win_m; ww -= _left_group->w() + win_m;
	// Middle spacer
	_main_group->begin();
	Spacer *mid_spacer = new Spacer(wx, wy, 2, wh);
	wx += mid_spacer->w() + win_m; ww -= mid_spacer->w() + win_m;
	// Right group
	_right_group = new Fl_Group(wx, wy, ww, wh);
	_right_top_bar = new Fl_Group(wx, wy, ww, wgt_h);
	bx = _right_top_bar->x(), by = _right_top_bar->y(), bw = _right_top_bar->w(), bh = _right_top_bar->h();
	wgt_w = text_width("Tileset:", 4);
	Label *tileset_heading = new Label(bx, by, wgt_w, bh, "Tileset:");
	bx += tileset_heading->w(); bw -= tileset_heading->w();
	_load_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _load_tb->w(); bw -= _load_tb->w();
	_add_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	bx += _add_tb->w(); bw -= _add_tb->w();
	_reload_tb = new Toolbar_Button(bx, by, wgt_h, wgt_h);
	_image_to_tiles_tb = new Toolbar_Button(bx+bw-wgt_h, by, wgt_h, wgt_h);
	_right_top_bar->end();
	_right_top_bar->resizable(NULL);
	wy += _right_top_bar->h() + wgt_m;
	_right_group->begin();
	_right_bottom_bar = new Fl_Group(wx, wy, ww, wgt_h);
	bx = _right_bottom_bar->x(), by = _right_bottom_bar->y(), bw = _right_bottom_bar->w(), bh = _right_bottom_bar->h();
	wgt_off = text_width("Tile: $FFF", 2) + wgt_m;
	wgt_w = bw - wgt_off;
	_tileset_name = new Label_Button(bx, by, wgt_w, bh);
	bx += wgt_w + wgt_m; bw -= wgt_w + wgt_m;
	_tile_heading = new Label(bx, by, bw, bh, "Tile: $00");
	_right_bottom_bar->end();
	_right_bottom_bar->resizable(_tileset_name);
	wy += _right_bottom_bar->h() + wgt_m;
	_right_group->begin();
	_tileset_pane = new Workpane(wx, wy, 258, 258);
	int ox = wx + Fl::box_dx(_tileset_pane->box()), oy = wy + Fl::box_dy(_tileset_pane->box());
	for (int i = 0; i < NUM_TILES; i++) {
		int tx = ox + (i % TILES_PER_ROW) * TILE_SIZE_2X, ty = oy + (i / TILES_PER_ROW) * TILE_SIZE_2X;
		Tile_Button *tb = new Tile_Button(tx, ty, (uint8_t)i);
		tb->callback((Fl_Callback *)select_tile_cb, this);
		_tile_buttons[i] = tb;
	}
	_tileset_pane->end();
	_tileset_pane->resizable(NULL);
	wy += _tileset_pane->h() + win_m;
	_right_group->begin();
	wgt_w = MAX(text_width("A", 2), text_width("F", 2)) + wgt_h;
	wgt_off = text_width("Color:", 3);
	_color = new Default_Spinner(wx+wgt_off, wy, wgt_w, wgt_h, "Color:");
	wx += _color->w() + wgt_off + 2; ww -= _color->w() + wgt_off + 2;
	_show_attributes = new Toggle_Switch(wx, wy, wgt_h / 2 + 2, wgt_h);
	wx += _show_attributes->w() + wgt_m; ww -= _show_attributes->w() + wgt_m;
	wgt_w = text_width("Flip:", 4);
	_flip_heading = new Label(wx, wy, wgt_w, wgt_h, "Flip:");
	wx += _flip_heading->w(); ww -= _flip_heading->w();
	wgt_w = text_width("X", 2) + wgt_h;
	_x_flip = new OS_Check_Button(wx, wy, wgt_w, wgt_h, "X");
	wx += _x_flip->w(); ww -= _x_flip->w();
	wgt_w = text_width("Y", 2) + wgt_h;
	_y_flip = new OS_Check_Button(wx, wy, wgt_w, wgt_h, "Y");
	_right_group->end();
	_right_group->resizable(NULL);
	_main_group->begin();
	_main_group->resizable(_left_group);

	// Dialogs
	_tilemap_open_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE);
	_tilemap_save_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
	_tileset_load_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE);
	_png_chooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
	_error_dialog = new Modal_Dialog(this, "Error", Modal_Dialog::ERROR_ICON);
	_warning_dialog = new Modal_Dialog(this, "Warning", Modal_Dialog::WARNING_ICON);
	_success_dialog = new Modal_Dialog(this, "Success", Modal_Dialog::SUCCESS_ICON);
	_unsaved_dialog = new Modal_Dialog(this, "Warning", Modal_Dialog::WARNING_ICON, true);
	_about_dialog = new Modal_Dialog(this, "About " PROGRAM_NAME, Modal_Dialog::APP_ICON);
	_new_tilemap_dialog = new New_Tilemap_Dialog("New Tilemap");
	_tilemap_width_dialog = new Tilemap_Width_Dialog("Tilemap Width");
	_resize_dialog = new Resize_Dialog("Resize Tilemap");
	_add_tileset_dialog = new Add_Tileset_Dialog("Add Tileset");
	_image_to_tiles_dialog = new Image_To_Tiles_Dialog("Image to Tiles");
	_help_window = new Help_Window(48, 48, 500, 400, PROGRAM_NAME " Help");

	// Drag-and-drop receivers
	_tilemap_dnd_receiver = new DnD_Receiver(0, 0, 0, 0);
	_tilemap_dnd_receiver->callback((Fl_Callback *)drag_and_drop_tilemap_cb, this);
	_tilemap_dnd_receiver->user_data(this);
	_tileset_dnd_receiver = new DnD_Receiver(0, 0, 0, 0);
	_tileset_dnd_receiver->callback((Fl_Callback *)drag_and_drop_tileset_cb, this);
	_tileset_dnd_receiver->user_data(this);

	// Configure window
	size_range(622, 406);
	resizable(_main_group);
	callback((Fl_Callback *)exit_cb, this);
	xclass(PROGRAM_NAME);

	// Configure window icon
#ifdef _WIN32
	icon((const void *)LoadIcon(fl_display, MAKEINTRESOURCE(IDI_ICON1)));
#else
	fl_open_display();
	XpmCreatePixmapFromData(fl_display, DefaultRootWindow(fl_display), (char **)&APP_ICON_XPM, &_icon_pixmap, &_icon_mask, NULL);
	icon((const void *)_icon_pixmap);
#endif

	// Configure workspaces
	_tilemap_scroll->dnd_receiver(_tilemap_dnd_receiver);
	_tileset_pane->dnd_receiver(_tileset_dnd_receiver);

	// Configure menu bar
	_menu_bar->box(OS_PANEL_THIN_UP_BOX);
	_menu_bar->down_box(FL_FLAT_BOX);

	// Configure menu bar items
	Fl_Menu_Item menu_items[] = {
		// label, shortcut, callback, data, flags
		OS_SUBMENU("&File"),
		OS_MENU_ITEM("&New", FL_COMMAND + 'n', (Fl_Callback *)new_cb, this, 0),
		OS_MENU_ITEM("&Open...", FL_COMMAND + 'o', (Fl_Callback *)open_cb, this, 0),
		OS_MENU_ITEM("Open &Recent", 0, NULL, NULL, FL_SUBMENU),
		// NUM_RECENT items with callback open_recent_tilemap_cb
		OS_NULL_MENU_ITEM(FL_ALT + '1', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '2', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '3', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '4', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '5', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '6', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '7', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '8', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '9', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + '0', (Fl_Callback *)open_recent_tilemap_cb, this, 0),
		OS_MENU_ITEM("Clear &Recent", 0, (Fl_Callback *)clear_recent_tilemaps_cb, this, 0),
		{},
		OS_MENU_ITEM("&Close", FL_COMMAND + 'w', (Fl_Callback *)close_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Save", FL_COMMAND + 's', (Fl_Callback *)save_cb, this, 0),
		OS_MENU_ITEM("Save &As...", FL_COMMAND + 'S', (Fl_Callback *)save_as_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("Load &Tileset...", FL_COMMAND + 't', (Fl_Callback *)load_tileset_cb, this, 0),
		OS_MENU_ITEM("A&dd Tileset...", FL_COMMAND + 'a', (Fl_Callback *)add_tileset_cb, this, 0),
		OS_MENU_ITEM("Re&load Tilesets", FL_COMMAND + 'r', (Fl_Callback *)reload_tilesets_cb, this, 0),
		OS_MENU_ITEM("Load R&ecent", 0, NULL, NULL, FL_SUBMENU),
		// NUM_RECENT items with callback load_recent_tileset_cb
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '1', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '2', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '3', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '4', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '5', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '6', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '7', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '8', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '9', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_NULL_MENU_ITEM(FL_ALT + FL_SHIFT + '0', (Fl_Callback *)load_recent_tileset_cb, this, 0),
		OS_MENU_ITEM("Clear &Recent", 0, (Fl_Callback *)clear_recent_tilesets_cb, this, 0),
		{},
		OS_MENU_ITEM("&Unload Tilesets", FL_COMMAND + 'W', (Fl_Callback *)unload_tilesets_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Print...", FL_COMMAND + 'p', (Fl_Callback *)print_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("E&xit", FL_ALT + FL_F + 4, (Fl_Callback *)exit_cb, this, 0),
		{},
		OS_SUBMENU("&Edit"),
		OS_MENU_ITEM("&Undo", FL_COMMAND + 'z', (Fl_Callback *)undo_cb, this, 0),
		OS_MENU_ITEM("&Redo", FL_COMMAND + 'y', (Fl_Callback *)redo_cb, this, 0),
		{},
		OS_SUBMENU("&View"),
		OS_MENU_ITEM("&Theme", 0, NULL, NULL, FL_SUBMENU | FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Classic", 0, (Fl_Callback *)classic_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::CLASSIC ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Aero", 0, (Fl_Callback *)aero_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::AERO ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Metro", 0, (Fl_Callback *)metro_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::METRO ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("A&qua", 0, (Fl_Callback *)aqua_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::AQUA ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Greybird", 0, (Fl_Callback *)greybird_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::GREYBIRD ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Me&tal", 0, (Fl_Callback *)metal_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::METAL ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Blue", 0, (Fl_Callback *)blue_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::BLUE ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Olive", 0, (Fl_Callback *)olive_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::OLIVE ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Rose Gold", 0, (Fl_Callback *)rose_gold_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::ROSE_GOLD ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Dark", 0, (Fl_Callback *)dark_theme_cb, this,
			FL_MENU_RADIO | (OS::current_theme() == OS::DARK ? FL_MENU_VALUE : 0)),
		{},
		OS_MENU_ITEM("&Grid", FL_COMMAND + 'g', (Fl_Callback *)grid_cb, this,
			FL_MENU_TOGGLE | (Config::grid() ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&Rainbow Tiles", FL_COMMAND + 'b', (Fl_Callback *)rainbow_tiles_cb, this,
			FL_MENU_TOGGLE | (Config::rainbow_tiles() ? FL_MENU_VALUE : 0)),
		{},
		OS_SUBMENU("&Tools"),
		OS_MENU_ITEM("&Width...", FL_COMMAND + 'd', (Fl_Callback *)tilemap_width_cb, this, 0),
		OS_MENU_ITEM("Re&size...", FL_COMMAND + 'e', (Fl_Callback *)resize_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("&Image to Tiles...", FL_COMMAND + 'i', (Fl_Callback *)image_to_tiles_cb, this, 0),
		{},
		OS_SUBMENU("&Options"),
		OS_MENU_ITEM("&Format", 0, NULL, NULL, FL_SUBMENU),
		OS_MENU_ITEM("Plain &tiles", FL_COMMAND + '1', (Fl_Callback *)plain_format_cb, this,
			FL_MENU_RADIO | (Config::format() == Tilemap_Format::PLAIN ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Tiles + &attributes", FL_COMMAND + '2', (Fl_Callback *)sgb_border_format_cb, this,
			FL_MENU_RADIO | (Config::format() == Tilemap_Format::TILE_ATTR ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&RBY Town Map", FL_COMMAND + '3', (Fl_Callback *)rby_town_map_format_cb, this,
			FL_MENU_RADIO | (Config::format() == Tilemap_Format::RLE_NYBBLES ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&GSC Town Map", FL_COMMAND + '4', (Fl_Callback *)gsc_town_map_format_cb, this,
			FL_MENU_RADIO | (Config::format() == Tilemap_Format::FF_END ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("&PC Town Map", FL_COMMAND + '5', (Fl_Callback *)pc_town_map_format_cb, this,
			FL_MENU_RADIO | (Config::format() == Tilemap_Format::XY_FLIP ? FL_MENU_VALUE : 0)),
		OS_MENU_ITEM("Pok\xc3\xa9gear &card", FL_COMMAND + '6', (Fl_Callback *)pokegear_card_format_cb, this,
			FL_MENU_RADIO | (Config::format() == Tilemap_Format::RLE_FF_END ? FL_MENU_VALUE : 0)),
		{},
		OS_MENU_ITEM("Show SGB &Colors", FL_COMMAND + 'G', (Fl_Callback *)show_attributes_cb, this,
			FL_MENU_TOGGLE | (Config::attributes() ? FL_MENU_VALUE : 0)),
		{},
		OS_SUBMENU("&Help"),
		OS_MENU_ITEM("&Help", FL_F + 1, (Fl_Callback *)help_cb, this, FL_MENU_DIVIDER),
		OS_MENU_ITEM("&About", FL_COMMAND + '/', (Fl_Callback *)about_cb, this, 0),
		{},
		{}
	};
	_menu_bar->copy(menu_items);

	// Initialize menu bar items
	int first_recent_tilemap_i = _menu_bar->find_index((Fl_Callback *)open_recent_tilemap_cb);
	int first_recent_tileset_i = _menu_bar->find_index((Fl_Callback *)load_recent_tileset_cb);
	for (int i = 0; i < NUM_RECENT; i++) {
		_recent_tilemap_mis[i] = const_cast<Fl_Menu_Item *>(&_menu_bar->menu()[first_recent_tilemap_i + i]);
		_recent_tileset_mis[i] = const_cast<Fl_Menu_Item *>(&_menu_bar->menu()[first_recent_tileset_i + i]);
	}
#define PM_FIND_MENU_ITEM_CB(c) (const_cast<Fl_Menu_Item *>(_menu_bar->find_item((Fl_Callback *)(c))))
	_classic_theme_mi = PM_FIND_MENU_ITEM_CB(classic_theme_cb);
	_aero_theme_mi = PM_FIND_MENU_ITEM_CB(aero_theme_cb);
	_metro_theme_mi = PM_FIND_MENU_ITEM_CB(metro_theme_cb);
	_aqua_theme_mi = PM_FIND_MENU_ITEM_CB(aqua_theme_cb);
	_greybird_theme_mi = PM_FIND_MENU_ITEM_CB(greybird_theme_cb);
	_metal_theme_mi = PM_FIND_MENU_ITEM_CB(metal_theme_cb);
	_blue_theme_mi = PM_FIND_MENU_ITEM_CB(blue_theme_cb);
	_olive_theme_mi = PM_FIND_MENU_ITEM_CB(olive_theme_cb);
	_rose_gold_theme_mi = PM_FIND_MENU_ITEM_CB(rose_gold_theme_cb);
	_dark_theme_mi = PM_FIND_MENU_ITEM_CB(dark_theme_cb);
	_plain_format_mi = PM_FIND_MENU_ITEM_CB(plain_format_cb);
	_sgb_border_format_mi = PM_FIND_MENU_ITEM_CB(sgb_border_format_cb);
	_rby_town_map_format_mi = PM_FIND_MENU_ITEM_CB(rby_town_map_format_cb);
	_gsc_town_map_format_mi = PM_FIND_MENU_ITEM_CB(gsc_town_map_format_cb);
	_pc_town_map_format_mi = PM_FIND_MENU_ITEM_CB(pc_town_map_format_cb);
	_pokegear_card_format_mi = PM_FIND_MENU_ITEM_CB(pokegear_card_format_cb);
	_grid_mi = PM_FIND_MENU_ITEM_CB(grid_cb);
	_rainbow_tiles_mi = PM_FIND_MENU_ITEM_CB(rainbow_tiles_cb);
	_show_attributes_mi = PM_FIND_MENU_ITEM_CB(show_attributes_cb);
	// Conditional menu items
	_reload_tilesets_mi = PM_FIND_MENU_ITEM_CB(reload_tilesets_cb);
	_unload_tilesets_mi = PM_FIND_MENU_ITEM_CB(unload_tilesets_cb);
	_close_mi = PM_FIND_MENU_ITEM_CB(close_cb);
	_save_mi = PM_FIND_MENU_ITEM_CB(save_cb);
	_save_as_mi = PM_FIND_MENU_ITEM_CB(save_as_cb);
	_print_mi = PM_FIND_MENU_ITEM_CB(print_cb);
	_undo_mi = PM_FIND_MENU_ITEM_CB(undo_cb);
	_redo_mi = PM_FIND_MENU_ITEM_CB(redo_cb);
	_tilemap_width_mi = PM_FIND_MENU_ITEM_CB(tilemap_width_cb);
	_resize_mi = PM_FIND_MENU_ITEM_CB(resize_cb);
#undef PM_FIND_MENU_ITEM_CB

	// Configure toolbar buttons

	_new_tb->tooltip("New... (Ctrl+N)");
	_new_tb->callback((Fl_Callback *)new_cb, this);
	_new_tb->image(NEW_ICON);
	_new_tb->take_focus();

	_open_tb->tooltip("Open... (Ctrl+O)");
	_open_tb->callback((Fl_Callback *)open_cb, this);
	_open_tb->image(OPEN_ICON);

	_save_tb->tooltip("Save (Ctrl+S)");
	_save_tb->callback((Fl_Callback *)save_cb, this);
	_save_tb->image(SAVE_ICON);
	_save_tb->deimage(SAVE_DISABLED_ICON);

	_print_tb->tooltip("Print (Ctrl+P)");
	_print_tb->callback((Fl_Callback *)print_cb, this);
	_print_tb->image(PRINT_ICON);
	_print_tb->deimage(PRINT_DISABLED_ICON);

	for (int i = 0; i < NUM_FORMATS; i++) {
		_format->add(format_name((Tilemap_Format)i));
	}
	_format->value(Config::format());
	_format->callback((Fl_Callback *)format_tb_cb, this);

	_tilemap_name->callback((Fl_Callback *)open_cb, this);

	_undo_tb->tooltip("Undo (Ctrl+Z)");
	_undo_tb->callback((Fl_Callback *)undo_cb, this);
	_undo_tb->image(UNDO_ICON);
	_undo_tb->deimage(UNDO_DISABLED_ICON);

	_redo_tb->tooltip("Redo (Ctrl+Y)");
	_redo_tb->callback((Fl_Callback *)redo_cb, this);
	_redo_tb->image(REDO_ICON);
	_redo_tb->deimage(REDO_DISABLED_ICON);

	_tilemap_width->default_value(GAME_BOY_WIDTH);
	_tilemap_width->range(1, 999);
	_tilemap_width->callback((Fl_Callback *)tilemap_width_tb_cb, this);

	_resize_tb->tooltip("Resize... (Ctrl+E)");
	_resize_tb->callback((Fl_Callback *)resize_cb, this);
	_resize_tb->image(RESIZE_ICON);
	_resize_tb->deimage(RESIZE_DISABLED_ICON);

	_load_tb->tooltip("Load Tileset... (Ctrl+T)");
	_load_tb->callback((Fl_Callback *)load_tileset_cb, this);
	_load_tb->image(LOAD_ICON);

	_add_tb->tooltip("Add Tileset... (Ctrl+A)");
	_add_tb->callback((Fl_Callback *)add_tileset_cb, this);
	_add_tb->image(ADD_ICON);

	_reload_tb->tooltip("Reload Tilesets... (Ctrl+R)");
	_reload_tb->callback((Fl_Callback *)reload_tilesets_cb, this);
	_reload_tb->image(RELOAD_ICON);
	_reload_tb->deimage(RELOAD_DISABLED_ICON);

	_image_to_tiles_tb->tooltip("Image to Tiles... (Ctrl+I)");
	_image_to_tiles_tb->callback((Fl_Callback *)image_to_tiles_cb, this);
	_image_to_tiles_tb->image(INPUT_ICON);

	_tileset_name->callback((Fl_Callback *)load_tileset_cb, this);

	select_tile_cb(_tile_buttons[0x00], this);

	_color->default_value(0);
	_color->range(0, NUM_SGB_COLORS - 1);

	_show_attributes->value(Config::attributes());
	_show_attributes->callback((Fl_Callback *)show_attributes_tb_cb, this);

	_flip_heading->box(FL_FLAT_BOX);
	_flip_heading->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

	// Configure dialogs

	_tilemap_open_chooser->title("Open Tilemap");
	_tilemap_open_chooser->filter("Tilemap Files\t*.{tilemap,rle,bin,map,kmp}\n");

	_tilemap_save_chooser->title("Save Tilemap");
	_tilemap_save_chooser->filter("Tilemap Files\t*.{tilemap,rle,bin,map,kmp}\n");
	_tilemap_save_chooser->preset_file("NewTilemap.tilemap");
	_tilemap_save_chooser->options(Fl_Native_File_Chooser::Option::SAVEAS_CONFIRM);

	_tileset_load_chooser->title("Open Tileset");
	_tileset_load_chooser->filter("Tileset Files\t*.{png,bmp,1bpp,2bpp,1bpp.lz,2bpp.lz}\n");

	_png_chooser->title("Print Screenshot");
	_png_chooser->filter("PNG Files\t*.png\n");
	_png_chooser->preset_file("screenshot.png");
	_png_chooser->options(Fl_Native_File_Chooser::Option::SAVEAS_CONFIRM);

	_error_dialog->width_range(280, 700);
	_warning_dialog->width_range(280, 700);
	_success_dialog->width_range(280, 700);
	_unsaved_dialog->width_range(280, 700);

	_new_tilemap_dialog->tilemap_width(GAME_BOY_WIDTH);
	_new_tilemap_dialog->tilemap_height(GAME_BOY_HEIGHT);

	std::string subject(PROGRAM_NAME " " PROGRAM_VERSION_STRING), message(
		"Copyright \xc2\xa9 " CURRENT_YEAR " " PROGRAM_AUTHOR ".\n"
		"\n"
		"Source code is available at:\n"
		"https://github.com/Rangi42/tilemap-studio\n"
		"\n"
		"Some icons by Yusuke Kamiyamane."
	);
	_about_dialog->subject(subject);
	_about_dialog->message(message);
	_about_dialog->width_range(280, 700);

	_help_window->content(
#include "help.html" // a C++11 raw string literal
		);

	update_recent_tilemaps();
	update_recent_tilesets();
	update_tilemap_metadata();
	update_tileset_metadata();
	update_active_controls();
}

Main_Window::~Main_Window() {
	delete _menu_bar; // includes menu items
	delete _status_bar; // includes status bar fields
	delete _main_group; // includes map and blocks
	delete _tilemap_dnd_receiver;
	delete _tileset_dnd_receiver;
	delete _tilemap_open_chooser;
	delete _tilemap_save_chooser;
	delete _tileset_load_chooser;
	delete _png_chooser;
	delete _error_dialog;
	delete _warning_dialog;
	delete _success_dialog;
	delete _unsaved_dialog;
	delete _about_dialog;
	delete _resize_dialog;
	delete _image_to_tiles_dialog;
	delete _help_window;
}

void Main_Window::show() {
	Fl_Double_Window::show();
#ifdef _WIN32
	// Fix for 16x16 icon from <http://www.fltk.org/str.php?L925>
	HWND hwnd = fl_xid(this);
	HANDLE big_icon = LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
		GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXICON), 0);
	SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
	HANDLE small_icon = LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CXSMICON), 0);
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
#else
	// Fix for X11 icon alpha mask <https://www.mail-archive.com/fltk@easysw.com/msg02863.html>
	XWMHints *hints = XGetWMHints(fl_display, fl_xid(this));
	hints->flags |= IconMaskHint;
	hints->icon_mask = _icon_mask;
	XSetWMHints(fl_display, fl_xid(this), hints);
	XFree(hints);
#endif
}

int Main_Window::handle(int event) {
	switch (event) {
	case FL_FOCUS:
	case FL_UNFOCUS:
		return 1;
	default:
		return Fl_Double_Window::handle(event);
	}
}

void Main_Window::store_recent_tilemap() {
	std::string last(_tilemap_file);
	for (int i = 0; i < NUM_RECENT; i++) {
		if (_recent_tilemaps[i] == _tilemap_file) {
			_recent_tilemaps[i] = last;
			break;
		}
		std::swap(last, _recent_tilemaps[i]);
	}
	update_recent_tilemaps();
}

void Main_Window::update_recent_tilemaps() {
	int last = -1;
	for (int i = 0; i < NUM_RECENT; i++) {
		if (_recent_tilemaps[i].empty()) {
			_recent_tilemap_mis[i]->label("");
			_recent_tilemap_mis[i]->hide();
		}
		else {
			const char *basename = fl_filename_name(_recent_tilemaps[i].c_str());
			_recent_tilemap_mis[i]->label(basename);
			_recent_tilemap_mis[i]->show();
			last = i;
		}
		_recent_tilemap_mis[i]->flags &= ~FL_MENU_DIVIDER;
	}
	_recent_tilemap_mis[last]->flags |= FL_MENU_DIVIDER;
}

void Main_Window::store_recent_tileset() {
	const std::string &tileset_file = _tileset_files.back();
	std::string last(tileset_file);
	for (int i = 0; i < NUM_RECENT; i++) {
		if (_recent_tilesets[i] == tileset_file) {
			_recent_tilesets[i] = last;
			break;
		}
		std::swap(last, _recent_tilesets[i]);
	}
	update_recent_tilesets();
}

void Main_Window::update_recent_tilesets() {
	int last = -1;
	for (int i = 0; i < NUM_RECENT; i++) {
		if (_recent_tilesets[i].empty()) {
			_recent_tileset_mis[i]->label("");
			_recent_tileset_mis[i]->hide();
		}
		else {
			const char *basename = fl_filename_name(_recent_tilesets[i].c_str());
			_recent_tileset_mis[i]->label(basename);
			_recent_tileset_mis[i]->show();
			last = i;
		}
		_recent_tileset_mis[i]->flags &= ~FL_MENU_DIVIDER;
	}
	_recent_tileset_mis[last]->flags |= FL_MENU_DIVIDER;
}

void Main_Window::update_status(Tile_Tessera *tt) {
	if (!_tilemap.size()) {
		_tilemap_dimensions->label("");
		_hover_id->label("");
		_hover_xy->label("");
		_hover_landmark->label("");
		_status_bar->redraw();
		return;
	}
	char buffer[64] = {};
#ifdef __GNUC__
	sprintf(buffer, "Tilemap: %zu x %zu", _tilemap.width(), _tilemap.height());
#else
	sprintf(buffer, "Tilemap: %u x %u", (uint32_t)_tilemap.width(), (uint32_t)_tilemap.height());
#endif
	_tilemap_dimensions->copy_label(buffer);
	if (!tt) {
		_hover_id->label("");
		_hover_xy->label("");
		_hover_landmark->label("");
		_status_bar->redraw();
		return;
	}
	sprintf(buffer, "ID: $%02X", tt->id());
	_hover_id->copy_label(buffer);
#ifdef __GNUC__
	sprintf(buffer, "X/Y (%zu, %zu)", tt->col(), tt->row());
#else
	sprintf(buffer, "X/Y (%u, %u)", (uint32_t)tt->col(), (uint32_t)tt->row());
#endif
	_hover_xy->copy_label(buffer);
	if (_tilemap.width() == GAME_BOY_WIDTH && _tilemap.height() == GAME_BOY_HEIGHT &&
		(Config::format() == Tilemap_Format::FF_END || Config::format() == Tilemap_Format::RLE_NYBBLES ||
		Config::format() == Tilemap_Format::XY_FLIP)) {
		size_t lx = tt->col() * TILE_SIZE + TILE_SIZE / 2, ly = tt->row() * TILE_SIZE + TILE_SIZE / 2; // center of tile
#ifdef __GNUC__
		sprintf(buffer, "Landmark (%zu, %zu)", lx, ly);
#else
		sprintf(buffer, "Landmark (%u, %u)", (uint32_t)lx, (uint32_t)ly);
#endif
		_hover_landmark->copy_label(buffer);
	}
	else {
		_hover_landmark->label("");
	}
}

void Main_Window::update_tilemap_metadata() {
	if (_tilemap.size()) {
		if (_tilemap_file.empty()) {
			_tilemap_name->label(NEW_TILEMAP_NAME);
		}
		else {
			const char *basename = fl_filename_name(_tilemap_file.c_str());
			_tilemap_name->label(basename);
		}
		const char *name = format_name(Config::format());
		_tilemap_format->label(name);
	}
	else {
		_tilemap_name->label("No file selected");
		_tilemap_format->label("");
	}
}

void Main_Window::update_tileset_metadata() {
	if (!_tilesets.empty()) {
		if (_tilesets.size() == 1) {
			std::string &f = _tileset_files.front();
			const char *basename = fl_filename_name(f.c_str());
			_tileset_name->label(basename);
		}
		else {
			char buffer[FL_PATH_MAX] = {};
#ifdef __GNUC__
			sprintf(buffer, "%zu files", _tilesets.size());
#else
			sprintf(buffer, "%u files", _tilesets.size());
#endif
			_tileset_name->copy_label(buffer);
		}
	}
	else {
		_tileset_name->label("No files selected");
	}
}

void Main_Window::update_active_controls() {
	if (_tilemap.size()) {
		_close_mi->activate();
		_save_mi->activate();
		_save_tb->activate();
		_save_as_mi->activate();
		_print_mi->activate();
		_print_tb->activate();
		if (_tilemap.can_undo()) {
			_undo_mi->activate();
			_undo_tb->activate();
		}
		else {
			_undo_mi->deactivate();
			_undo_tb->deactivate();
		}
		if (_tilemap.can_redo()) {
			_redo_mi->activate();
			_redo_tb->activate();
		}
		else {
			_redo_mi->deactivate();
			_redo_tb->deactivate();
		}
		_resize_mi->activate();
		_resize_tb->activate();
	}
	else {
		_close_mi->deactivate();
		_save_mi->deactivate();
		_save_tb->deactivate();
		_save_as_mi->deactivate();
		_print_mi->deactivate();
		_print_tb->deactivate();
		_undo_mi->deactivate();
		_undo_tb->deactivate();
		_redo_mi->deactivate();
		_redo_tb->deactivate();
		_resize_mi->deactivate();
		_resize_tb->deactivate();
	}
	if (!_tilesets.empty()) {
		_reload_tilesets_mi->activate();
		_reload_tb->activate();
		_unload_tilesets_mi->activate();
	}
	else {
		_reload_tilesets_mi->deactivate();
		_reload_tb->deactivate();
		_unload_tilesets_mi->deactivate();
	}
	if (Config::format() == Tilemap_Format::TILE_ATTR) {
		_color->activate();
		_show_attributes->activate();
		_show_attributes_mi->activate();
	}
	else {
		_color->default_value(0);
		_color->deactivate();
		_show_attributes->deactivate();
		_show_attributes_mi->deactivate();
	}
	if (Config::format() == Tilemap_Format::XY_FLIP || Config::format() == Tilemap_Format::TILE_ATTR) {
		_flip_heading->activate();
		_x_flip->activate();
		_y_flip->activate();
	}
	else {
		_flip_heading->deactivate();
		_x_flip->clear();
		_x_flip->deactivate();
		_y_flip->clear();
		_y_flip->deactivate();
	}
	int n = format_tileset_size(Config::format());
	for (int i = 0; i < n; i++) {
		_tile_buttons[i]->activate();
	}
	for (int i = n; i < NUM_TILES; i++) {
		_tile_buttons[i]->deactivate();
	}
	if (_selected->id() >= n) {
		select_tile_cb(_tile_buttons[0x00], this);
	}
}

void Main_Window::resize_tilemap() {
	size_t w = _resize_dialog->tilemap_width(), h = _resize_dialog->tilemap_height();
	size_t n = w * h;
	if (_tilemap.size() == n) { return; }

	_tilemap.resize(w, h, _resize_dialog->horizontal_anchor(), _resize_dialog->vertical_anchor());

	while (_tilemap_scroll->children() > 2) { // keep scrollbars
		_tilemap_scroll->remove(0);
	}
	for (size_t i = 0; i < n; i++) {
		Tile_Tessera *tt = _tilemap.tile(i);
		tt->callback((Fl_Callback *)change_tile_cb, this);
		_tilemap_scroll->add(tt);
	}

	_tilemap_width->default_value(w);
	tilemap_width_tb_cb(NULL, this);
	update_status(NULL);
	update_active_controls();
	redraw();
}

void Main_Window::flood_fill(Tile_Tessera *tt) {
	Tile_State fs = tt->state();
	Tile_State ts(_selected->id(), x_flip(), y_flip(), sgb_color());
	if (fs == ts) { return; }
	std::queue<size_t> queue;
	size_t w = _tilemap.width(), h = _tilemap.height(), n = _tilemap.size();
	size_t row = tt->row(), col = tt->col();
	size_t i = row * w + col;
	queue.push(i);
	while (!queue.empty()) {
		size_t j = queue.front();
		queue.pop();
		if (j >= n) { continue; }
		Tile_Tessera *ff = _tilemap.tile(j);
		if (ff->state() != fs) { continue; }
		ff->state(ts); // fill
		size_t r = ff->row(), c = ff->col();
		if (c > 0) { queue.push(j-1); } // left
		if (c < w - 1) { queue.push(j+1); } // right
		if (r > 0) { queue.push(j-w); } // up
		if (r < h - 1) { queue.push(j+w); } // down
	}
}

void Main_Window::substitute_tile(Tile_Tessera *tt) {
	Tile_State fs = tt->state();
	Tile_State ts(_selected->id(), x_flip(), y_flip(), sgb_color());
	size_t n = _tilemap.size();
	for (size_t i = 0; i < n; i++) {
		Tile_Tessera *ff = _tilemap.tile(i);
		if (ff->state() == fs) {
			ff->state(ts);
		}
	}
}

void Main_Window::swap_tiles(Tile_Tessera *tt) {
	Tile_State s1 = tt->state();
	Tile_State s2(_selected->id(), x_flip(), y_flip(), sgb_color());
	if (s1 == s2) { return; }
	size_t n = _tilemap.size();
	for (size_t i = 0; i < n; i++) {
		Tile_Tessera *ff = _tilemap.tile(i);
		if (ff->state() == s1) {
			ff->state(s2);
		}
		else if (ff->state() == s2) {
			ff->state(s1);
		}
	}
}

void Main_Window::open_tilemap(const char *filename, size_t width, size_t height) {
	close_cb(NULL, this);

	const char *basename;

	if (filename) {
		_tilemap_file = filename;
		basename = fl_filename_name(filename);

		Tilemap::Result r = _tilemap.read_tiles(filename);
		if (r) {
			_tilemap.clear();
			std::string msg = "Error reading ";
			msg = msg + basename + "!\n\n" + Tilemap::error_message(r);
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return;
		}
	}
	else {
		_tilemap_file = "";
		basename = NEW_TILEMAP_NAME;

		_tilemap.new_tiles(width, height);
	}

	size_t n = _tilemap.size();
	for (size_t i = 0; i < n; i++) {
		Tile_Tessera *tt = _tilemap.tile(i);
		tt->callback((Fl_Callback *)change_tile_cb, this);
		_tilemap_scroll->add(tt);
	}

	_tilemap_width->default_value(_tilemap.width());
	tilemap_width_tb_cb(NULL, this);

	// set filenames
	char buffer[FL_PATH_MAX] = {};
	sprintf(buffer, PROGRAM_NAME " - %s", basename);
	copy_label(buffer);
	sprintf(buffer, "%s", basename);
	fl_filename_setext(buffer, sizeof(buffer), ".png");
	_png_chooser->preset_file(buffer);

	store_recent_tilemap();
	update_tilemap_metadata();
	update_status(NULL);
	update_active_controls();
	redraw();
}

void Main_Window::open_recent_tilemap(int n) {
	if (n < 0 || n >= NUM_RECENT || _recent_tilemaps[n].empty()) {
		return;
	}

	if (unsaved()) {
		std::string msg = modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Open another tilemap anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return; }
	}

	const char *filename = _recent_tilemaps[n].c_str();
	_tilemap.modified(false);
	open_tilemap(filename);
}

bool Main_Window::save_tilemap(bool force) {
	const char *filename = _tilemap_file.c_str();
	const char *basename = fl_filename_name(filename);

	if (!_tilemap.can_write_tiles()) {
		std::string msg = "Cannot format the tilemap as ";
		msg = msg + format_name(Config::format()) + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	if (_tilemap.modified() || force) {
		if (!_tilemap.write_tiles(filename)) {
			std::string msg = "Could not write to ";
			msg = msg + basename + "!";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}
	}
	_tilemap.modified(false);

	char buffer[FL_PATH_MAX] = {};
	sprintf(buffer, PROGRAM_NAME " - %s", basename);
	copy_label(buffer);
	update_tilemap_metadata();

	strcpy(buffer, basename);
	fl_filename_setext(buffer, FL_PATH_MAX, ".png");
	_png_chooser->preset_file(buffer);

	if (force) {
		store_recent_tilemap();
	}

	std::string msg = "Saved ";
	msg = msg + basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	return true;
}

void Main_Window::add_tileset(const char *filename, int start, int offset, int length) {
	const char *basename = fl_filename_name(filename);
	Tileset tileset(start, offset, length);
	Tileset::Result result = tileset.read_tiles(filename);
	if (result) {
		std::string msg = "Error reading ";
		msg = msg + basename + "!\n\n" + Tileset::error_message(result);
		_error_dialog->message(msg);
		_error_dialog->show(this);
		tileset.clear();
		return;
	}
	_tilesets.push_back(tileset);
	_tileset_files.push_back(filename);
	store_recent_tileset();
	update_tileset_metadata();
	update_active_controls();
	redraw();
}

void Main_Window::load_recent_tileset(int n) {
	if (n < 0 || n >= NUM_RECENT || _recent_tilesets[n].empty()) {
		return;
	}

	if (_tilesets.size() > 1) {
		std::string msg = "You have added ";
		msg = msg + std::to_string(_tilesets.size()) + " tilesets!\n\n"
			"Load another tileset anyway?";
		_unsaved_dialog->message(msg);
		_unsaved_dialog->show(this);
		if (_unsaved_dialog->canceled()) { return; }
	}

	const char *filename = _recent_tilesets[n].c_str();
	load_tileset(filename);
}

void Main_Window::image_to_tiles() {
	const char *image_filename = _image_to_tiles_dialog->image_filename();
	const char *image_basename = fl_filename_name(image_filename);

	Fl_RGB_Image *img = NULL;
	if (ends_with(image_filename, ".bmp") || ends_with(image_filename, ".BMP")) {
		img = new Fl_BMP_Image(image_filename);
	}
	else {
		img = new Fl_PNG_Image(image_filename);
	}
	if (!img || img->fail()) {
		delete img;
		std::string msg = "Could not convert ";
		msg = msg + image_basename + "!\n\nCannot open file.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}

	size_t n = 0;
	Tile *tiles = get_image_tiles(img, n);
	delete img;
	if (!tiles || !n) {
		delete [] tiles;
		std::string msg = "Could not convert ";
		msg = msg + image_basename + "!\n\nImage dimensions do not fit the tile grid.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}

	std::vector<Tile_Tessera *> tilemap;
	std::vector<size_t> tileset;

	Tilemap_Format fmt = _image_to_tiles_dialog->format();
	uint8_t start_id = _image_to_tiles_dialog->start_id();
	bool use_7f = _image_to_tiles_dialog->use_7f();
	for (size_t i = 0; i < n; i++) {
		Tile &tile = tiles[i];
		if (use_7f && is_blank_tile(tile)) {
			tilemap.push_back(new Tile_Tessera(0, 0, 0, 0, SPACE_TILE_ID));
			continue;
		}
		size_t ti = 0, nt = tileset.size();
		bool x_flip = false, y_flip = false;
		for (; ti < nt; ti++) {
			size_t j = tileset[ti];
			if (are_identical_tiles(tile, tiles[j], fmt, x_flip, y_flip)) {
				break;
			}
		}
		if (ti == nt) {
			if (nt + (size_t)start_id > 0xFF) {
				for (Tile_Tessera *tt : tilemap) {
					delete tt;
				}
				delete [] tiles;
				std::string msg = "Could not convert ";
				msg = msg + image_basename + "!\n\nToo many unique tiles.";
				_error_dialog->message(msg);
				_error_dialog->show(this);
				return;
			}
			tileset.push_back(i);
		}
		uint8_t id = start_id + (uint8_t)ti;
		Tile_Tessera *tt = new Tile_Tessera(0, 0, 0, 0, id, x_flip, y_flip);
		tilemap.push_back(tt);
	}

	const char *tilemap_filename = _image_to_tiles_dialog->tilemap_filename();
	const char *tilemap_basename = fl_filename_name(tilemap_filename);
	if (!Tilemap::write_tiles(tilemap_filename, tilemap, fmt)) {
		for (Tile_Tessera *tt : tilemap) {
			delete tt;
		}
		delete [] tiles;
		std::string msg = "Could not write to ";
		msg = msg + tilemap_basename + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}


	for (Tile_Tessera *tt : tilemap) {
		delete tt;
	}

	const char *tileset_filename = _image_to_tiles_dialog->tileset_filename();
	const char *tileset_basename = fl_filename_name(tileset_filename);
	int nt = (int)tileset.size();
	int tw = MIN(nt, 16);
	int th = (nt + tw - 1) / tw;

	Fl_Image_Surface *surface = new Fl_Image_Surface(tw * TILE_SIZE, th * TILE_SIZE);
	surface->set_current();

	fl_rectf(0, 0, tw * TILE_SIZE, th * TILE_SIZE, FL_WHITE);
	for (int i = 0; i < nt; i++) {
		size_t ti = tileset[i];
		Tile &tile = tiles[ti];
		int x = i % tw, y = i / tw;
		for (int ty = 0; ty < TILE_SIZE; ty++) {
			for (int tx = 0; tx < TILE_SIZE; tx++) {
				fl_color(tile[ty * TILE_SIZE + tx]);
				fl_point(x * TILE_SIZE + tx, y * TILE_SIZE + ty);
			}
		}
	}

	Fl_RGB_Image *timg = surface->image();
	delete surface;
	Fl_Display_Device::display_device()->set_current();

	Image::Result result = Image::write_image(tileset_filename, timg);
	delete timg;
	if (result) {
		delete [] tiles;
		std::string msg = "Could not write to ";
		msg = msg + tileset_basename + "!\n\n" + Image::error_message(result);
		_error_dialog->message(msg);
		_error_dialog->show(this);
	}

	delete [] tiles;

	std::string msg = "Converted ";
	msg = msg + image_basename + " to\n" + tilemap_basename + " and " + tileset_basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	_format->value((int)fmt);
	format_tb_cb(NULL, this);
	open_tilemap(tilemap_filename);
	unload_tilesets_cb(NULL, this);
	add_tileset(tileset_filename, start_id);
}

void Main_Window::drag_and_drop_tilemap_cb(DnD_Receiver *dndr, Main_Window *mw) {
	Fl_Window *top = Fl::modal();
	if (top && top != mw) { return; }
	std::string filename = dndr->text().substr(0, dndr->text().find('\n'));
	mw->open_tilemap(filename.c_str());
}

void Main_Window::drag_and_drop_tileset_cb(DnD_Receiver *dndr, Main_Window *mw) {
	Fl_Window *top = Fl::modal();
	if (top && top != mw) { return; }
	std::string filename = dndr->text().substr(0, dndr->text().find('\n'));
	mw->load_tileset(filename.c_str());
}

void Main_Window::new_cb(Fl_Widget *, Main_Window *mw) {
	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Create a new tilemap anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	mw->_new_tilemap_dialog->show(mw);
	if (mw->_new_tilemap_dialog->canceled()) { return; }

	mw->_tilemap.modified(false);
	mw->new_tilemap(mw->_new_tilemap_dialog->tilemap_width(), mw->_new_tilemap_dialog->tilemap_height());
}

void Main_Window::open_cb(Fl_Widget *, Main_Window *mw) {
	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Open another tilemap anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	int status = mw->_tilemap_open_chooser->show();
	if (status == 1) { return; }

	const char *filename = mw->_tilemap_open_chooser->filename();
	const char *basename = fl_filename_name(filename);
	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_tilemap_open_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->_tilemap.modified(false);
	mw->open_tilemap(filename);
}

void Main_Window::open_recent_tilemap_cb(Fl_Menu_ *m, Main_Window *mw) {
	int first_recent_i = m->find_index((Fl_Callback *)open_recent_tilemap_cb);
	int i = m->find_index(m->mvalue()) - first_recent_i;
	mw->open_recent_tilemap(i);
}

void Main_Window::clear_recent_tilemaps_cb(Fl_Menu_ *, Main_Window *mw) {
	for (int i = 0; i < NUM_RECENT; i++) {
		mw->_recent_tilemaps[i].clear();
		mw->_recent_tilemap_mis[i]->hide();
	}
}

void Main_Window::load_recent_tileset_cb(Fl_Menu_ *m, Main_Window *mw) {
	int first_recent_i = m->find_index((Fl_Callback *)load_recent_tileset_cb);
	int i = m->find_index(m->mvalue()) - first_recent_i;
	mw->load_recent_tileset(i);
}

void Main_Window::clear_recent_tilesets_cb(Fl_Menu_ *, Main_Window *mw) {
	for (int i = 0; i < NUM_RECENT; i++) {
		mw->_recent_tilesets[i].clear();
		mw->_recent_tileset_mis[i]->hide();
	}
}

void Main_Window::close_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }

	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Close it anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	mw->label(PROGRAM_NAME);
	mw->_tilemap.clear();
	mw->_tilemap_scroll->clear();
	mw->_tilemap_scroll->scroll_to(0, 0);
	mw->_tilemap_scroll->contents(0, 0);
	mw->init_sizes();
	mw->_tilemap_file.clear();
	mw->update_tilemap_metadata();
	mw->update_status(NULL);
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::save_cb(Fl_Widget *w, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }
	if (mw->_tilemap_file.empty()) {
		save_as_cb(w, mw);
	}
	else {
		mw->save_tilemap(false);
	}
}

void Main_Window::save_as_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }

	int status = mw->_tilemap_save_chooser->show();
	if (status == 1) { return; }

	Tilemap_Format fmt = Config::format();

	char filename[FL_PATH_MAX] = {};
	const char *ext = format_extension(fmt);
	add_dot_ext(mw->_tilemap_save_chooser->filename(), ext, filename);
	const char *basename = fl_filename_name(filename);

	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_tilemap_save_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->_tilemap_file = filename ? filename : "";
	mw->save_tilemap(true);
}

void Main_Window::load_tileset_cb(Fl_Widget *, Main_Window *mw) {
	if (mw->_tilesets.size() > 1) {
		std::string msg = "You have added ";
		msg = msg + std::to_string(mw->_tilesets.size()) + " tilesets!\n\n"
			"Load another tileset anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	int status = mw->_tileset_load_chooser->show();
	if (status == 1) { return; }

	const char *filename = mw->_tileset_load_chooser->filename();
	const char *basename = fl_filename_name(filename);
	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_tileset_load_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->load_tileset(filename);
}

void Main_Window::add_tileset_cb(Fl_Widget *, Main_Window *mw) {
	int status = mw->_tileset_load_chooser->show();
	if (status == 1) { return; }

	const char *filename = mw->_tileset_load_chooser->filename();
	const char *basename = fl_filename_name(filename);
	if (status == -1) {
		std::string msg = "Could not open ";
		msg = msg + basename + "!\n\n" + mw->_tileset_load_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	mw->_add_tileset_dialog->limit_tileset_options(filename);
	mw->_add_tileset_dialog->start_id(mw->_selected ? mw->_selected->id() : 0x00);
	mw->_add_tileset_dialog->show(mw);
	if (mw->_add_tileset_dialog->canceled()) { return; }
	int start = mw->_add_tileset_dialog->start_id();
	int offset = mw->_add_tileset_dialog->offset();
	int length = mw->_add_tileset_dialog->length();
	mw->add_tileset(filename, start, offset, length);
}

void Main_Window::reload_tilesets_cb(Fl_Widget *, Main_Window *mw) {
	if (mw->_tilesets.empty()) { return; }

	std::vector<Tileset> tilesets(mw->_tilesets);
	std::vector<std::string> tileset_files(mw->_tileset_files);
	mw->_tilesets.clear();
	mw->_tileset_files.clear();
	size_t n = tilesets.size();
	for (size_t i = 0; i < n; i++) {
		const char *filename = tileset_files[i].c_str();
		Tileset &t = tilesets[i];
		mw->add_tileset(filename, t.start_id(), t.offset(), t.length());
	}
}

void Main_Window::unload_tilesets_cb(Fl_Widget *w, Main_Window *mw) {
	if (mw->_tilesets.empty()) { return; }

	if (w && mw->_tilesets.size() > 1) {
		std::string msg = "You have added ";
		msg = msg + std::to_string(mw->_tilesets.size()) + " tilesets!\n\n"
			"Unload them anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	for (Tileset &t : mw->_tilesets) {
		t.clear();
	}
	mw->_tilesets.clear();
	mw->_tileset_files.clear();
	mw->update_tileset_metadata();
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::print_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }

	int status = mw->_png_chooser->show();
	if (status == 1) { return; }

	char filename[FL_PATH_MAX] = {};
	add_dot_ext(mw->_png_chooser->filename(), ".png", filename);
	const char *basename = fl_filename_name(filename);

	if (status == -1) {
		std::string msg = "Could not print to ";
		msg = msg + basename + "!\n\n" + mw->_png_chooser->errmsg();
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
		return;
	}

	Fl_RGB_Image *img = mw->_tilemap.print_tilemap();
	Image::Result result = Image::write_image(filename, img);
	delete img;
	if (result) {
		std::string msg = "Could not print to ";
		msg = msg + basename + "!\n\n" + Image::error_message(result);
		mw->_error_dialog->message(msg);
		mw->_error_dialog->show(mw);
	}
	else {
		std::string msg = "Printed ";
		msg = msg + basename + "!";
		mw->_success_dialog->message(msg);
		mw->_success_dialog->show(mw);
	}
}

void Main_Window::exit_cb(Fl_Widget *, Main_Window *mw) {
	// Override default behavior of Esc to close main window
	if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) { return; }

	if (mw->unsaved()) {
		std::string msg = mw->modified_filename();
		msg = msg + " has unsaved changes!\n\n"
			"Exit anyway?";
		mw->_unsaved_dialog->message(msg);
		mw->_unsaved_dialog->show(mw);
		if (mw->_unsaved_dialog->canceled()) { return; }
	}

	// Save global config
	Preferences::set("theme", OS::current_theme());
	Preferences::set("x", mw->x());
	Preferences::set("y", mw->y());
	Preferences::set("w", mw->w());
	Preferences::set("h", mw->h());
	Preferences::set("grid", Config::grid());
	Preferences::set("rainbow", Config::rainbow_tiles());
	Preferences::set("colors", Config::attributes());
	Preferences::set("grid", Config::grid());
	Preferences::set("format", Config::format());
	Preferences::set("attributes", Config::attributes());
	for (int i = 0; i < NUM_RECENT; i++) {
		Preferences::set_string(Fl_Preferences::Name("recent-map%d", i), mw->_recent_tilemaps[i]);
	}
	for (int i = 0; i < NUM_RECENT; i++) {
		Preferences::set_string(Fl_Preferences::Name("recent-set%d", i), mw->_recent_tilesets[i]);
	}
	if (mw->_resize_dialog->initialized()) {
		Preferences::set("resize-anchor", mw->_resize_dialog->anchor());
	}

	exit(EXIT_SUCCESS);
}

void Main_Window::undo_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }
	mw->_tilemap.undo();
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::redo_cb(Fl_Widget *, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }
	mw->_tilemap.redo();
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::classic_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_classic_theme();
	mw->_classic_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::aero_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_aero_theme();
	mw->_aero_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::metro_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_metro_theme();
	mw->_metro_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::aqua_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_aqua_theme();
	mw->_aqua_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::greybird_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_greybird_theme();
	mw->_greybird_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::metal_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_metal_theme();
	mw->_metal_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::blue_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_blue_theme();
	mw->_blue_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::olive_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_olive_theme();
	mw->_olive_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::rose_gold_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_rose_gold_theme();
	mw->_rose_gold_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::dark_theme_cb(Fl_Menu_ *, Main_Window *mw) {
	OS::use_dark_theme();
	mw->_dark_theme_mi->setonly();
	for (Tileset &t : mw->_tilesets) {
		t.refresh_inactive_image();
	}
	mw->redraw();
}

void Main_Window::grid_cb(Fl_Menu_ *m, Main_Window *mw) {
	Config::grid(!!m->mvalue()->value());
	mw->redraw();
}

void Main_Window::rainbow_tiles_cb(Fl_Menu_ *m, Main_Window *mw) {
	Config::rainbow_tiles(!!m->mvalue()->value());
	mw->redraw();
}

void Main_Window::tilemap_width_cb(Fl_Menu_ *, Main_Window *mw) {
	mw->_tilemap_width_dialog->tilemap_width((size_t)mw->_tilemap_width->value());
	mw->_tilemap_width_dialog->show(mw);
	if (mw->_tilemap_width_dialog->canceled()) { return; }
	mw->_tilemap_width->default_value(mw->_tilemap_width_dialog->tilemap_width());
	tilemap_width_tb_cb(mw->_tilemap_width, mw);
}

void Main_Window::resize_cb(Fl_Menu_ *, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }
	mw->_resize_dialog->tilemap_size(mw->_tilemap.width(), mw->_tilemap.height());
	mw->_resize_dialog->show(mw);
	if (mw->_resize_dialog->canceled()) { return; }
	mw->resize_tilemap();
}

void Main_Window::image_to_tiles_cb(Fl_Widget *, Main_Window *mw) {
	mw->_image_to_tiles_dialog->format(Config::format());
	mw->_image_to_tiles_dialog->show(mw);
	if (mw->_image_to_tiles_dialog->canceled()) { return; }
	mw->image_to_tiles();
}

void Main_Window::plain_format_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::format(Tilemap_Format::PLAIN);
	mw->_format->value(Config::format());
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::sgb_border_format_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::format(Tilemap_Format::TILE_ATTR);
	mw->_format->value(Config::format());
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::rby_town_map_format_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::format(Tilemap_Format::RLE_NYBBLES);
	mw->_format->value(Config::format());
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::gsc_town_map_format_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::format(Tilemap_Format::FF_END);
	mw->_format->value(Config::format());
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::pc_town_map_format_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::format(Tilemap_Format::XY_FLIP);
	mw->_format->value(Config::format());
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::pokegear_card_format_cb(Fl_Menu_ *, Main_Window *mw) {
	Config::format(Tilemap_Format::RLE_FF_END);
	mw->_format->value(Config::format());
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::show_attributes_cb(Fl_Menu_ *m, Main_Window *mw) {
	Config::attributes(!!m->mvalue()->value());
	mw->_show_attributes->value(Config::attributes());
	mw->redraw();
}

void Main_Window::help_cb(Fl_Widget *, Main_Window *mw) {
	mw->_help_window->show(mw);
}

void Main_Window::about_cb(Fl_Widget *, Main_Window *mw) {
	mw->_about_dialog->show(mw);
}

void Main_Window::format_tb_cb(Dropdown *, Main_Window *mw) {
	Config::format((Tilemap_Format)mw->_format->value());
	Fl_Menu_Item *menu_items[NUM_FORMATS] = {
		mw->_plain_format_mi,         // PLAIN
		mw->_sgb_border_format_mi,    // TILE_ATTR
		mw->_rby_town_map_format_mi,  // RLE_NYBBLES
		mw->_gsc_town_map_format_mi,  // FF_END
		mw->_pokegear_card_format_mi, // XY_FLIP
		mw->_pc_town_map_format_mi    // RLE_FF_END
	};
	menu_items[Config::format()]->setonly();
	mw->update_active_controls();
	mw->redraw();
}

void Main_Window::tilemap_width_tb_cb(OS_Spinner *, Main_Window *mw) {
	if (!mw->_tilemap.size()) { return; }
	mw->_tilemap.width((size_t)mw->_tilemap_width->value());
	int sx = mw->_tilemap_scroll->x() + Fl::box_dx(mw->_tilemap_scroll->box());
	int sy = mw->_tilemap_scroll->y() + Fl::box_dy(mw->_tilemap_scroll->box());
	mw->_tilemap_scroll->init_sizes();
	mw->_tilemap_scroll->contents((int)(mw->_tilemap.width() * TILE_SIZE_2X), (int)(mw->_tilemap.height() * TILE_SIZE_2X));
	mw->_tilemap_scroll->scroll_to(0, 0);
	mw->_tilemap.reposition_tiles(sx, sy);
	mw->_tilemap_scroll->redraw();
	mw->update_status(NULL);
}

void Main_Window::show_attributes_tb_cb(Toggle_Switch *, Main_Window *mw) {
	Config::attributes(!!mw->_show_attributes->value());
	if (Config::attributes()) {
		mw->_show_attributes_mi->set();
	}
	else {
		mw->_show_attributes_mi->clear();
	}
	mw->redraw();
}

void Main_Window::change_tile_cb(Tile_Tessera *tt, Main_Window *mw) {
	if (!mw->_map_editable) { return; }
	if (Fl::event_button() == FL_LEFT_MOUSE) {
		if (!mw->_selected) { return; }
		if (Fl::event_is_click()) {
			mw->_tilemap.remember();
			mw->update_active_controls();
		}
		if (Fl::event_shift()) {
			// Shift+left-click to flood fill
			mw->flood_fill(tt);
			mw->_tilemap_scroll->redraw();
			mw->_tilemap.modified(true);
		}
		else if (Fl::event_ctrl()) {
			// Ctrl+left-click to replace
			mw->substitute_tile(tt);
			mw->_tilemap_scroll->redraw();
			mw->_tilemap.modified(true);
		}
		else if (Fl::event_alt()) {
			// Alt+click to swap
			mw->swap_tiles(tt);
			mw->_tilemap_scroll->redraw();
			mw->_tilemap.modified(true);
		}
		else {
			// Left-click/drag to edit
			tt->id(mw->_selected->id());
			tt->sgb_color(mw->sgb_color());
			tt->x_flip(mw->x_flip());
			tt->y_flip(mw->y_flip());
			tt->damage(1);
			mw->_tilemap.modified(true);
		}
	}
	else if (Fl::event_button() == FL_RIGHT_MOUSE) {
		// Right-click to select
		int color = tt->sgb_color();
		mw->_color->default_value(color > -1 ? color : 0);
		mw->_x_flip->value(tt->x_flip());
		mw->_y_flip->value(tt->y_flip());
		mw->_color->redraw();
		mw->_x_flip->redraw();
		mw->_y_flip->redraw();
		uint8_t id = tt->id();
		select_tile_cb(mw->_tile_buttons[id], mw);
	}
}

void Main_Window::select_tile_cb(Tile_Button *tb, Main_Window *mw) {
	tb->setonly();
	mw->_selected = tb;
	char buffer[32];
	sprintf(buffer, "Tile: $%02X", tb->id());
	mw->_tile_heading->copy_label(buffer);
	mw->_tile_heading->redraw();
}
