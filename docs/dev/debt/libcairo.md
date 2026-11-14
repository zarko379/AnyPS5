# libcairo technical debt

### Functional

- [Matrix.cpp](../../../core/libs/prx/libcairo/Matrix.cpp), [Region.cpp](../../../core/libs/prx/libcairo/Region.cpp), [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp), [Png.cpp](../../../core/libs/prx/libcairo/Png.cpp), [Pattern.cpp](../../../core/libs/prx/libcairo/Pattern.cpp) and [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) - Sony's three Cairo patches ([source](https://www.playstation.com/en-us/oss/ps5/cairo/)) are not applied to the Cairo 1.18.0 these exports run, with the image backend only, as on PS5 system software 9.00 and later. The patch that changes behaviour adds `CAIRO_STATUS_NULL_SURFACE_IMAGE` where a surface pattern's image cannot be acquired; no export reaches that path while `cairo_paint`, `cairo_fill`, `cairo_mask` and the other drawing exports are stubs. Sony publishes no pixman or libpng source, so their console versions are unknown; this build uses pixman 0.42.2, the newest when Cairo 1.18.0 came out, and libpng 1.6.59.

- [Region.cpp](../../../core/libs/prx/libcairo/Region.cpp) - a null region or rectangle, which Cairo dereferences, returns `CAIRO_STATUS_NULL_POINTER` from `cairo_region_union_rectangle`, 0 from `cairo_region_num_rectangles` and leaves `cairo_region_get_rectangle` without effect; an `nth` outside the region, which Cairo reads past the array for, returns an empty rectangle. Negative sizes and coordinate overflow are not separately validated; pixman can reject an invalid box or wrap its endpoints. A merged box wider or taller than `INT32_MAX` overflows a signed subtraction in `cairo_region_get_rectangle`; the result is undefined. Console behaviour is unmeasured.

- [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp), [Pattern.cpp](../../../core/libs/prx/libcairo/Pattern.cpp) and [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) - Cairo dereferences these null pointers, and console behaviour is unmeasured. A null surface makes `cairo_surface_status` return `CAIRO_STATUS_NULL_POINTER`, and a null context makes `cairo_path_extents` store zeros, as Cairo answers for an object in error. A null surface in `cairo_surface_create_similar` or `cairo_surface_create_for_rectangle`, and null dashes with a positive count in `cairo_set_dash`, throw through `NotImplemented_nid_no_patch`, because Cairo has no error object to return for them. Every other null handle, key or callback reaches Cairo unchanged.

- [Png.cpp](../../../core/libs/prx/libcairo/Png.cpp) - console malformed-PNG status is unmeasured. Cairo 1.18.0 maps `CAIRO_STATUS_PNG_ERROR` to a no-memory error surface, so an invalid PNG CRC reports `CAIRO_STATUS_NO_MEMORY`. Later Cairo versions provide a distinct PNG-error surface; that change is not backported.

- [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) - `cairo_in_fill` on a context without error throws through `NotImplemented_nid_no_patch` when the fill rule is neither winding nor even-odd. Cairo 1.18.0 `cairo-path-in-fill.c` returns 0 for an empty path and 1 for a point on an edge under any rule, but reaches `ASSERT_NOT_REACHED` for any other point, which aborts or returns 0 depending on `NDEBUG`; the guard runs before Cairo, so the empty-path and on-edge answers are not reproduced for such a rule. Not measured on a console.

### Functional font options

- [FontOptions.cpp](../../../core/libs/prx/libcairo/FontOptions.cpp) implements eight font-option lifecycle, antialias and hint-setting helpers with Cairo-compatible owned state and a protected nil object. Tested on Windows and Linux sanitizers.

- [FontOptions.cpp](../../../core/libs/prx/libcairo/FontOptions.cpp) - `cairo_set_font_options` copies the antialias, subpixel order, hint style and hint metrics of the guest object into a Cairo 1.18.0 font options object before forwarding it, and passes null as null and the nil object as Cairo's nil. Cairo's other font-option fields keep their defaults because no guest export sets them; not measured on a console.

### Functional glyphs

