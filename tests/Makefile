# Makefile for afterhours automated tests
#
# Usage:
#   make        - Build all tests
#   make test   - Build and run all tests
#   make win-vulkan - Cross-build sokol tests for Windows/Vulkan -> output/win/*.exe
#   make clean  - Remove test binaries

CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -g
CXXFLAGS_RAYLIB := $(CXXFLAGS) -DAFTER_HOURS_USE_RAYLIB

# Homebrew prefix differs per machine (/opt/homebrew on Apple-Silicon default,
# ~/homebrew for a per-user install). Auto-detect via `brew --prefix raylib`
# and fall back to the classic Cellar path. Override on the CLI if needed:
#   make RAYLIB_PREFIX=/path/to/raylib
RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)
ifeq ($(RAYLIB_PREFIX),)
    RAYLIB_PREFIX := /opt/homebrew/Cellar/raylib/5.5
endif
RAYLIB_INCLUDE := $(RAYLIB_PREFIX)/include
RAYLIB_LIB := $(RAYLIB_PREFIX)/lib
# fmt is header-only here; its headers live under the Homebrew include dir.
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifeq ($(BREW_PREFIX),)
    BREW_PREFIX := /opt/homebrew
endif
# Pin <afterhours/...> to THIS checkout regardless of the directory's name.
# A bare `-isystem ../..` resolves <afterhours/...> to the *parent* of the
# checkout dir, which only works when the checkout is literally named
# `afterhours`. In a git worktree (or any differently-named clone) that silently
# picks up a *sibling* `afterhours` checkout, so tests compile against the wrong
# source. Generate a private include dir containing a symlink named exactly
# `afterhours` -> this checkout root, and use it as the sole afterhours root.
AFTERHOURS_ROOT := $(realpath ..)
INCLUDE_ROOT := $(CURDIR)/.afh-include
$(shell mkdir -p $(INCLUDE_ROOT) && ln -sfn $(AFTERHOURS_ROOT) $(INCLUDE_ROOT)/afterhours)

# Emits a .d beside each binary listing the headers it pulled in. Without this,
# editing a header rebuilds nothing and you test a stale binary.
#
# -MD, not -MMD: the afterhours headers arrive via -isystem (see INCLUDE_ROOT
# below), which makes them system headers, and -MMD omits those by definition.
# With -MMD the dep files come out not mentioning a single afterhours header —
# tracking that silently does nothing. The cost is SDK headers in the .d too.
DEPFLAGS := -MD -MP

INCLUDES := -isystem $(INCLUDE_ROOT) -isystem $(AFTERHOURS_ROOT)/vendor -isystem $(BREW_PREFIX)/include
INCLUDES_RAYLIB := -I$(RAYLIB_INCLUDE) $(INCLUDES)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS_RAYLIB := $(RAYLIB_LIB)/libraylib.a -framework OpenGL -framework CoreFoundation -framework Cocoa -framework IOKit -framework CoreGraphics -framework CoreVideo -lobjc
else ifeq ($(UNAME_S),Linux)
    LDFLAGS_RAYLIB := -L$(RAYLIB_LIB) -lraylib -lGL -lX11 -lEGL
endif

# All build products land in the repo-wide output/ dir (see
# examples/catalog/common.mk) instead of littering the source tree.
OUT := $(realpath ..)/output
$(shell mkdir -p $(OUT))

