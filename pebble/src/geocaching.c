#include "pebble.h"

// ADDED GPath point definitions for arrow and north marker
static const GPathInfo ARROW_POINTS =
{
  14,
  (GPoint []) {
    {0, -30},
    {10, -20},
    {6, -16},
    {3, -20},
    {3, 10},
    {7, 15},
    {7, 30},
    {0, 22},
    {-7, 30},
    {-7, 15},
    {-3, 10},
    {-3, -20},
    {-6, -16},
    {-10, -20},
  }
};
static const GPathInfo NORTH_POINTS =
{
  3,
  (GPoint []) {
    {0, -40},
    {6, -31},
    {-6, -31},
  }
};

// updated to match new Adroid app and added DECLINATION_KEY for pebblekit JS
enum GeoKey {
  DISTANCE_KEY = 0x0,
  BEARING_INDEX_KEY = 0x1,
  EXTRAS_KEY = 0x2,
  DT_RATING_KEY = 0x3,
  GC_NAME_KEY = 0x4,
  GC_CODE_KEY = 0x5,
  GC_SIZE_KEY = 0x6,
  AZIMUTH_KEY = 0x7,
  DECLINATION_KEY = 0x8,
};

Window *window;

// deleted BitmapLayer and GBitmap definition

// ADDED GPath definitions
Layer *arrow_layer;
GPath *arrow;
GPath *north;
GRect arrow_bounds;
GPoint arrow_centre;

int16_t compass_heading = 0;
int16_t north_heading = 0;
int16_t bearing = 0;
uint8_t status;
bool rot_arrow = false;
bool gotdecl = false;
int16_t declination = 0;

TextLayer *text_distance_layer;
TextLayer *text_time_layer;
Layer *line_layer;

static uint8_t data_display = 0;

static AppSync sync;
static uint8_t sync_buffer[150];

#ifdef PBL_COLOR

#define COLOR_SCHEME_KEY 1
#define DEFAULT_COLOR_SCHEME 1
#define CUSTOM_SCHEME_KEY 10

uint8_t color_mode = 0; // 0-off 1-change scheme 2-change custom
const uint8_t NUM_SCHEMES = 10; // black/rainbow, white/rainbow, blue, red, dark green, light green, yellow, orange, pink
uint8_t color_scheme = DEFAULT_COLOR_SCHEME;
static char* scheme_name[] = {
  "custom",
  "blk/rainbow",
  "wht/rainbow",
  "blue",
  "red",
  "drk green",
  "lgt green",
  "yellow",
  "orange",
  "pink",
};

uint8_t col_back[] = {192, 192, 255, 198, 241, 196, 238, 252, 248, 243, };
uint8_t col_arrow[] = {255, 248, 248, 203, 224, 232, 232, 250, 246, 251, };
uint8_t col_comp[] = {255, 252, 252, 203, 225, 236, 216, 248, 244, 247, };
uint8_t col_dist[] = {255, 204, 204, 203, 225, 236, 216, 248, 244, 247, };
uint8_t col_line[] = {255, 195, 195, 203, 224, 232, 232, 250, 246, 251, };
uint8_t col_time[] = {255, 227, 227, 203, 225, 236, 216, 248, 244, 247, };

uint8_t change_color = 0;
static char* color_name[] = {
  "background",
  "N arrow",
  "compass",
  "distance",
  "line",
  "time",
};

void config_buttons_provider1(void *context);
void config_buttons_provider2(void *context);
#endif

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);
void config_buttons_provider(void *context);

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {

  switch (key) {

    case DISTANCE_KEY:
      text_layer_set_text(text_distance_layer, new_tuple->value->cstring);
      rot_arrow = (strncmp(text_layer_get_text(text_distance_layer), "NO", 2) != 0) ? true : false;
      break;

    case AZIMUTH_KEY:
      bearing = new_tuple->value->int16;
      layer_mark_dirty(arrow_layer);
      break;
    
    case DECLINATION_KEY:
      declination = new_tuple->value->int16;
      gotdecl = true;
      layer_mark_dirty(arrow_layer);
      break;
  }
}

// Draw line between geocaching data and time
void line_layer_update_callback(Layer *layer, GContext* ctx) {
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, (GColor){.argb = col_line[color_scheme]});
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
#endif
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

