/*

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.

*/

#ifndef SI3S_MAIN_C
#define SI3S_MAIN_C

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <time.h>

#include <json-c/json.h>

#define UNIX_TIME_STRING_SIZE (64)
#define HUMAN_TIME_STRING_SIZE (64)
#define BATTERY_PERCENTAGE_STRING_SIZE (64)

#define BATTERY_PERCENTAGE_PARSING_BUFFER_SIZE (8)

#define ERROR_MESSAGE_BATTERY_READ ("[Battery read error]")

// Sanity check.
_Static_assert(
	(strlen(ERROR_MESSAGE_BATTERY_READ)+1) < BATTERY_PERCENTAGE_STRING_SIZE,
	"Expected `BATTERY_PERCENTAGE_STRING_SIZE` to be large enough to contain error messages."
);

static void sleep_ms(const unsigned int ms) {
	if (ms <= 0) {
		return;
	}

	const useconds_t microseconds = ms * 1000;

	usleep(microseconds);
}

static int is_valid_time(const time_t t) {
	return t != ((time_t)-1);
}

static long long int get_unix_time(const time_t timestamp) {
	return (long long int)timestamp;
}

enum battery_percentage_error {
	battery_percentage_error_none,
	battery_percentage_error_read,
	battery_percentage_error_unexpected_character,
	battery_percentage_error_invalid_length,
};

/*
	Read battery level and write it to `percentage`.
*/
static enum battery_percentage_error battery_percentage_get(int *percentage) {
	char raw_percentage[BATTERY_PERCENTAGE_PARSING_BUFFER_SIZE];
	for (int i = 0; i < BATTERY_PERCENTAGE_PARSING_BUFFER_SIZE; i++) {
		raw_percentage[i] = '\0';
	}

	const char *const file = "/sys/class/power_supply/BAT0/capacity";
	FILE *f = fopen(file, "r");
	if (f == NULL) {
		return battery_percentage_error_read;
	}

	char *read_result = fgets(raw_percentage, BATTERY_PERCENTAGE_PARSING_BUFFER_SIZE, f);

	fclose(f);
	f = NULL;

	if (read_result == NULL) {
		return battery_percentage_error_read;
	}

	size_t length = 0;
	for (size_t i = 0; i < (BATTERY_PERCENTAGE_PARSING_BUFFER_SIZE)-1; i++) {
		const char c = raw_percentage[i];
		if (c >= '0' && c <= '9') {
			continue;
		}
		if (c == '\n') {
			length = i;
			break;
		}

		return battery_percentage_error_unexpected_character;
	}

	if (length == 0) {
		return battery_percentage_error_invalid_length;
	}

	raw_percentage[length] = '\0';

	int read_percentage = atoi(raw_percentage);

	*percentage = read_percentage;

	return battery_percentage_error_none;
}

int main(void) {
	FILE *fp = stdout;

	char battery_percentage_string[BATTERY_PERCENTAGE_STRING_SIZE] = {0};
	char unix_time_string[UNIX_TIME_STRING_SIZE] = {0};
	char human_time_string[HUMAN_TIME_STRING_SIZE] = {0};

	enum battery_percentage_error bpe = 0;
	int battery_percentage = 0;

	fprintf(fp, "%s\n", "{\"version\":1}[[]");

	long long int battery_last_read_timestamp = 0;
	const int battery_read_interval_seconds = 60;

	//for (int i = 0; i < 16; i++) {
	while (1) {
		time_t current_time = time(NULL);
		if (!is_valid_time(current_time)) {
			goto error;
		}

		const long long int unix_time = get_unix_time(current_time);

		const int should_read_battery_percentage = (
			(bpe == battery_percentage_error_none) &&
			(unix_time - battery_last_read_timestamp) >= battery_read_interval_seconds
		);

		if (should_read_battery_percentage) {
			bpe = battery_percentage_get(&battery_percentage);
			if (bpe != battery_percentage_error_none) {
				battery_percentage = 0;
			}

			battery_last_read_timestamp = unix_time;
		}

		memset(unix_time_string, '\0', UNIX_TIME_STRING_SIZE);
		snprintf(unix_time_string, UNIX_TIME_STRING_SIZE, "@%lld", unix_time);

		memset(battery_percentage_string, '\0', BATTERY_PERCENTAGE_STRING_SIZE);
		if (battery_percentage == 0) {
			snprintf(
				battery_percentage_string,
				BATTERY_PERCENTAGE_STRING_SIZE,
				ERROR_MESSAGE_BATTERY_READ
			);
		} else if (battery_percentage < 100) {
			snprintf(
				battery_percentage_string,
				BATTERY_PERCENTAGE_STRING_SIZE,
				"[Battery %d%%]",
				battery_percentage
			);
		}

		json_object *root = json_object_new_array();
		if (root == NULL) {
			goto error;
		}

		json_object *item_battery_percentage = json_object_new_object();
		if (item_battery_percentage == NULL) {
			goto error;
		}

		json_object_object_add(item_battery_percentage, "color", json_object_new_string("#999999"));
		json_object_object_add(item_battery_percentage, "full_text", json_object_new_string(battery_percentage_string));

		json_object *item_time_unix = json_object_new_object();
		if (item_time_unix == NULL) {
			goto error;
		}

		json_object_object_add(item_time_unix, "color", json_object_new_string("#999999"));
		json_object_object_add(item_time_unix, "full_text", json_object_new_string(unix_time_string));

		// TODO: Reuse object.
		// json_object_object_del(item_time_unix, "full_text");

		json_object *item_time_human = json_object_new_object();
		if (item_time_human == NULL) {
			goto error;
		}

		memset(unix_time_string, '\0', UNIX_TIME_STRING_SIZE);

		struct tm *tm_info = localtime(&current_time);

		const size_t written = strftime(human_time_string, HUMAN_TIME_STRING_SIZE, "%Y-%m-%d %H:%M:%S · ", tm_info);
		if (written == 0) {
			memset(human_time_string, '\0', HUMAN_TIME_STRING_SIZE);

			fflush(fp);
			sleep_ms(1000);

			continue;
		}

		json_object_object_add(item_time_human, "color", json_object_new_string("#999999"));
		json_object_object_add(item_time_human, "full_text", json_object_new_string(human_time_string));

		json_object_array_add(root, item_battery_percentage);
		json_object_array_add(root, item_time_unix);
		json_object_array_add(root, item_time_human);

		const char *output = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
		fprintf(fp, ",%s\n", output);

		json_object_put(root);

		fflush(fp);
		sleep_ms(500);
	}

	//return 0;

error:
	return 1;
}

#endif
