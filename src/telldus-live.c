#include <pebble.h>

enum {
	AKEY_MODULE,
	AKEY_ACTION,
};

static Window *window;
static TextLayer *text_layer;

void out_sent_handler(DictionaryIterator *sent, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message was delivered");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message failed");
}

void in_received_handler(DictionaryIterator *received, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "incoming message received");
	// Check for fields you expect to receive
	Tuple *moduleTuple = dict_find(received, AKEY_MODULE);
	Tuple *actionTuple = dict_find(received, AKEY_ACTION);
	if (moduleTuple && actionTuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Got module: %s, action: %s", moduleTuple->value->cstring, actionTuple->value->cstring);
		if (strcmp(moduleTuple->value->cstring, "auth") == 0) {
			if (strcmp(actionTuple->value->cstring, "request") == 0) {
				text_layer_set_text(text_layer, "Please log in on your phone");
			} else if (strcmp(actionTuple->value->cstring, "authenticating") == 0) {
				text_layer_set_text(text_layer, "Authenticating...");
			} else if (strcmp(actionTuple->value->cstring, "done") == 0) {
				text_layer_set_text(text_layer, "You are now logged in, more to come...");
			}
		}
	} else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Malformed message");
	}
}

void in_dropped_handler(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "incoming message dropped");
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	Tuplet value = TupletCString(0, "configuration");
	dict_write_tuplet(iter, &value);
	app_message_outbox_send();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	text_layer_set_text(text_layer, "Up");
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	text_layer_set_text(text_layer, "Down");
}

static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
	window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	text_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 40 } });
	text_layer_set_text(text_layer, "Loading...");
	text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
	text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);
	layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
	text_layer_destroy(text_layer);
}

static void init(void) {
	window = window_create();
	window_set_click_config_provider(window, click_config_provider);
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

	const uint32_t inbound_size = 64;
	const uint32_t outbound_size = 64;
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