// NEW compass handler
void handle_compass(CompassHeadingData heading_data){
/*
  static char buf[64];
  snprintf(buf, sizeof(buf), " %d°", declination);
  text_layer_set_text(text_distance_layer, buf);
*/
  north_heading = TRIGANGLE_TO_DEG(heading_data.magnetic_heading) - declination;
  if (north_heading >= 360) north_heading -= 360;
  if (north_heading < 0) north_heading += 360;
  status = heading_data.compass_status;
  layer_mark_dirty(arrow_layer);
}

// NEW arrow layer update handler
void arrow_layer_update_callback(Layer *path, GContext *ctx) {
  
#ifdef PBL_COLOR
#else
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);
#endif
  
  compass_heading = bearing + north_heading;
  if (compass_heading >= 360) {
    compass_heading = compass_heading - 360;
  }
  
// Don't rotate the arrow if we have NO GPS or NO BT in the distance box
  if (rot_arrow) {
    gpath_rotate_to(arrow, compass_heading * TRIG_MAX_ANGLE / 360);
  }
  
// draw outline north arrow if pebble compass is fine calibrating, solid if good
  gpath_rotate_to(north, north_heading * TRIG_MAX_ANGLE / 360);
  if (!gotdecl) {
    #ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx, GColorRed);
      gpath_draw_filled(ctx, north);
    #else
      gpath_draw_outline(ctx, north);
    #endif
  } else {
    #ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx, (GColor){.argb = col_arrow[color_scheme]});
    #endif
    gpath_draw_filled(ctx, north);
  }

// draw outline arrow if pebble compass is invalid, solid if OK
  if (status == 0) {
    #ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx, GColorRed);
      gpath_draw_filled(ctx, arrow);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, arrow_centre, 1);
    #else
      gpath_draw_outline(ctx, arrow);
      graphics_fill_circle(ctx, arrow_centre, 1);
    #endif
  } else {
    #ifdef PBL_COLOR
      if (status == 1) {
        graphics_context_set_fill_color(ctx, GColorYellow);
      } else {
        graphics_context_set_fill_color(ctx, (GColor){.argb = col_arrow[color_scheme]});
      }
      gpath_draw_filled(ctx, arrow);
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_circle(ctx, arrow_centre, 1);
    #else
      gpath_draw_filled(ctx, arrow);
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_circle(ctx, arrow_centre, 1);
      graphics_context_set_fill_color(ctx, GColorWhite);
    #endif
    }
}

void bluetooth_connection_changed(bool connected) {

  if (!connected) {

// ADDED text instead of bitmap
    text_layer_set_text(text_distance_layer, "NO BT");
    //vibes_short_pulse();

  } else {
    const Tuple *tuple = app_sync_get(&sync, DISTANCE_KEY);
    text_layer_set_text(text_distance_layer, (tuple == NULL) ? "NO GPS" : tuple->value->cstring);
  }
  if (strncmp(text_layer_get_text(text_distance_layer), "NO", 2) != 0) {
    rot_arrow = true;
    #ifdef PBL_COLOR
      text_layer_set_text_color(text_distance_layer, (GColor){.argb = col_dist[color_scheme]});
    #endif
  } else {
    rot_arrow = false;
    #ifdef PBL_COLOR
      text_layer_set_text_color(text_distance_layer, GColorRed);
    #endif
  } 
}

bool have_additional_data(){
  const Tuple *tuple = app_sync_get(&sync, EXTRAS_KEY);
  return (tuple == NULL || tuple->value->uint8 == 0)? false : true;
}