ALL_TESTS := \
	border_width_test \
	border_only_test \
	checkbox_mark_test \
	touch_target_test \
	touch_target_single_test \
	font_tier_scaling_test \
	text_visibility_test \
	image_tint_test \
	e2e_arguments_test \
	scrollbar_render_test \
	scroll_input_gate_test \
	middle_click_test \
	wheel_input_test \
	wheel_delta_kind_test \
	command_picker_test \
	command_picker_ui_test \
	terminal_test \
	terminal_commands_test \
	three_d_commands_test \
	state_commands_test \
	terminal_ui_test \
	font_atlas_measure_test \
	font_prewarm_test \
	sokol_letter_spacing_test \
	font_display_size_test \
	font_points_test \
	window_focus_test \
	glyph_placeholder_test \
	monospace_cell_test \
	font_memory_test \
	ui_texture_lifecycle_test \
	ui_update_lifecycle_test \
	ui_update_lifecycle_single_test \
	drag_preview_test \
	drag_scroll_hit_test \
	parent_gesture_test \
	label_clip_test \
	label_test \
	scroll_clip_decoration_test \
	animation_test \
	reveal_test \
	motion_config_test \
	reduced_motion_test \
	gesture_availability_test \
	appearance_test \
	frame_loop_test \
	headless_cadence_test \
	jobs_test \
	frame_phases_test \
	color_value_test \
	value_types_test \
	value_types_none_test \
	color_palette_test \
	color_palette_none_test \
	wrappers_test \
	window_state_test \
	animation_presets_test \
	shimmer_test \
	particles_test \
	trigger_volume_test \
	ui_modifiers_test \
	ancestor_transform_test \
	text_units_test \
	text_fit_test \
	none_measure_test \
	triangle_winding_none_test \
	text_unit_motion_test \
	text_alignment_test \
	sound_volume_test \
	caption_test \
	audio_plugin_test \
	audio_stream_test \
	synth_beep_test \
	settings_save_test \
	settings_comment_test \
	settings_path_test \
	settings_table_test \
	files_test_roots_test \
	files_e2e_temp_roots_test \
	files_e2e_tmpdir_env_test \
	clipboard_test \
	native_dialogs_test \
	native_dialogs_async_test \
	input_binding_codec_test \
	input_prompts_test \
	input_device_activity_test \
	input_activity_snapshot_test \
	input_chord_specificity_test \
	input_enum_actions_test \
	input_repeat_test \
	input_remap_test \
	input_axis_edge_test \
	file_watcher_test \
	render_capture_test \
	presented_frame_test \
	triangle_winding_test \
	bug_note_test \
	charts_test \
	system_profile_test \
	allocation_counters_test \
	system_groups_test \
	profiling_test \
	profiling_disabled_test \
	profiling_collapsed_test \
	spike_capture_test \
	render_counters_test \
	render_counters_disabled_test \
	perf_provider_warn_test \
	blend_mode_test \
	blur_shader_test \
	rect_test \
	math3d_test \
	rounded_corners_test \
	rounded_outline_mask_test \
	viewport_test \
	grid_test \
	pathfinding_provider_test \
	bitset_test \
	log_file_test \
	crash_handler_test \
	crash_handler_reentry_test \
	crash_handler_web_sim_test \
	crash_report_file_test \
	setting_row_style_test \
	settings_presets_test \
	singleton_iteration_test \
	get_unique_test \
	singleton_add_test \
	ui_idle_cost_test \
	cached_singleton_test \
	press_delay_test \
	graphics_key_injection_test \
	e2e_gamepad_test \
	e2e_key_command_test \
	e2e_type_command_test \
	overlay_layer_test \
	popup_overlay_layer_test \
	unhandled_command_test \
	e2e_retry_test \
	e2e_resize_steps_test \
	e2e_render_checkpoint_test \
	e2e_window_size_test \
	e2e_app_test \
	e2e_audio_commands_test \
	e2e_audio_volume_test \
	e2e_audio_volume_disabled_test \
	e2e_register_command_test \
	e2e_screenshot_test \
	e2e_expect_screenshot_test \
	e2e_is_running_test \
	e2e_defer_unknown_test \
	e2e_pending_list_test \
	e2e_unread_press_test \
	e2e_results_test \
	e2e_script_headers_test \
	point_in_element_test \
	e2e_ui_gestures_test \
	e2e_name_lookup_test \
	mouse_axis_action_test \
	card_and_type_scale_test \
	fixed_timestep_test \
	time_scale_substeps_test \
	audio_lifecycle_test \
	audio_stub_build_test \
	notimplemented_log_test \
	local_time_test \
	font_catalog_test \
	frame_rate_stats_test \
	tunable_test \
