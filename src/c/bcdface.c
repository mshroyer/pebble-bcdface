#include <pebble.h>

/* Windows and layers */

static Window *window = NULL;
static Layer *main_layer = NULL;
static TextLayer *date_layer = NULL;
static BitmapLayer *bt_layer = NULL;

/* Resources */

static GBitmap *bt_bitmap = NULL;

/* Runtime configuration */

#define CONFIG_STORAGE_KEY 1

/**
 * Configuration data
 *
 * This will be persisted to local storage, so any new fields should be
 * appended to the end of the struct for forward compatibility.
 */
typedef struct {
    /* Whether to notify when the phone disconnects */
    bool notify_disconnect;

    /* Whether to show seconds */
    bool second_tick;
} config_t;

static config_t current_config = {0};

/* Derived parameters */

typedef struct {
    /* Timer event unit */
    TimeUnits tick_unit;

    /* The radius of the dots and circles */
    int16_t dot_radius;

    /* Pixel offset of the first column */
    int16_t col_offset;

    /* Pixel spacing between columns */
    int16_t col_spacing;
} derived_params_t;

static derived_params_t derived_params = {0};

/* Runtime state */

#define DATE_STR_SZ 11

static char date_str[DATE_STR_SZ];
static bool last_bt_state = false;
static bool window_visible = false;

/*** Configuration ************************************************************/

/**
 * Return the default configuration.
 *
 * This should be kept in sync with the default values in config.js.
 */
static config_t default_config() {
    return (config_t){
        .notify_disconnect = true,
        .second_tick = false,
    };
}

/**
 * Calculate and save derived values based on current configuration.
 */
static derived_params_t compute_derived_params(const config_t *config) {
    derived_params_t result = {
        .tick_unit = MINUTE_UNIT,
        .dot_radius = 10,
        .col_spacing = 0,
        .col_offset = 0,
    };

    Layer *window_layer = window_get_root_layer(window);
    const GRect bounds = layer_get_bounds(window_layer);
    const int16_t num_cols = config->second_tick ? 6 : 4;

    if (config->second_tick) {
        result.tick_unit = SECOND_UNIT;
        result.dot_radius = 8;
    }

    result.col_spacing = (bounds.size.w - 2 * num_cols * result.dot_radius) / (num_cols + 1);
    result.col_offset =
        (bounds.size.w - result.col_spacing * (num_cols - 1) - 2 * num_cols * result.dot_radius) /
        2;

    return result;
}

/**
 * Parse a message from the phone app into a config_t struct.
 */
static config_t parse_config_message(const DictionaryIterator *iter) {
    config_t result = default_config();

    Tuple *seconds_tuple = dict_find(iter, MESSAGE_KEY_SecondTick);
    if (seconds_tuple) {
        result.second_tick = seconds_tuple->value->int32 == 1;
    }

    Tuple *bt_tuple = dict_find(iter, MESSAGE_KEY_NotifyDisconnect);
    if (bt_tuple) {
        result.notify_disconnect = bt_tuple->value->int32 == 1;
    }

    return result;
}

/**
 * Apply a new configuration and its derived values.
 */
static void apply_config(const config_t *config) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Applying configuration");

    current_config = *config;
    derived_params = compute_derived_params(config);
}

/**
 * Load a persisted config from storage on the watch.
 */
static void load_config() {
    current_config = default_config();

    if (!persist_exists(CONFIG_STORAGE_KEY)) {
        APP_LOG(APP_LOG_LEVEL_INFO, "No persisted config found, using default");
        return;
    }

    int persisted_size = persist_get_size(CONFIG_STORAGE_KEY);
    if ((size_t)persisted_size > sizeof(current_config)) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Persisted config larger than ours! Using default instead");
        return;
    }

    if (persist_read_data(CONFIG_STORAGE_KEY, &current_config, sizeof(current_config)) !=
        persisted_size) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Error loading persisted config! Restoring default");
        current_config = default_config();
        return;
    }

    APP_LOG(APP_LOG_LEVEL_INFO, "Loaded persisted config");
}

/*** Drawing ******************************************************************/

/**
 * Draw a single BCD digit.
 */
static void draw_digit(Layer *layer, GContext *ctx, int col, int bits, int val) {
    const GRect bounds = layer_get_bounds(layer);
    const int16_t x_coord = derived_params.col_offset + derived_params.dot_radius +
                            (2 * derived_params.dot_radius + derived_params.col_spacing) * col;
    GPoint point;
    int i;

    for (i = 0; i < bits; i++) {
        point = GPoint(x_coord, bounds.size.h - derived_params.dot_radius * (3 * i + 2));
        if (val & 1) {
            graphics_fill_circle(ctx, point, derived_params.dot_radius);
        } else {
            graphics_draw_circle(ctx, point, derived_params.dot_radius);
        }

        val >>= 1;
    }
}