void up_click_handler(ClickRecognizerRef recognizer, void *context) {

  data_display++;
  data_display %= 5;

  if ( !have_additional_data() ) data_display = 0;

  if(data_display == 0){
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  } else if(data_display == 1){

    tick_timer_service_unsubscribe();
    const Tuple *tuple = app_sync_get(&sync, GC_NAME_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 2){

    const Tuple *tuple = app_sync_get(&sync, GC_CODE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 3){

    const Tuple *tuple = app_sync_get(&sync, GC_SIZE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 4){

    const Tuple *tuple = app_sync_get(&sync, DT_RATING_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  }

}

void down_click_handler(ClickRecognizerRef recognizer, void *context) {

  if(data_display == 0) data_display = 5;
  data_display--;

  if ( !have_additional_data() ) data_display = 0;

  if(data_display == 0){
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

  } else if(data_display == 1){

    const Tuple *tuple = app_sync_get(&sync, GC_NAME_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 2){

    const Tuple *tuple = app_sync_get(&sync, GC_CODE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 3){

    const Tuple *tuple = app_sync_get(&sync, GC_SIZE_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  } else if(data_display == 4){

    tick_timer_service_unsubscribe();

    const Tuple *tuple = app_sync_get(&sync, DT_RATING_KEY);
    const char *gc_data = tuple == NULL ? "" : tuple->value->cstring;
    text_layer_set_text(text_time_layer, *gc_data ? gc_data : "??");

  }

}

#ifdef PBL_COLOR

void color_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  switch (change_color) {
    case 0:
      col_back[0]--;
      if (col_back[0] < 192) col_back[0] = 255;
      break;
    case 1:
      col_arrow[0]--;
      if (col_arrow[0] < 192) col_arrow[0] = 255;
      break;
    case 2:
      col_comp[0]--;
      if (col_comp[0] < 192) col_comp[0] = 255;
      break;
    case 3:
      col_dist[0]--;
      if (col_dist[0] < 192) col_dist[0] = 255;
      break;
    case 4:
      col_line[0]--;
      if (col_line[0] < 192) col_line[0] = 255;
      break;
    case 5:
      col_time[0]--;
      if (col_time[0] < 192) col_time[0] = 255;
      break;
  }
  window_set_background_color(window, (GColor){.argb = col_back[color_scheme]});
  text_layer_set_text_color(text_distance_layer, (GColor){.argb = col_dist[color_scheme]});
  text_layer_set_text_color(text_time_layer, (GColor){.argb = col_time[color_scheme]});
  layer_mark_dirty(arrow_layer);
}
void color_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  char message[] = "XXXXXXXXXXXXXXXXX";
  snprintf(message, sizeof(message), "%d", change_color);
  APP_LOG(APP_LOG_LEVEL_INFO, message);

  switch (change_color) {
    case 0:
      col_back[0]++;
      if (col_back[0] == 0) col_back[0] = 192;
      break;
    case 1:
      col_arrow[0]++;
      if (col_arrow[0] == 0) col_arrow[0] = 192;
      break;
    case 2:
      col_comp[0]++;
      if (col_comp[0] == 0) col_comp[0] = 192;
      break;
    case 3:
      col_dist[0]++;
      if (col_dist[0] == 0) col_dist[0] = 192;
      break;
    case 4:
      col_line[0]++;
      if (col_line[0] == 0) col_line[0] = 192;
      break;
    case 5:
      col_time[0]++;
      if (col_time[0] == 0) col_time[0] = 192;
      break;
  }
  window_set_background_color(window, (GColor){.argb = col_back[color_scheme]});
  text_layer_set_text_color(text_distance_layer, (GColor){.argb = col_dist[color_scheme]});
  text_layer_set_text_color(text_time_layer, (GColor){.argb = col_time[color_scheme]});
  layer_mark_dirty(arrow_layer);
}
void color_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  change_color++;
  if (change_color > 5) {
    color_mode = 0;
    text_layer_set_text(text_distance_layer, "NO GPS");
    window_set_click_config_provider(window, config_buttons_provider);
  }
  text_layer_set_text(text_time_layer, color_name[change_color]);
}
  
void scheme_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  color_scheme--;
  if (color_scheme == 255) color_scheme = NUM_SCHEMES - 1;
  text_layer_set_text(text_time_layer, scheme_name[color_scheme]);
  window_set_background_color(window, (GColor){.argb = col_back[color_scheme]});
  text_layer_set_text_color(text_distance_layer, (GColor){.argb = col_dist[color_scheme]});
  text_layer_set_text_color(text_time_layer, (GColor){.argb = col_time[color_scheme]});
  layer_mark_dirty(arrow_layer);
}
void scheme_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  color_scheme++;
  if (color_scheme >= NUM_SCHEMES) color_scheme = 0;
  text_layer_set_text(text_time_layer, scheme_name[color_scheme]);
  window_set_background_color(window, (GColor){.argb = col_back[color_scheme]});
  text_layer_set_text_color(text_distance_layer, (GColor){.argb = col_dist[color_scheme]});
  text_layer_set_text_color(text_time_layer, (GColor){.argb = col_time[color_scheme]});
  layer_mark_dirty(arrow_layer);
}
void scheme_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (color_scheme != 0) {
    color_mode = 0;
    text_layer_set_text(text_distance_layer, "NO GPS");
    window_set_click_config_provider(window, config_buttons_provider);
  } else { // custom color scheme
    change_color = 0;
    text_layer_set_text(text_distance_layer, "custom");
    text_layer_set_text(text_time_layer, color_name[change_color]);
    window_set_click_config_provider(window, config_buttons_provider2);
  }
}

void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  color_mode++;
  char message[] = "XXXXXXXXXXXXXXXXX";
  snprintf(message, sizeof(message), "%d", color_mode);
  APP_LOG(APP_LOG_LEVEL_INFO, message);
  if (color_mode > 1) color_mode = 0;
  switch (color_mode) {
    case 0: // color select off
      text_layer_set_text(text_distance_layer, "NO GPS");
      window_set_click_config_provider(window, config_buttons_provider);
      break;
    case 1: // color scheme select
      text_layer_set_text(text_distance_layer, "scheme");
      text_layer_set_text(text_time_layer, scheme_name[color_scheme]);
      window_set_click_config_provider(window, config_buttons_provider1);
      break;
    case 2: // custom colors select
      text_layer_set_text(text_distance_layer, "colors");
      text_layer_set_text(text_time_layer, color_name[change_color]);
      window_set_click_config_provider(window, config_buttons_provider2);
      break;
  }
}