model_library_test \
lighting_zones_test \
	app_states_test \
	intro_sequence_test \
	graphics_concepts_pedantic_test \
	fixed_tick_rate_test \
	sound_playback_test \
	headless_runconfig_test \
	window_state_runconfig_test \
	harness_spellings_test \
	fit_content_test \
	label_inset_test \
	text_memo_test \
	pseudo_locale_test \
	editing_capability_test \
	autolayout_test \
	origin_rounding_test \
	trailing_anchor_test \
	dialog_test \
	escape_owner_test \
	dump_ui_test \
	accessibility_test \
	scroll_anchor_test \
	scroll_absolute_content_test \
	measure_config_test \
	scroll_anchor_test \
	widget_retirement_test \
	text_line_edit_test \
	grapheme_edit_test \
	entity_mapping_test \
	files_atomic_write_test \
	files_confined_path_test \
	files_resource_path_test \
	files_cache_path_test \
	files_documents_test \
	capture_test \
	capture_perf_test \
	capture_async_test \
	polyline_test \
	measure_memo_test \
	design_defaults_test \
	default_theme_test \
	entity_index_test \
	index_scaling_test \
	query_seed_test \
	query_skip_mods_test \
	capture_attribution_test \
	capture_frame_boundary_test \
	draw_commands_test \
	screen_luma_test \
	color_contrast_test \
	auto_text_color_test \
	scissor_coalescing_test \
	blend_batch_test \
	focus_origin_test \
	tab_order_test \
	reset_focus_test \
	focus_shortcuts_test \
	cleanup_order_test \
	attachment_test \
	canonical_snapshot_test \
	entity_pool_test \
	entity_query_test \
	entity_membership_test \
	temp_warning_caller_test \
	query_scratch_test \
	prefabs_test \
	get_or_default_test \
	component_toggle_test \
	query_extreme_by_test \
	query_filters_test \
	typed_entity_handle_test \
	input_injector_mouse_delta_test \
	keymap_test \
	label_clear_test \
	hit_priority_test \
	pointer_owner_test \
	two_trees_test \
	text_selection_test \
	selectable_text_test \
	text_stroke_test \
	text_stroke_render_test \
	multiline_text_test \
	subtree_hover_test \
	hover_focus_name_test \
	inert_test \
	menu_test \
	color_swatch_test \
	overlay_placement_test \
	tooltip_test \
	delayed_busy_test \
	progress_bar_test \
	psx_budget_test \
	psx_backend_test \
	pagination_test \
	random_engine_test \
	render_order_test \
	draw_over_test \
	scroll_smoothing_test \
	scrollbar_gutter_test \
	setup_test \
	slot_at_creation_test \
	size_constraints_test \
	theme_scope_test \
	defer_test \
	draw_sort_test \
	config_size_test \
	config_strings_test \
	overdraw_audit_test \
	virtual_list_test \
	folded_range_test \
	text_action_names_test \
	sizing_repro_test \
	slider_test \
	form_binding_test \
	split_test \
	downstream_gaps_test \
	headless_fallback_test \
	raylib_fallback_test \
	models_test \
	scene_lights_test \
	effects_uniform_cache_test \
	house_of_cards_test \
	frustum_test \
	eye_position_test \
	instance_batcher_test \
	instance_batcher_persist_test \
	lod_test \
	blob_shadow_test \
	world_label_test \
	render_view_test \
	image_test \
	image_raylib_test \
	texture_usable_test \
	texture_usable_raylib_test \
	per_draw_filter_test \
	stepper_test \
	styling_test \
	system_tags_test \
	should_run_overload_test \
	system_deactivate_hook_test \
	library_duplicate_test \
	library_intern_test \
	shared_handlers_test \
	default_font_test \
	load_font_size_test \
	edit_keys_test \
	tab_container_test \
	tab_toggle_geometry_test \
	small_components_test \
	text_area_test \
	text_area_wrap_cache_test \
	text_input_test \
	toast_test \
	text_wrap_test \
	label_word_wrap_test \
	theme_io_test \
	tree_view_test \
	window_helpers_test \
	skeletal_test \
	translation_test \
	layout_inspector_test \
	letter_spacing_zero_test \
	mouse_pre_event_test

# .app packaging is macOS-only, and the test shells out to plutil.
ifeq ($(UNAME_S),Darwin)
ALL_TESTS += bundle_test
ALL_TESTS += native_dialogs_macos_test
ALL_TESTS += render_capture_metal_test
ALL_TESTS += triangle_winding_metal_test
ALL_TESTS += image_metal_test
ALL_TESTS += texture_usable_metal_test
# Sokol is Metal here, so macOS-only; renders headless, no window.
ALL_TESTS += sokol_blend_test
ALL_TESTS += sokol_capture_test
ALL_TESTS += sokol_geometry_test
ALL_TESTS += sokol_rounded_outline_mask_test
# Own process: the Metal backend cannot be re-initialised, so this cannot be a
# section of sokol_blend_test.
ALL_TESTS += sokol_hidpi_test
ALL_TESTS += capture_supersample_test
ALL_TESTS += sokol_resize_test
ALL_TESTS += sokol_multires_capture_test
ALL_TESTS += sokol_headless_run_test
ALL_TESTS += web_metal_symbols_test
ALL_TESTS += sokol_lazy_gl_test
ALL_TESTS += sokol_include_order_test
ALL_TESTS += sokol_wrappers_test
ALL_TESTS += sokol_frame_dt_test
ALL_TESTS += sokol_msaa_test
ALL_TESTS += sokol_cmd_chords_test
ALL_TESTS += sokol_mouse_pre_init_test
ALL_TESTS += sokol_startup_order_test
ALL_TESTS += staging_texture_leak_test
endif

