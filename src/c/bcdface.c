#include <pebble.h>

#ifdef SHOW_SECONDS
#define NUM_COLUMNS 6
#define RADIUS 8
#define TICK_UNIT SECOND_UNIT
#else
#define NUM_COLUMNS 4
#define RADIUS 10
#define TICK_UNIT MINUTE_UNIT
#endif

#define DATE_STR_SZ 11


/*** Windows and layers ***/
static Window *window = NULL;
static Layer *main_layer = NULL;
static TextLayer *date_layer = NULL;
static BitmapLayer *bt_layer = NULL;

/*** Resources ***/
static GBitmap *bt_bitmap = NULL;

/*** Mutable configuration ***/
/* Whether to display seconds */
static bool config_seconds = false;
/* Whether to notify on Bluetooth disconnect */
static bool config_bt = false;

/*** Derived values ***/
static int16_t dot_radius;
static int16_t col_offset, col_spacing;

/*** Runtime state ***/
static char date_str[DATE_STR_SZ];
static bool last_bt_state = false;


static void draw_digit(Layer *layer, GContext *ctx,
                       int col, int bits, int val)
{
	const GRect bounds = layer_get_bounds(layer);
	const int16_t x_coord = col_offset + dot_radius +
		(2 * dot_radius + col_spacing) * col;
	GPoint point;
	int i;

	for (i = 0; i < bits; i++) {
		point = GPoint(x_coord, bounds.size.h - dot_radius * (3 * i + 2));
		if (val & 1) {
			graphics_fill_circle(ctx, point, dot_radius);
		} else {
			graphics_draw_circle(ctx, point, dot_radius);
		}

		val >>= 1;
	}
}

static void update_proc(Layer *layer, GContext *ctx)
{
	time_t t = time(NULL);
	struct tm *now = localtime(&t);

	graphics_context_set_stroke_color(ctx, GColorWhite);
	graphics_context_set_fill_color(ctx, GColorWhite);

	draw_digit(layer, ctx, 0, 2, now->tm_hour / 10);
	draw_digit(layer, ctx, 1, 4, now->tm_hour % 10);
	draw_digit(layer, ctx, 2, 3, now->tm_min  / 10);
	draw_digit(layer, ctx, 3, 4, now->tm_min  % 10);
	if (config_seconds) {
		draw_digit(layer, ctx, 4, 3, now->tm_sec  / 10);
		draw_digit(layer, ctx, 5, 4, now->tm_sec  % 10);
	}
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed)
{
	layer_mark_dirty(main_layer);
	strftime(date_str, DATE_STR_SZ, "%a %b %d", tick_time);
	text_layer_set_text(date_layer, date_str);
}

#ifdef NOTIFY_DISCONNECT

static void handle_bt(bool bt_state)
{
	if (last_bt_state && !bt_state) {
		vibes_double_pulse();
	}

	last_bt_state = bt_state;
	layer_set_hidden(bitmap_layer_get_layer(bt_layer), bt_state);
}

#endif /* defined NOTIFY_DISCONNECT */

/***
 * Calculate and save derived values based on current configuration.
 */
static void update_derived() {
	Layer *window_layer = window_get_root_layer(window);
	const GRect bounds = layer_get_bounds(window_layer);
	const int16_t num_cols = config_seconds ? 6 : 4;

	if (config_seconds) {
		dot_radius = 8;
	} else {
		dot_radius = 10;
	}

	col_spacing = (bounds.size.w - 2 * num_cols * dot_radius) /
		(num_cols + 1);
	col_offset = (bounds.size.w - col_spacing * (num_cols - 1) -
		      2 * num_cols * dot_radius) / 2;
}

static void window_load(Window *window) {
	update_derived();

	Layer *window_layer = window_get_root_layer(window);
	const GRect bounds = layer_get_bounds(window_layer);

	main_layer = layer_create((GRect) {
		.origin = { 0, 0 },
		.size = { bounds.size.w, bounds.size.h }
	});
	date_layer = text_layer_create((GRect) {
		.origin = { 0, 0 },
		.size = { bounds.size.w, 40 }
	});
#ifdef NOTIFY_DISCONNECT
	bt_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PHONE);
	bt_layer = bitmap_layer_create((GRect) {
		.origin = { 0, 6 },
		.size = { 20, 20 }
	});
	bitmap_layer_set_bitmap(bt_layer, bt_bitmap);
#endif

	layer_set_update_proc(main_layer, update_proc);
	layer_add_child(window_layer, main_layer);
	layer_add_child(window_layer, text_layer_get_layer(date_layer));
#ifdef NOTIFY_DISCONNECT
	layer_add_child(window_layer, bitmap_layer_get_layer(bt_layer));
#endif

	text_layer_set_background_color(date_layer, GColorBlack);
	text_layer_set_text_color(date_layer, GColorWhite);
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
}

static void window_appear(Window *window) {
	const time_t now_time = time(NULL);

	tick_timer_service_subscribe(TICK_UNIT, handle_tick);

	/*
	 * Force immediate redraw so there isn't an annoying pause before
	 * the date string becomes visible when returning to the watch
	 * face.  This call is also necessary so that "now" gets set before
	 * the first run of update_proc()
	 */
	handle_tick(localtime(&now_time), TICK_UNIT);

#ifdef NOTIFY_DISCONNECT
	bluetooth_connection_service_subscribe(handle_bt);
	handle_bt(bluetooth_connection_service_peek());
#endif
}

static void window_disappear(Window *window) {
#ifdef NOTIFY_DISCONNECT
	bluetooth_connection_service_unsubscribe();
#endif
	tick_timer_service_unsubscribe();
}

static void window_unload(Window *window) {
#ifdef NOTIFY_DISCONNECT
	bitmap_layer_destroy(bt_layer);
	gbitmap_destroy(bt_bitmap);
#endif
	text_layer_destroy(date_layer);
	layer_destroy(main_layer);
}

static void init(void) {
	/* date_str = malloc(DATE_STR_SZ); */
	/* now = malloc(sizeof(struct tm)); */

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.appear = window_appear,
		.disappear = window_disappear,
		.unload = window_unload,
	});
	window_set_background_color(window, GColorBlack);
	window_stack_push(window, true);
}

static void deinit(void) {
	window_destroy(window);
	/* free(date_str); */
	/* free(now); */
}

int main(void) {
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

	app_event_loop();
	deinit();

	return 0;
}