void config_buttons_provider1(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, scheme_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, scheme_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, scheme_select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);
}
void config_buttons_provider2(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, color_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, color_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, color_select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);
}

#endif

void config_buttons_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);

  #ifdef PBL_COLOR
    window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);
    window_single_click_subscribe(BUTTON_ID_SELECT, NULL);
  #endif
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
// UPDATED to include date
  static char time_text[] = "XXX XX 00:00";

  char *time_format;

// UPDATED to include date
  if (clock_is_24h_style()) {
    time_format = "%b %e %R";
  } else {
    time_format = "%b %e %I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    text_layer_set_text(text_time_layer, time_text + 1);
  } else {
    text_layer_set_text(text_time_layer, time_text);
  }
}

void handle_init(void) {

  #ifdef PBL_COLOR
    color_scheme = persist_exists(COLOR_SCHEME_KEY) ? persist_read_int(COLOR_SCHEME_KEY) : DEFAULT_COLOR_SCHEME;
    if (persist_exists(CUSTOM_SCHEME_KEY)) {
      col_back[0] = persist_read_int(CUSTOM_SCHEME_KEY + 0);
      col_arrow[0] = persist_read_int(CUSTOM_SCHEME_KEY + 1);
      col_dist[0] = persist_read_int(CUSTOM_SCHEME_KEY + 2);
      col_line[0] = persist_read_int(CUSTOM_SCHEME_KEY + 3);
      col_time[0] = persist_read_int(CUSTOM_SCHEME_KEY + 4);
    }
  #endif

  window = window_create();
  #ifdef PBL_COLOR
    window_set_background_color(window, (GColor){.argb = col_back[color_scheme]});
  #else
    window_set_background_color(window, GColorBlack);
    window_set_fullscreen(window, true);
  #endif

  window_stack_push(window, true);

  Layer *window_layer = window_get_root_layer(window);

  // Initialize distance layout
  Layer *distance_holder = layer_create(GRect(0, 80, 144, 40));
  layer_add_child(window_layer, distance_holder);

  ResHandle roboto_36 = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_36);
  text_distance_layer = text_layer_create(GRect(0, 0, 144, 40));
  text_layer_set_background_color(text_distance_layer, GColorClear);
  #ifdef PBL_COLOR
    text_layer_set_text_color(text_distance_layer, (GColor){.argb = col_dist[color_scheme]});
  #else
    text_layer_set_text_color(text_distance_layer, GColorWhite);
  #endif
  text_layer_set_text_alignment(text_distance_layer, GTextAlignmentCenter);
  text_layer_set_font(text_distance_layer, fonts_load_custom_font(roboto_36));
  layer_add_child(distance_holder, text_layer_get_layer(text_distance_layer));

  // Initialize time layout