- [Glyph.cpp](../../../core/libs/prx/libcairo/Glyph.cpp) implements `cairo_glyph_free` for null, which Cairo ignores. No export here allocates glyph arrays yet, and `cairo_glyph_allocate` is not in the catalogue, so a non-null array comes from outside this library and still throws through `NotImplemented_nid_no_patch` instead of reaching `free`. Freeing must follow the allocator once `cairo_scaled_font_text_to_glyphs` returns arrays. Unverified on a console.

### Callable exports

- [Callable exports](../../../core/libs/prx/libcairo/Export.cpp) - The remaining callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libcairo.c) and [selected export metadata](https://gist.github.com/kyon-eth/a0bb48f2b3070a5eac8a638810f195f2/a3838ac54ebcc0bbfeb3cc46002a9a2dcce4dbdc) from two version-labelled images (SHA-256 `ec2158e0c5875316d200037d7f4bc48861626451752523f6930d687307c75ef8`). The labels are not authenticated. Prototypes and behavior of these remaining declarations are unverified; they still throw through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `cairo_scaled_font_get_font_face` | `+BL3-h8zGDo` |
| `cairo_paint` | `+PEo5Z7DOlY` |
| `cairo_font_face_set_user_data` | `1GGpdjyZslI` |
| `cairo_stroke_extents` | `1K5ly63m6fo` |
| `cairo_ft_scaled_font_lock_face` | `4gDTaBZH8lo` |
| `cairo_glyph_path` | `54HrgUIQHOk` |
| `cairo_scaled_font_destroy` | `6Y3HHedGzlw` |
| `cairo_scaled_font_get_font_options` | `8SOxQrQ7AfQ` |
| `cairo_in_stroke` | `8zMm0QOWZAA` |
| `cairo_scaled_font_get_ctm` | `A++pFuyxZPg` |
| `cairo_ft_font_face_create_for_pattern` | `By6b1vjSVok` |
| `cairo_mask` | `D1KYCZ80Sac` |
| `cairo_scaled_font_extents` | `FbWFbLUQOys` |
| `cairo_show_glyphs` | `FnhwGKKZOjU` |
| `cairo_scaled_font_get_font_matrix` | `HKjYBSCx3mY` |
| `cairo_font_face_destroy` | `IgBFcd4cF28` |
| `cairo_scaled_font_text_extents` | `Jk9paxbMQsk` |
| `cairo_set_scaled_font` | `LUh-6rZhniA` |
| `cairo_fill` | `Ndvm8s28Gyc` |
| `cairo_pop_group_to_source` | `Oyzf6kDk6Ns` |
| `cairo_clip` | `PkGZZi1G2Gw` |
| `cairo_stroke_preserve` | `RzM4ZhlKzPg` |
| `cairo_ft_scaled_font_unlock_face` | `SJMLSxaYvWU` |
| `cairo_paint_with_alpha` | `TAIhaLjmAlg` |
| `cairo_scaled_font_status` | `a5WkqZXnG2w` |
| `cairo_font_face_get_user_data` | `bXupa3+NYIs` |
| `cairo_stroke` | `cdr2T4SoIyU` |
| `cairo_fill_preserve` | `dewyjTlk+u8` |
| `cairo_fill_extents` | `dkoQNd4kBlQ` |
| `cairo_ft_font_face_create_for_ft_face` | `dqEhIWRTjuo` |
| `cairo_push_group` | `hlh15JjNtWg` |
| `cairo_ft_font_options_substitute` | `ii895dyBjMo` |
| `cairo_select_font_face` | `ldk9m5h+-no` |
| `cairo_scaled_font_reference` | `mb4bdMeFRs8` |
| `cairo_font_face_reference` | `nqmUW6Z8xLE` |
| `cairo_scaled_font_text_to_glyphs` | `ojbXRC0CmUo` |
| `cairo_scaled_font_glyph_extents` | `oy7NcAkex1I` |
| `cairo_show_text` | `psdqLsaPnkk` |
| `cairo_set_font_face` | `qzEwpfbLVxI` |
| `cairo_scaled_font_create` | `uKNPDudJTmo` |
| `cairo_device_destroy` | `wO1D4B90k0k` |