static void main_layer_update_proc(Layer *layer, GContext *ctx) {
    time_t t = time(NULL);
    struct tm *now = localtime(&t);

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorWhite);

    draw_digit(layer, ctx, 0, 2, now->tm_hour / 10);
    draw_digit(layer, ctx, 1, 4, now->tm_hour % 10);
    draw_digit(layer, ctx, 2, 3, now->tm_min / 10);
    draw_digit(layer, ctx, 3, 4, now->tm_min % 10);
    if (current_config.second_tick) {
        draw_digit(layer, ctx, 4, 3, now->tm_sec / 10);
        draw_digit(layer, ctx, 5, 4, now->tm_sec % 10);
    }
}

/*** Event handlers ***********************************************************/

static void subscribe_ui_event_handlers();
static void manually_invoke_ui_event_handlers();

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(main_layer);
    strftime(date_str, DATE_STR_SZ, "%a %b %d", tick_time);
    text_layer_set_text(date_layer, date_str);
}

static void handle_bt(bool bt_state) {
    if (current_config.notify_disconnect) {
        if (last_bt_state && !bt_state) {
            vibes_double_pulse();
        }
        layer_set_hidden(bitmap_layer_get_layer(bt_layer), bt_state);
    }
    last_bt_state = bt_state;
}

static void handle_inbox_received(DictionaryIterator *iter, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "handle_inbox_received callback");

    config_t new_config = parse_config_message(iter);
    apply_config(&new_config);

    if (window_visible) {
        subscribe_ui_event_handlers();
        manually_invoke_ui_event_handlers();
    }

    if (persist_write_data(CONFIG_STORAGE_KEY, &current_config, sizeof(current_config)) !=
        sizeof(current_config)) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Error persisting config!");
    } else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Successfully persisted config");
    }
}

static void handle_inbox_dropped(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Dropped inbox message, reason = %d", reason);
}

/**
 * Idempotentally subscribe to the timer and BT event handlers.
 *
 * This may be invoked to change handler subscriptions without explicitly
 * unsubscribing first.
 */
static void subscribe_ui_event_handlers() {
    tick_timer_service_subscribe(derived_params.tick_unit, handle_tick);
    bluetooth_connection_service_subscribe(handle_bt);
}

/**
 * Manually invoke the UI event handlers.
 */
static void manually_invoke_ui_event_handlers() {
    const time_t now_time = time(NULL);

    handle_tick(localtime(&now_time), derived_params.tick_unit);
    if (current_config.notify_disconnect) {
        handle_bt(bluetooth_connection_service_peek());
    } else {
        layer_set_hidden(bitmap_layer_get_layer(bt_layer), true);
    }
}

/*** App lifecycle callbacks **************************************************/

static void window_load(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_load callback");

    derived_params = compute_derived_params(&current_config);

    Layer *window_layer = window_get_root_layer(window);
    const GRect bounds = layer_get_bounds(window_layer);

    main_layer = layer_create((GRect){.origin = {0, 0}, .size = {bounds.size.w, bounds.size.h}});
    date_layer = text_layer_create((GRect){.origin = {0, 0}, .size = {bounds.size.w, 40}});
    bt_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PHONE);
    bt_layer = bitmap_layer_create((GRect){.origin = {0, 6}, .size = {20, 20}});
    bitmap_layer_set_bitmap(bt_layer, bt_bitmap);

    layer_set_update_proc(main_layer, main_layer_update_proc);
    layer_add_child(window_layer, main_layer);
    layer_add_child(window_layer, text_layer_get_layer(date_layer));
    layer_add_child(window_layer, bitmap_layer_get_layer(bt_layer));

    text_layer_set_background_color(date_layer, GColorBlack);
    text_layer_set_text_color(date_layer, GColorWhite);
    text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
    text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
}

static void window_appear(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_appear callback");

    window_visible = true;
    subscribe_ui_event_handlers();

    /*
     * Force immediate redraw so there isn't an annoying pause before
     * the date string becomes visible when returning to the watch
     * face.  This call is also necessary so that "now" gets set before
     * the first run of main_layer_update_proc()
     */
    manually_invoke_ui_event_handlers();
}

static void window_disappear(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_disappear callback");

    window_visible = false;

    bluetooth_connection_service_unsubscribe();
    tick_timer_service_unsubscribe();
}

static void window_unload(Window *window) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "window_unload callback");

    bitmap_layer_destroy(bt_layer);
    gbitmap_destroy(bt_bitmap);
    text_layer_destroy(date_layer);
    layer_destroy(main_layer);
}

/*** Main *********************************************************************/

static void init() {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "init callback");

    load_config();

    app_message_register_inbox_received(handle_inbox_received);
    app_message_register_inbox_dropped(handle_inbox_dropped);
    app_message_open(APP_MESSAGE_INBOX_SIZE_MINIMUM, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);

    window = window_create();
    window_set_window_handlers(window, (WindowHandlers){
                                           .load = window_load,
                                           .appear = window_appear,
                                           .disappear = window_disappear,
                                           .unload = window_unload,
                                       });
    window_set_background_color(window, GColorBlack);
    window_stack_push(window, true);
}

static void deinit() {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit callback");

    window_destroy(window);
}

int main() {
    init();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();

    return 0;
}