// Size adjusted so the date and time will fit
  Layer *date_holder = layer_create(GRect(0, 132, 144, 36));
  layer_add_child(window_layer, date_holder);

  line_layer = layer_create(GRect(8, 0, 144-16, 2));
  layer_set_update_proc(line_layer, line_layer_update_callback);
  layer_add_child(date_holder, line_layer);

  ResHandle roboto_22 = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_22);
  text_time_layer = text_layer_create(GRect(0, 2, 144, 32));
  text_layer_set_background_color(text_time_layer, GColorClear);
  #ifdef PBL_COLOR
    text_layer_set_text_color(text_time_layer, (GColor){.argb = col_time[color_scheme]});
  #else
    text_layer_set_text_color(text_time_layer, GColorWhite);
  #endif
  text_layer_set_text_alignment(text_time_layer, GTextAlignmentCenter);
  text_layer_set_font(text_time_layer, fonts_load_custom_font(roboto_22));
  layer_add_child(date_holder, text_layer_get_layer(text_time_layer));

  // Initialize compass layout
  Layer *compass_holder = layer_create(GRect(0, 0, 144, 79));
  layer_add_child(window_layer, compass_holder);

// deleted bitmap layer create

// NEW definitions for Path layer, adjusted size due to a glitch drawing the North dot
  arrow_layer = layer_create(GRect(0, 0, 144, 79));
  layer_set_update_proc(arrow_layer, arrow_layer_update_callback);
  layer_add_child(compass_holder, arrow_layer);

// centrepoint added for use in rotations
  arrow_bounds = layer_get_frame(arrow_layer);
  arrow_centre = GPoint(arrow_bounds.size.w/2, arrow_bounds.size.h/2);

// Initialize and define the paths
  arrow = gpath_create(&ARROW_POINTS);
  gpath_move_to(arrow, arrow_centre);
  north = gpath_create(&NORTH_POINTS);
  gpath_move_to(north, arrow_centre);

  Tuplet initial_values[] = {
    TupletCString(DISTANCE_KEY, "NO GPS"),
    TupletInteger(BEARING_INDEX_KEY, 0),
    TupletInteger(EXTRAS_KEY, 0),
    TupletCString(DT_RATING_KEY, ""),
    TupletCString(GC_NAME_KEY, ""),
    TupletCString(GC_CODE_KEY, ""),
    TupletCString(GC_SIZE_KEY, ""),
    TupletInteger(AZIMUTH_KEY, 0),
    TupletInteger(DECLINATION_KEY, 0),
  };

  const int inbound_size = 150;
  const int outbound_size = 0;
  app_message_open(inbound_size, outbound_size);

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
                NULL, NULL);

  // Subscribe to notifications
  bluetooth_connection_service_subscribe(bluetooth_connection_changed);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
// compass service added
  compass_service_subscribe(handle_compass);
  compass_service_set_heading_filter(2 * (TRIG_MAX_ANGLE/360));

  window_set_click_config_provider(window, config_buttons_provider);

}

void handle_deinit(void) {

#ifdef PBL_COLOR
  persist_write_int(COLOR_SCHEME_KEY, color_scheme);
  persist_write_int(CUSTOM_SCHEME_KEY + 0, col_back[0]);
  persist_write_int(CUSTOM_SCHEME_KEY + 1, col_arrow[0]);
  persist_write_int(CUSTOM_SCHEME_KEY + 2, col_dist[0]);
  persist_write_int(CUSTOM_SCHEME_KEY + 3, col_line[0]);
  persist_write_int(CUSTOM_SCHEME_KEY + 4, col_time[0]);
#endif
  
// added unsubscribes
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
  compass_service_unsubscribe();

// added layer destroys
  text_layer_destroy(text_time_layer);
  layer_destroy(line_layer);
  text_layer_destroy(text_distance_layer);
  gpath_destroy(arrow);
  layer_destroy(arrow_layer);
  window_destroy(window);

}

int main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}