#include <string>
#include <vector>
#include <set>
#include <algorithm>

#pragma warning(push, 0)
#include <FL/Fl.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_BMP_Image.H>
#include <FL/Fl_Image_Surface.H>
#pragma warning(pop)

#include "utils.h"
#include "config.h"
#include "image.h"
#include "tilemap.h"
#include "tileset.h"
#include "tile.h"
#include "main-window.h"

typedef std::set<Fl_Color> Color_Set;

static bool build_tilemap(const Tile *tiles, size_t n,
	std::vector<Tile_Tessera *> &tilemap, std::vector<size_t> &tileset,
	Tilemap_Format fmt, uint16_t start_id, bool use_space, uint16_t space_id) {
	for (size_t i = 0; i < n; i++) {
		const Tile &tile = tiles[i];
		if (use_space && is_blank_tile(tile)) {
			tilemap.push_back(new Tile_Tessera(0, 0, 0, 0, space_id));
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
			size_t mn = (size_t)format_tileset_size(fmt);
			if (nt + (size_t)start_id > mn) {
				return false;
			}
			tileset.push_back(i);
		}
		uint16_t id = start_id + (uint16_t)ti;
		Tile_Tessera *tt = new Tile_Tessera(0, 0, 0, 0, id, x_flip, y_flip);
		tilemap.push_back(tt);
	}
	return true;
}

static Fl_RGB_Image *draw_tileset(const Tile *tiles, std::vector<size_t> &tileset) {
	int nt = (int)tileset.size();
	int tw = MIN(nt, TILES_PER_ROW);
	int th = (nt + tw - 1) / tw;

	Fl_Image_Surface *surface = new Fl_Image_Surface(tw * TILE_SIZE, th * TILE_SIZE);
	surface->set_current();

	fl_rectf(0, 0, tw * TILE_SIZE, th * TILE_SIZE, FL_WHITE);
	for (int i = 0; i < nt; i++) {
		size_t ti = tileset[i];
		const Tile &tile = tiles[ti];
		int x = i % tw, y = i / tw;
		for (int ty = 0; ty < TILE_SIZE; ty++) {
			for (int tx = 0; tx < TILE_SIZE; tx++) {
				fl_color(tile[ty * TILE_SIZE + tx]);
				fl_point(x * TILE_SIZE + tx, y * TILE_SIZE + ty);
			}
		}
	}

	Fl_RGB_Image *img = surface->image();
	delete surface;
	Fl_Display_Device::display_device()->set_current();

	return img;
}