RAYLIB_TESTS := \
	auto_text_color_test \
	e2e_app_test \
	skeletal_test \
	math3d_test \
	value_types_test \
	wrappers_test \
	window_focus_test \
	window_state_test \
	edit_keys_test \
	default_font_test \
	font_display_size_test \
	load_font_size_test \
	border_width_test \
	border_only_test \
	text_visibility_test \
	image_tint_test \
	scroll_input_gate_test \
	middle_click_test \
	wheel_input_test \
	wheel_delta_kind_test \
	command_picker_ui_test \
	terminal_ui_test \
	text_stroke_render_test \
	input_device_activity_test \
	input_activity_snapshot_test \
	input_enum_actions_test \
	input_axis_edge_test \
	render_capture_test \
	presented_frame_test \
	triangle_winding_test \
	bug_note_test \
	blur_shader_test \
	rounded_outline_mask_test \
	clipboard_test \
	mk_cost_test \
	capture_perf_test \
	headless_fallback_test \
	raylib_fallback_test \
	models_test \
	scene_lights_test \
	effects_uniform_cache_test \
	house_of_cards_test \
	frustum_test \
	eye_position_test \
	instance_batcher_test \
	instance_batcher_persist_test \
	dialog_test \
	escape_owner_test \
	dump_ui_test \
	scroll_anchor_test \
	measure_config_test \
	scroll_anchor_test \
	widget_retirement_test \
	text_line_edit_test \
	grapheme_edit_test \
	delayed_busy_test \
	progress_bar_test \
	render_order_test \
	setup_test \
	size_constraints_test \
	slider_test \
	form_binding_test \
	split_test \
	stepper_test \
	styling_test \
	tab_container_test \
	small_components_test \
	text_area_test \
	text_input_test \
	color_swatch_test \
	editing_capability_test \
	label_inset_test \
	fit_content_test \
	headless_runconfig_test \
	window_state_runconfig_test \
	harness_spellings_test \
	fixed_timestep_test \
	time_scale_substeps_test \
	audio_lifecycle_test \
	sound_playback_test \
	card_and_type_scale_test \
	mouse_axis_action_test \
	unhandled_command_test \
	overlay_layer_test \
	popup_overlay_layer_test \
	press_delay_test \
	graphics_key_injection_test \
	e2e_gamepad_test \
	text_wrap_test \
	label_word_wrap_test \
	theme_io_test \
	tree_view_test \
	layout_inspector_test

ifeq ($(UNAME_S),Darwin)
ALL_TESTS += gesture_availability_macos_test
endif

TEST_BINS := $(addprefix $(OUT)/,$(ALL_TESTS))

# Declared after ALL_TESTS so the bare-name aliases are actually included.
.PHONY: all test clean $(ALL_TESTS)

all: $(TEST_BINS)

# Bare names stay usable: `make split_test` builds $(OUT)/split_test.
$(ALL_TESTS): %: $(OUT)/%

# One rule for every test. The raylib split rides on target-specific variables
# rather than a second copy of the recipe.
RAYLIB_BINS := $(addprefix $(OUT)/,$(RAYLIB_TESTS))

CXXFLAGS_T = $(CXXFLAGS)
INCLUDES_T = $(INCLUDES)
LDFLAGS_T  =

$(RAYLIB_BINS): CXXFLAGS_T = $(CXXFLAGS_RAYLIB)
$(RAYLIB_BINS): INCLUDES_T = $(INCLUDES_RAYLIB)
$(RAYLIB_BINS): LDFLAGS_T  = $(LDFLAGS_RAYLIB)

$(OUT)/crash_handler_web_sim_test: CXXFLAGS_T = $(CXXFLAGS) -D__EMSCRIPTEN__
$(OUT)/crash_handler_web_sim_test: INCLUDES_T = -Iweb_sim $(INCLUDES)

# Shift+Enter is a raw is_key_down check, and the key injector is only wired in
# behind this macro. Without it the backend is asked directly and answers false
# with no window, so the modifier half of a chord cannot be tested at all.
$(OUT)/input_device_activity_test $(OUT)/input_enum_actions_test $(OUT)/input_axis_edge_test: LDFLAGS_T = $(subst $(RAYLIB_LIB)/libraylib.a,-L$(RAYLIB_LIB) -lraylib,$(LDFLAGS_RAYLIB))
# These tests double every raylib input function they exercise and stub
# the rest (input_raylib_unused_stubs.h), so they link no raylib at all.
# Linking the real archive pulls rcore.c.o for the unused window/rlgl entry
# points, and raylib 6.x's rcore.c.o also defines the doubled input
# functions: duplicate symbols against a static-only install. Keep just the
# platform frameworks (either raylib spelling is stripped, so this holds on
# Linux too).
$(OUT)/input_device_activity_test $(OUT)/input_enum_actions_test $(OUT)/input_axis_edge_test: LDFLAGS_T = $(subst -lraylib,,$(subst $(RAYLIB_LIB)/libraylib.a,,$(LDFLAGS_RAYLIB)))

