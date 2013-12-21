#include <pebble.h>

enum {
	AKEY_MODULE,
	AKEY_ACTION,
	AKEY_NAME,
	AKEY_ID,
};

static Window *window;
static TextLayer *textLayer;
static MenuLayer *menuLayer;

#define MAX_DEVICE_LIST_ITEMS (30)
#define MAX_DEVICE_NAME_LENGTH (16)

typedef struct {
	int id;
	char name[MAX_DEVICE_NAME_LENGTH+1];
	int methods;
} Device;

static Device s_device_list_items[MAX_DEVICE_LIST_ITEMS];
static int s_device_count = 0;


void out_sent_handler(DictionaryIterator *sent, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message was delivered");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message failed");
}

void setDevice(int id, char *name) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting device %i", id);
	if (s_device_count >= MAX_DEVICE_LIST_ITEMS) {
		return;
	}
	s_device_list_items[s_device_count].id = id;
	strncpy(s_device_list_items[s_device_count].name, name, MAX_DEVICE_NAME_LENGTH);
	s_device_count++;
}

void in_received_handler(DictionaryIterator *received, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "incoming message received");
	// Check for fields you expect to receive
	Tuple *moduleTuple = dict_find(received, AKEY_MODULE);
	Tuple *actionTuple = dict_find(received, AKEY_ACTION);
	if (!moduleTuple || !actionTuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Malformed message");
		return;
	}
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Got module: %s, action: %s", moduleTuple->value->cstring, actionTuple->value->cstring);
	if (strcmp(moduleTuple->value->cstring, "auth") == 0) {
		if (strcmp(actionTuple->value->cstring, "request") == 0) {
			text_layer_set_text(textLayer, "Please log in on your phone");
		} else if (strcmp(actionTuple->value->cstring, "authenticating") == 0) {
			text_layer_set_text(textLayer, "Authenticating...");
		} else if (strcmp(actionTuple->value->cstring, "done") == 0) {
			text_layer_set_text(textLayer, "You are now logged in, more to come...");
			layer_set_hidden(text_layer_get_layer(textLayer), true);
			layer_set_hidden(menu_layer_get_layer(menuLayer), false);
		} else if (strcmp(actionTuple->value->cstring, "clear") == 0) {
			s_device_count = 0;
			layer_set_hidden(text_layer_get_layer(textLayer), false);
			layer_set_hidden(menu_layer_get_layer(menuLayer), true);
		}
	} else if (strcmp(moduleTuple->value->cstring, "device") == 0) {
		Tuple *idTuple = dict_find(received, AKEY_ID);
		Tuple *nameTuple = dict_find(received, AKEY_NAME);
		if (!idTuple || !nameTuple) {
			return;
		}
		setDevice(idTuple->value->int8, nameTuple->value->cstring);
		menu_layer_reload_data(menuLayer);
	}
}

void in_dropped_handler(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "incoming message dropped");
}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
	return 44;
}

static void draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, void *data) {
	Device* device;
	const int index = cell_index->row;

	/*if ((item = get_todo_list_item_at_index(index)) == NULL) {
		return;
	}*/
	device = &s_device_list_items[index];

	menu_cell_basic_draw(ctx, cell_layer, device->name, NULL, NULL);
}

static uint16_t get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *data) {
	return s_device_count;
}

static void select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
	Device* device = &s_device_list_items[cell_index->row];
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Select callback, index: %i", cell_index->row);
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	Tuplet module = TupletCString(AKEY_MODULE, "device");
	dict_write_tuplet(iter, &module);
	Tuplet action = TupletCString(AKEY_ACTION, "select");
	dict_write_tuplet(iter, &action);
	Tuplet id = TupletInteger(AKEY_ID, device->id);
	dict_write_tuplet(iter, &id);
	app_message_outbox_send();
}

static void select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Select long callback, index: %i", cell_index->row);
}

static void window_load(Window *window) {
	Layer *windowLayer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(windowLayer);
	GRect frame = layer_get_frame(windowLayer);

	textLayer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 40 } });
	text_layer_set_text(textLayer, "Loading...");
	text_layer_set_text_alignment(textLayer, GTextAlignmentCenter);
	text_layer_set_overflow_mode(textLayer, GTextOverflowModeWordWrap);
	layer_add_child(windowLayer, text_layer_get_layer(textLayer));

	menuLayer = menu_layer_create(frame);
	menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks) {
		.get_cell_height = (MenuLayerGetCellHeightCallback) get_cell_height_callback,
		.draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
		.get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) get_num_rows_callback,
		.select_click = (MenuLayerSelectCallback) select_callback,
		.select_long_click = (MenuLayerSelectCallback) select_long_callback
	});
	menu_layer_set_click_config_onto_window(menuLayer, window);
	layer_set_hidden(menu_layer_get_layer(menuLayer), true);
	layer_add_child(windowLayer, menu_layer_get_layer(menuLayer));
}

static void window_unload(Window *window) {
	text_layer_destroy(textLayer);
}

static void init(void) {
	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	const bool animated = true;
	window_stack_push(window, animated);

	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);

	const uint32_t inbound_size = 128;
	const uint32_t outbound_size = 128;
	app_message_open(inbound_size, outbound_size);
}

static void deinit(void) {
	window_destroy(window);
}

int main(void) {
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

	app_event_loop();
	deinit();
}
