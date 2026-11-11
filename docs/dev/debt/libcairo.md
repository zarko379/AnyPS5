# libcairo technical debt

### Functional

- [Cairo affine matrices](../../../core/libs/prx/libcairo/Matrix.cpp) - Ten pure affine operations implement the documented [Cairo matrix API](https://www.cairographics.org/manual/cairo-cairo-matrix-t.html), including multiplication with aliased outputs, singular inversion status and point/distance transforms. The 48-byte `cairo_matrix_t` layout and C signatures are public; the PS5 library revision and native behavior have not been measured. These functions do not require the other 73 pending surface, font or rendering exports.

### Functional image and surface exports

- [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp) implements 20 Cairo image/surface/stride exports with caller-owned and internally-owned pixel buffers, reference-counted surfaces, format and stride handling, device scales, subsurfaces and error surfaces. CPU image flush/mark-dirty require no deferred device synchronization. The original 19 exports passed Windows and Linux sanitizer tests and native Cairo 1.18.0 differential metadata checks. Surface user-data tests pass on Linux; the callback has no Windows run. No PS5 console comparison or complete rasterizer is claimed.

- The PS5 library revision and [surface user-data](../../../core/libs/prx/libcairo/Surface.cpp) callback behavior remain unmeasured. A null surface pointer still throws because its contract is unknown. Surface user-data ownership follows [Cairo 1.18.0](https://cairographics.org/releases/cairo-1.18.0.tar.xz), `src/cairo-surface.c` and `src/cairo-array.c`. Arbitrary recursive slot replacement or growth from callbacks is not established.

### Functional font options

- [FontOptions.cpp](../../../core/libs/prx/libcairo/FontOptions.cpp) implements eight font-option lifecycle, antialias and hint-setting helpers with Cairo-compatible owned state and a protected nil object. Tested on Windows and Linux sanitizers.

### Functional regions

- [Region.cpp](../../../core/libs/prx/libcairo/Region.cpp) implements six region lifecycle and rectangle operations with canonical band geometry and Cairo-compatible errors; Windows, Linux sanitizers and native Cairo/Pixman geometry comparisons passed.

### Functional context and coordinate transforms

- [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implements seventeen Cairo context lifecycle, nested matrix save/restore, and user/device coordinate transform exports, with an owned surface reference, a sticky invalid-matrix/invalid-restore state and System V guest ABI. Drawing, clipping, groups and rasterization remain pending; this does not provide a functioning Cairo renderer. The context code depends on the image/surface lifecycles above and shares the affine matrix primitives.

- [Path data in Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) builds paths, rectangles, arcs and path extents in Cairo 24.8 fixed point as Cairo 1.18.0 `cairo-path-fixed.c`, `cairo-arc.c` and `cairo-gstate.c` do, with the incremental inverse CTM and the line-width clamp; not measured on a console. Cairo recomputes its identity-transform shortcut when the target device scale changes; here a later device-scale change back to 1 does not restore the shortcut for a CTM that was modified to identity, which only changes results for non-finite coordinates. Tolerance is fixed at 0.1 because `cairo_set_tolerance` is not exported.

### Functional patterns

- [Pattern.cpp](../../../core/libs/prx/libcairo/Pattern.cpp) implements sixteen gradient, surface and mesh pattern constructors, reference counting, color stops, matrix, filter, extend and mesh patch exports following [Cairo 1.18.0](https://cairographics.org/releases/cairo-1.18.0.tar.xz) `src/cairo-pattern.c`. No pattern getter or rasterizer is exported, so the stored state is only checked by host tests; not measured on a console. A null pattern passed to a mutator is ignored, whereas Cairo dereferences it. Mesh patches are zero-initialised, so a corner copied from an unset corner 0 is transparent where Cairo copies uninitialised memory.

- [Context sources in Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implement `cairo_set_source`, `cairo_set_source_rgb`, `cairo_set_source_rgba`, `cairo_set_source_surface`, `cairo_set_operator`, `cairo_get_operator`, `cairo_set_font_size` and `cairo_set_font_options` as saved graphics state with Cairo 1.18.0 reference ownership and destruction order. The source, font matrix and font options are not used by any implemented drawing or text export; not measured on a console.

### Callable exports

- [Callable exports](../../../core/libs/prx/libcairo/Export.cpp) - 45 remaining callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libcairo.c) and [selected export metadata](https://gist.github.com/kyon-eth/a0bb48f2b3070a5eac8a638810f195f2/a3838ac54ebcc0bbfeb3cc46002a9a2dcce4dbdc) from two version-labelled images (SHA-256 `ec2158e0c5875316d200037d7f4bc48861626451752523f6930d687307c75ef8`). The labels are not authenticated. Prototypes and behavior of these remaining declarations are unverified; they still throw through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `cairo_scaled_font_get_font_face` | `+BL3-h8zGDo` |
| `cairo_paint` | `+PEo5Z7DOlY` |
| `cairo_font_face_set_user_data` | `1GGpdjyZslI` |
| `cairo_stroke_extents` | `1K5ly63m6fo` |
| `cairo_image_surface_create_from_png_stream` | `4dvdsqyLM8c` |
| `cairo_ft_scaled_font_lock_face` | `4gDTaBZH8lo` |
| `cairo_glyph_path` | `54HrgUIQHOk` |
| `cairo_scaled_font_destroy` | `6Y3HHedGzlw` |
| `cairo_scaled_font_get_font_options` | `8SOxQrQ7AfQ` |
| `cairo_in_stroke` | `8zMm0QOWZAA` |
| `cairo_scaled_font_get_ctm` | `A++pFuyxZPg` |
| `cairo_surface_write_to_png_stream` | `A3FLVZ4FHbY` |
| `cairo_ft_font_face_create_for_pattern` | `By6b1vjSVok` |
| `cairo_mask` | `D1KYCZ80Sac` |
| `cairo_scaled_font_extents` | `FbWFbLUQOys` |
| `cairo_show_glyphs` | `FnhwGKKZOjU` |
| `cairo_scaled_font_get_font_matrix` | `HKjYBSCx3mY` |
| `cairo_font_face_destroy` | `IgBFcd4cF28` |
| `cairo_scaled_font_text_extents` | `Jk9paxbMQsk` |
| `cairo_clip_extents` | `LJNqwiDWTm0` |
| `cairo_set_scaled_font` | `LUh-6rZhniA` |
| `cairo_fill` | `Ndvm8s28Gyc` |
| `cairo_pop_group_to_source` | `Oyzf6kDk6Ns` |
| `cairo_clip` | `PkGZZi1G2Gw` |
| `cairo_stroke_preserve` | `RzM4ZhlKzPg` |
| `cairo_ft_scaled_font_unlock_face` | `SJMLSxaYvWU` |
| `cairo_paint_with_alpha` | `TAIhaLjmAlg` |
| `cairo_scaled_font_status` | `a5WkqZXnG2w` |
| `cairo_in_fill` | `akimK5gpdho` |
| `cairo_font_face_get_user_data` | `bXupa3+NYIs` |
| `cairo_stroke` | `cdr2T4SoIyU` |
| `cairo_fill_preserve` | `dewyjTlk+u8` |
| `cairo_fill_extents` | `dkoQNd4kBlQ` |
| `cairo_ft_font_face_create_for_ft_face` | `dqEhIWRTjuo` |
| `cairo_push_group` | `hlh15JjNtWg` |
| `cairo_glyph_free` | `lAEbTbSUTTo` |
| `cairo_select_font_face` | `ldk9m5h+-no` |
| `cairo_scaled_font_reference` | `mb4bdMeFRs8` |
| `cairo_font_face_reference` | `nqmUW6Z8xLE` |
| `cairo_scaled_font_text_to_glyphs` | `ojbXRC0CmUo` |
| `cairo_scaled_font_glyph_extents` | `oy7NcAkex1I` |
| `cairo_show_text` | `psdqLsaPnkk` |
| `cairo_set_font_face` | `qzEwpfbLVxI` |
| `cairo_scaled_font_create` | `uKNPDudJTmo` |
| `cairo_device_destroy` | `wO1D4B90k0k` |

### FreeType font-option substitution

- [FreeType font-option substitution](../../../core/libs/prx/libcairo/src/FtFontOptions.cpp) follows Cairo 1.18.0 [cairo-ft-font.c](https://gitlab.freedesktop.org/cairo/cairo/-/blob/1.18.0/src/cairo-ft-font.c), `_cairo_ft_font_options_substitute` and its public wrapper, through the guest Fontconfig exports. PS5 behavior is unmeasured. The existing four-field font-options state has no internal LCD filter; its Cairo default leaves `lcdfilter` untouched. Hint metrics are unused by upstream substitution. Allocation failure stops substitution without rolling back earlier properties, as in the void upstream wrapper.
- [libfontconfig imports of the patched module](../../../core/libs/nid/src/PeNidPatcher.cpp) were not measured on a console. libcairo calls `FcPatternAddBool`, `FcPatternAddInteger`, `FcPatternDel` and `FcPatternGet` from libfontconfig; nid_patcher renames those raw exports to NIDs, and with `--import-directory` the PE patcher gives the importer the same NIDs. The ELF and Mach-O patchers do not take the option and keep the raw import names, which the patched libfontconfig does not export.