$(OUT)/text_area_test $(OUT)/scroll_input_gate_test $(OUT)/middle_click_test $(OUT)/edit_keys_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB) -DAFTER_HOURS_ENABLE_E2E_TESTING
$(OUT)/text_area_test $(OUT)/scroll_input_gate_test $(OUT)/wheel_input_test $(OUT)/edit_keys_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB) -DAFTER_HOURS_ENABLE_E2E_TESTING
$(OUT)/text_area_test $(OUT)/scroll_input_gate_test $(OUT)/edit_keys_test $(OUT)/graphics_key_injection_test $(OUT)/input_activity_snapshot_test $(OUT)/e2e_gamepad_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB) -DAFTER_HOURS_ENABLE_E2E_TESTING
$(OUT)/text_area_test $(OUT)/scroll_input_gate_test $(OUT)/wheel_input_test $(OUT)/wheel_delta_kind_test $(OUT)/edit_keys_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB) -DAFTER_HOURS_ENABLE_E2E_TESTING
$(OUT)/text_area_test $(OUT)/scroll_input_gate_test $(OUT)/edit_keys_test $(OUT)/graphics_key_injection_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB) -DAFTER_HOURS_ENABLE_E2E_TESTING
$(OUT)/text_area_test $(OUT)/scroll_input_gate_test $(OUT)/edit_keys_test $(OUT)/graphics_key_injection_test $(OUT)/input_activity_snapshot_test $(OUT)/e2e_gamepad_test $(OUT)/form_binding_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB) -DAFTER_HOURS_ENABLE_E2E_TESTING

# Filter to sources: the .d files list every header as a prerequisite, and a
# bare $^ would hand them all to clang. .cc is the sokol implementation TU.
$(OUT)/%: %.cpp
	$(CXX) $(CXXFLAGS_T) $(DEPFLAGS) $(INCLUDES_T) $(filter %.cpp %.cc,$^) $(LDFLAGS_T) -o $@

# Exceptions. A rule with prerequisites and no recipe adds to the pattern rule
# above instead of replacing it.
$(OUT)/ui_update_lifecycle_single_test: ui_update_lifecycle_single_test.cpp
	$(CXX) $(CXXFLAGS_T) $(DEPFLAGS) $(INCLUDES_T) $< $(LDFLAGS_T) -o $@

$(OUT)/touch_target_single_test: touch_target_single_test.cpp
	$(CXX) $(CXXFLAGS_T) $(DEPFLAGS) $(INCLUDES_T) $< $(LDFLAGS_T) -o $@

$(OUT)/entity_mapping_test: ../examples/entity_mapping_test_helper.cpp
$(OUT)/entity_mapping_test: INCLUDES_T += -I../examples
$(OUT)/random_engine_test:  CXXFLAGS_T += -DAFTER_HOURS_ENABLE_RANDOM
$(OUT)/temp_warning_caller_test: CXXFLAGS_T += -DAFTER_HOURS_DEBUG
$(OUT)/graphics_concepts_pedantic_test: CXXFLAGS_T += -Wpedantic -Werror
$(OUT)/graphics_concepts_pedantic_test: INCLUDES_T = -I$(INCLUDE_ROOT) -isystem $(AFTERHOURS_ROOT)/vendor -isystem $(BREW_PREFIX)/include
$(OUT)/sound_volume_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB)
$(OUT)/sound_volume_test: INCLUDES_T = $(INCLUDES_RAYLIB)
$(OUT)/caption_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB)
$(OUT)/caption_test: INCLUDES_T = $(INCLUDES_RAYLIB)
$(OUT)/audio_plugin_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB)
$(OUT)/audio_plugin_test: INCLUDES_T = $(INCLUDES_RAYLIB)
$(OUT)/audio_stream_test: CXXFLAGS_T = $(CXXFLAGS_RAYLIB)
$(OUT)/audio_stream_test: INCLUDES_T = $(INCLUDES_RAYLIB)
$(OUT)/files_atomic_write_test: ../src/plugins/files.cpp
$(OUT)/files_confined_path_test: ../src/plugins/files.cpp
$(OUT)/files_resource_path_test: ../src/plugins/files.cpp
$(OUT)/files_test_roots_test: ../src/plugins/files.cpp
$(OUT)/files_e2e_temp_roots_test: ../src/plugins/files.cpp
$(OUT)/files_e2e_tmpdir_env_test: ../src/plugins/files.cpp
$(OUT)/files_cache_path_test: ../src/plugins/files.cpp
$(OUT)/files_documents_test: ../src/plugins/files.cpp
$(OUT)/bundle_test: ../src/plugins/files.cpp
$(OUT)/file_watcher_test: ../src/plugins/file_watcher.cpp

$(OUT)/native_dialogs_async_test: ../src/plugins/native_dialogs.cpp

$(OUT)/native_dialogs_macos_test: native_dialogs_macos_test.mm ../src/plugins/native_dialogs_macos.mm
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) $(filter %.mm,$^) -framework Cocoa -o $@