void Main_Window::image_to_tiles() {
	// Open the input image

	const char *image_filename = _image_to_tiles_dialog->image_filename();
	const char *image_basename = fl_filename_name(image_filename);

	Fl_RGB_Image *img = NULL;
	if (ends_with(image_basename, ".bmp") || ends_with(image_basename, ".BMP")) {
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

	// Read the input image tiles

	size_t n = 0, w = 0;
	Tile *tiles = get_image_tiles(img, n, w);
	delete img;
	if (!tiles || !n) {
		delete [] tiles;
		std::string msg = "Could not convert ";
		msg = msg + image_basename + "!\n\nImage dimensions do not fit the "
			STRINGIFY(TILE_SIZE) "x" STRINGIFY(TILE_SIZE) " tile grid.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return;
	}

	// Build the palette

	Tilemap_Format fmt = _image_to_tiles_dialog->format();
	bool make_palette = _image_to_tiles_dialog->palette();

	if (make_palette) {
		// Algorithm ported from superfamiconv
		// <https://github.com/Optiroc/SuperFamiconv>

		size_t max_palettes = (size_t)format_palettes_size(fmt);
		size_t max_colors = (size_t)format_palette_size(fmt);

		// Get the color set of each tile
		std::vector<Color_Set> cs_tiles;
		size_t qi = 0;
		for (; qi < n; qi++) {
			const Tile &tile = tiles[qi];
			Color_Set s;
			for (int j = 0; j < NUM_TILE_PIXELS; j++) {
				s.insert(tile[j]);
			}
			if (s.size() > max_colors) {
				break;
			}
			cs_tiles.push_back(s);
		}

		// Check that all color sets fit within the color limit
		if (qi < n) {
			size_t qx = qi % w, qy = qi / w;
			delete [] tiles;
			std::string msg = "Could not convert ";
			msg = msg + image_basename + "!\n\nThe tile at (" +
				std::to_string(qx) + ", " + std::to_string(qy) +
				") has more than " + std::to_string(max_colors) + " colors.";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return;
		}

		// Remove duplicate color sets
		std::vector<Color_Set> cs_uniq(cs_tiles.size());
		auto cs_uniq_last = std::copy_if(cs_tiles.begin(), cs_tiles.end(), cs_uniq.begin(), [&](Color_Set &s) {
			return std::find(cs_uniq.begin(), cs_uniq.end(), s) == cs_uniq.end();
		});
		cs_uniq.resize(std::distance(cs_uniq.begin(), cs_uniq_last));

		// Remove color sets that are proper subsets of other color sets
		std::vector<Color_Set> cs_full(cs_uniq.size());
		auto cs_full_last = std::copy_if(cs_uniq.begin(), cs_uniq.end(), cs_full.begin(), [&](Color_Set &s) {
			for (Color_Set &c : cs_uniq) {
				if (s != c && std::includes(c.begin(), c.end(), s.begin(), s.end())) {
					return false;
				}
			}
			return true;
		});
		cs_full.resize(std::distance(cs_full.begin(), cs_full_last));

		// Sort color sets from fewest to most colors
		std::sort(cs_full.begin(), cs_full.end(), [](Color_Set &a, Color_Set &b) {
			return a.size() < b.size();
		});

		// Combine color sets as long as they fit within the color limit
		std::vector<Color_Set> cs_opt;
		for (Color_Set &s : cs_full) {
			int best_i = -1, i = 0;
			for (Color_Set &c : cs_opt) {
				Color_Set d;
				std::set_difference(s.begin(), s.end(), c.begin(), c.end(), std::inserter(d, d.begin()));
				if (c.size() + d.size() <= max_colors) {
					best_i = i;
				}
				++i;
			}
			if (best_i == -1) {
				cs_opt.push_back(s);
			}
			else {
				cs_opt[best_i].insert(s.begin(), s.end());
			}
		}

		// Check that the palettes fit within the palette limit
		if (cs_opt.size() > max_palettes) {
			delete [] tiles;
			std::string msg = "Could not convert ";
			msg = msg + image_basename + "!\n\nThe tiles need more than " +
				std::to_string(max_palettes) + " palettes.";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return;
		}

		// Sort color sets from most to fewest colors
		std::sort(cs_opt.begin(), cs_opt.end(), [](Color_Set &a, Color_Set &b) {
			return a.size() > b.size();
		});

		// TODO: Use the optimized palette
	}

	// Build the tilemap and tileset

	std::vector<Tile_Tessera *> tilemap;
	std::vector<size_t> tileset;

	uint16_t start_id = _image_to_tiles_dialog->start_id();
	bool use_space = _image_to_tiles_dialog->use_space();
	uint16_t space_id = _image_to_tiles_dialog->space_id();
	if (!build_tilemap(tiles, n, tilemap, tileset, fmt, start_id, use_space, space_id)) {
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

	// Get the output filenames

	const char *tileset_filename = _image_to_tiles_dialog->tileset_filename();
	const char *tilemap_filename = _image_to_tiles_dialog->tilemap_filename();
	const char *attrmap_filename = _image_to_tiles_dialog->attrmap_filename();
	const char *tileset_basename = fl_filename_name(tileset_filename);
	const char *tilemap_basename = fl_filename_name(tilemap_filename);

	// Create the tilemap file

	if (!Tilemap::write_tiles(tilemap_filename, attrmap_filename, tilemap, fmt)) {
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

	// Create the tileset file

	Fl_RGB_Image *timg = draw_tileset(tiles, tileset);
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

	// Alert the completed operation

	std::string msg = "Converted ";
	msg = msg + image_basename + " to\n" + tilemap_basename + " and " + tileset_basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	// Open the tilemap and load the tileset

	Config::format(fmt);
	update_active_controls();
	redraw();

	open_tilemap(tilemap_filename);
	unload_tilesets_cb(NULL, this);
	add_tileset(tileset_filename, start_id);
}