ifeq ($(UNAME_S),Darwin)
$(OUT)/reduced_motion_test: LDFLAGS_T += -framework AppKit
$(OUT)/appearance_test: LDFLAGS_T += -framework AppKit
endif

ifeq ($(UNAME_S),Darwin)
$(OUT)/gesture_availability_macos_test: gesture_availability_test.cpp
	$(CXX) $(CXXFLAGS) -DAFTER_HOURS_ENABLE_MACOS_GESTURES -fblocks $(DEPFLAGS) $(INCLUDES) $< -framework AppKit -o $@
endif

# Sokol's implementation TU must be Objective-C++ for Metal; reuse the web
# example's rather than keeping a second copy.
SOKOL_FRAMEWORKS := -framework Metal -framework MetalKit -framework Cocoa \
                    -framework QuartzCore -framework AudioToolbox -framework Foundation
$(OUT)/sokol_blend_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_blend_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_blend_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_capture_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_capture_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_capture_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/sokol_capture_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_geometry_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_geometry_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_geometry_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/web_metal_symbols_test: web_metal_symbols_test.cpp web_sim/web_probe.cpp web_sim/emscripten.h
	$(CXX) $(CXXFLAGS) -c -DAFTER_HOURS_USE_SOKOL -DSOKOL_GLES3=1 -D__EMSCRIPTEN__ -isystem $(CURDIR)/web_sim $(INCLUDES) web_sim/web_probe.cpp -o $(OUT)/web_probe_web.o
	@if nm -u $(OUT)/web_probe_web.o | grep -E '_?(metal|vulkan)_'; then echo "web probe references Metal/Vulkan symbols (R-144 regression)"; exit 1; fi
	$(CXX) $(CXXFLAGS) -c -DAFTER_HOURS_USE_SOKOL -DSOKOL_METAL=1 $(INCLUDES) web_sim/web_probe.cpp -o $(OUT)/web_probe_native.o
	@if ! nm -u $(OUT)/web_probe_native.o | grep -q '_metal_create_system_device'; then echo "native control lost Metal symbols (over-exclusion)"; exit 1; fi
	@echo "web: no undefined metal_*/vulkan_* symbols" > $(OUT)/web_metal_symbols.audit
	@echo "native: metal_* symbols referenced" >> $(OUT)/web_metal_symbols.audit
	$(CXX) $(CXXFLAGS) -DWEB_METAL_AUDIT_PATH=\"$$(realpath $(OUT)/web_metal_symbols.audit)\" $(INCLUDES) $< -o $@

# The multi-TU sokol rules emit a .d covering only sokol_impl.cc, so header
# edits rebuild nothing without an explicit prerequisite (stale-binary trap).
$(OUT)/sokol_blend_test: ../src/backends/sokol/drawing_helpers.h
$(OUT)/sokol_capture_test: ../src/backends/sokol/drawing_helpers.h ../src/capture.h ../src/graphics_common.h
$(OUT)/sokol_geometry_test: ../src/backends/sokol/drawing_helpers.h
$(OUT)/sokol_rounded_outline_mask_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_rounded_outline_mask_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_rounded_outline_mask_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_rounded_outline_mask_test: ../src/backends/sokol/drawing_helpers.h ../src/plugins/ui/rounded_corners.h
$(OUT)/sokol_hidpi_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_hidpi_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_hidpi_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/capture_supersample_test: ../examples/web/sokol_impl.cc
$(OUT)/capture_supersample_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/capture_supersample_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/capture_supersample_test: ../src/backends/sokol/drawing_helpers.h ../src/backends/sokol/backend.h
$(OUT)/sokol_resize_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_resize_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_resize_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_resize_test: ../src/backends/sokol/backend.h ../src/backends/sokol/window_macos.h ../src/backends/sokol/capture_impl.h
$(OUT)/sokol_multires_capture_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_multires_capture_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_multires_capture_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_headless_run_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_headless_run_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_headless_run_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_lazy_gl_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_lazy_gl_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_lazy_gl_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_lazy_gl_test: ../src/backends/sokol/drawing_helpers.h ../src/backends/sokol/backend.h
$(OUT)/sokol_include_order_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_include_order_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_include_order_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_wrappers_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_wrappers_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_wrappers_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/sokol_wrappers_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_wrappers_test: ../src/backends/sokol/drawing_helpers.h ../src/backends/sokol/backend.h
$(OUT)/sokol_frame_dt_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_frame_dt_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_frame_dt_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_frame_dt_test: ../src/backends/sokol/backend.h
$(OUT)/sokol_msaa_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_msaa_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_msaa_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_msaa_test: ../src/backends/sokol/drawing_helpers.h ../src/backends/sokol/backend.h
$(OUT)/sokol_cmd_chords_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_cmd_chords_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_cmd_chords_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_cmd_chords_test: ../src/backends/sokol/backend.h
$(OUT)/sokol_startup_order_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_startup_order_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_startup_order_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_startup_order_test: ../src/backends/sokol/backend.h ../src/backends/sokol/capture_impl.h

$(OUT)/staging_texture_leak_test: ../examples/web/sokol_impl.cc
$(OUT)/staging_texture_leak_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/staging_texture_leak_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/staging_texture_leak_test: ../src/backends/sokol/capture_impl.h

$(OUT)/render_capture_metal_test: render_capture_test.cpp ../examples/web/sokol_impl.cc
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -DAFTER_HOURS_USE_SOKOL -x objective-c++ $(filter %.cpp %.cc,$^) $(SOKOL_FRAMEWORKS) -o $@

$(OUT)/triangle_winding_metal_test: triangle_winding_test.cpp ../examples/web/sokol_impl.cc
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -DAFTER_HOURS_USE_SOKOL -x objective-c++ $(filter %.cpp %.cc,$^) $(SOKOL_FRAMEWORKS) -o $@

$(OUT)/image_metal_test: image_test.cpp ../examples/web/sokol_impl.cc
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -DAFTER_HOURS_USE_SOKOL -x objective-c++ $(filter %.cpp %.cc,$^) $(SOKOL_FRAMEWORKS) -o $@

$(OUT)/blur_shader_test: ../src/plugins/files.cpp

$(OUT)/image_raylib_test: image_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(DEPFLAGS) $(INCLUDES_RAYLIB) $< $(LDFLAGS_RAYLIB) -o $@

$(OUT)/texture_usable_metal_test: texture_usable_test.cpp ../examples/web/sokol_impl.cc
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -DAFTER_HOURS_USE_SOKOL -x objective-c++ $(filter %.cpp %.cc,$^) $(SOKOL_FRAMEWORKS) -o $@

$(OUT)/texture_usable_raylib_test: texture_usable_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(DEPFLAGS) $(INCLUDES_RAYLIB) $< $(LDFLAGS_RAYLIB) -o $@

$(OUT)/per_draw_filter_test: per_draw_filter_test.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) $(DEPFLAGS) $(INCLUDES_RAYLIB) $< $(LDFLAGS_RAYLIB) -o $@

# Windows/Vulkan cross-build (from macOS, via zig). The .exes link on the Mac
# and run on the Windows box. They render headless and read pixels back, so
# they can't run until the headless device + capture land (Phases 3-4);
# in this phase linking is the check. Not part of `all`.
# Header edits don't rebuild the .exes (no .d tracking here, same trap as
# the Metal rules); delete output/win/ to force a rebuild.
WIN_VULKAN_OUT := $(OUT)/win
WIN_VULKAN_TESTS := sokol_blend_test sokol_geometry_test sokol_hidpi_test \
	sokol_resize_test sokol_include_order_test
WIN_VULKAN_BINS := $(addprefix $(WIN_VULKAN_OUT)/,$(addsuffix .exe,$(WIN_VULKAN_TESTS))) \
	$(WIN_VULKAN_OUT)/render_capture_vulkan_test.exe
WIN_VULKAN_CXXFLAGS := -std=c++20 -Wall -Wextra -g -target x86_64-windows-gnu \
	-DAFTER_HOURS_USE_VULKAN
WIN_VULKAN_INCLUDES := $(INCLUDES) -isystem $(AFTERHOURS_ROOT)/vendor/vulkan/include
WIN_VULKAN_LDFLAGS := -lvulkan-1 -lkernel32 -luser32 -lshell32 -lgdi32

$(WIN_VULKAN_OUT)/libvulkan-1.a: $(AFTERHOURS_ROOT)/vendor/vulkan/vulkan-1.def
	@mkdir -p $(WIN_VULKAN_OUT)
	zig dlltool -d $< -l $@ -m i386:x86-64

$(WIN_VULKAN_OUT)/%.exe: %.cpp ../examples/web/sokol_impl.cc $(WIN_VULKAN_OUT)/libvulkan-1.a
	zig c++ $(WIN_VULKAN_CXXFLAGS) $(WIN_VULKAN_INCLUDES) $(filter %.cpp %.cc,$^) -L$(WIN_VULKAN_OUT) $(WIN_VULKAN_LDFLAGS) -o $@

$(WIN_VULKAN_OUT)/render_capture_vulkan_test.exe: render_capture_test.cpp ../examples/web/sokol_impl.cc $(WIN_VULKAN_OUT)/libvulkan-1.a
	zig c++ $(WIN_VULKAN_CXXFLAGS) $(WIN_VULKAN_INCLUDES) $(filter %.cpp %.cc,$^) -L$(WIN_VULKAN_OUT) $(WIN_VULKAN_LDFLAGS) -o $@

win-vulkan: $(WIN_VULKAN_BINS)
.PHONY: win-vulkan

# Runs every test even when one fails, then reports. The old loop dropped each
# exit code, so only the last test's status escaped and a red suite read green.
check-boundaries:
	bash ../check_plugin_boundaries.sh
	sh ui_boundary_check.sh
	sh theme_bypass_check.sh
.PHONY: check-boundaries
test: check-boundaries all
	@fail=0; for t in $(ALL_TESTS); do \
		echo "Running $$t"; \
		$(OUT)/$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "SOME TESTS FAILED"; exit 1; fi

clean:
	rm -rf $(TEST_BINS) $(addsuffix .d,$(TEST_BINS)) $(addsuffix .dSYM,$(TEST_BINS))
	rm -f input_system_mouse_delta_backend_smoke.o smoke_raylib.o smoke_sokol.o
	rm -rf $(INCLUDE_ROOT)
	rm -rf $(WIN_VULKAN_OUT)

-include $(wildcard $(OUT)/*.d)

$(OUT)/profiling_benchmark_on: profiling_benchmark.cpp
	$(CXX) $(CXXFLAGS) -O2 -DAFTERHOURS_ENABLE_PROFILING=1 $(DEPFLAGS) $(INCLUDES) $< -o $@

$(OUT)/profiling_benchmark_off: profiling_benchmark.cpp
	$(CXX) $(CXXFLAGS) -O2 -DAFTERHOURS_ENABLE_PROFILING=0 $(DEPFLAGS) $(INCLUDES) $< -o $@

.PHONY: profiling-benchmark
profiling-benchmark: $(OUT)/profiling_benchmark_on $(OUT)/profiling_benchmark_off
	$(OUT)/profiling_benchmark_off stopped
	$(OUT)/profiling_benchmark_on stopped
	$(OUT)/profiling_benchmark_on record
	$(OUT)/profiling_benchmark_on panel

$(OUT)/profiling_render_benchmark_on: profiling_benchmark.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) -O2 -DAFTERHOURS_ENABLE_PROFILING=1 $(DEPFLAGS) $(INCLUDES_RAYLIB) $< $(LDFLAGS_RAYLIB) -o $@

$(OUT)/profiling_render_benchmark_off: profiling_benchmark.cpp
	$(CXX) $(CXXFLAGS_RAYLIB) -O2 -DAFTERHOURS_ENABLE_PROFILING=0 $(DEPFLAGS) $(INCLUDES_RAYLIB) $< $(LDFLAGS_RAYLIB) -o $@

.PHONY: profiling-render-benchmark
profiling-render-benchmark: $(OUT)/profiling_render_benchmark_on $(OUT)/profiling_render_benchmark_off
	$(OUT)/profiling_render_benchmark_off stopped 3000 "$(PROFILING_FONT)"
	$(OUT)/profiling_render_benchmark_on stopped 3000 "$(PROFILING_FONT)"
	$(OUT)/profiling_render_benchmark_on record 3000 "$(PROFILING_FONT)"
	$(OUT)/profiling_render_benchmark_on panel 3000 "$(PROFILING_FONT)"

$(OUT)/font_atlas_measure_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/font_prewarm_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/sokol_letter_spacing_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/font_points_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/glyph_placeholder_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/load_font_size_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/font_display_size_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/font_memory_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/headless_fallback_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/capture_perf_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
$(OUT)/raylib_fallback_test: CXXFLAGS_T += -DAFTERHOURS_TEST_FIXTURES=\"$(CURDIR)/fixtures\"
# Shares the examples' Kenney tree rather than checking in a second copy.
$(OUT)/models_test: CXXFLAGS_T += -DAFTERHOURS_MODEL_FIXTURE=\"$(realpath ../examples/catalog/3d/tree-large.glb)\"
$(OUT)/house_of_cards_test: CXXFLAGS_T += -DAFTERHOURS_MODELS_DIR=\"$(realpath ../examples/catalog/3d)\"
$(OUT)/instance_batcher_test: CXXFLAGS_T += -DAFTERHOURS_MODELS_DIR=\"$(realpath ../examples/catalog/3d)\"
$(OUT)/instance_batcher_persist_test: CXXFLAGS_T += -DAFTERHOURS_MODELS_DIR=\"$(realpath ../examples/catalog/3d)\"

$(OUT)/sokol_mouse_pre_init_test: ../examples/web/sokol_impl.cc
$(OUT)/sokol_mouse_pre_init_test: CXXFLAGS_T += -DAFTER_HOURS_USE_SOKOL -x objective-c++
$(OUT)/sokol_mouse_pre_init_test: LDFLAGS_T += $(SOKOL_FRAMEWORKS)
$(OUT)/sokol_mouse_pre_init_test: ../src/backends/sokol/backend.h
