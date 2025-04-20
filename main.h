#ifndef NEW_H
#define NEW_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>    
#include <wayland-client.h> 

struct zwlr_output_manager_v1;
struct zwlr_output_head_v1;
struct zwlr_output_mode_v1;
struct zwlr_output_configuration_v1;
struct zwlr_output_configuration_head_v1;


/**
 * This file is an implementation of the wlr-output-management protocol. 
 * The file is structured in the following way:
 * 1. Macros
 * 2. Structures
 * 3. Global variables (in main.c)
 * 4. Implementation of Registry events
 * 5. Implementation of Output Manager events
 * 6. Implementation of Head events
 * 7. Implementation of Mode events
 * 8. Implementation of Configuration events
 * 9. Implementation of Command functions
 * 10. Implementation of Logging functions
 * 11. Main ()
 */

#define MAX_SUBCMDS                        5

#define LOG_LEVEL_INFO                     1
#define LOG_LEVEL_ERROR                    2
#define LOG_LEVEL_SUCCESS                  3
#define LOG_LEVEL_EVENT_RECEIVED           4
#define LOG_LEVEL_REQUEST_SENT             5
#define LOG_LEVEL_UNKNOWN                  6
#define LOG_LEVEL_RESULT                   7

#define NO_ERROR                           0
#define INVALID MAIN COMMAND               1
#define COMMAND_INCOMPLETE                 2
#define INVALID_OUTPUT_HEAD                3
#define EXCEEDED MAX_SUBCMDS               4
#define NO_SUBCOMMANDS                     5
#define INVALID_SUBCOMMAND                 6
#define MEMORY NOT ALLOCATED               7
#define INVALID_SUBCOMMAND_MODE            8
#define INVALID_SUBCOMMAND_CMODE           9
#define INVALID_SUBCOMMAND_POS            10
#define INVALID_SUBCOMMAND_TRANSFORM      11
#define INVALID_SUBCOMMAND_SCALE          12
#define INVALID_SUBCOMMAND_ADAPTIVE_SYNC  13
#define INVALID_MONITOR COMMAND           14
#define LOG_FILE_ERROR                    15
#define INVALID_MONITOR_SINGLE            16
#define INVALID_MONITOR_PERIOD            17
#define INVALID_MONITOR_MULTIPLE          18

struct command_result {
    uint32_t command;
	uint32_t validity;
	uint32_t error_code;
    void * data;
};

struct set_output_parser {
	struct local_head * head;
	struct custom_mode * cmode;
	struct local_mode_modified * mode;
	struct position * pos;
	struct transform * transform;
	struct scale * scale;
	struct adaptive_sync * adaptive_sync;	
};

struct custom_mode {
	int32_t status;
	int32_t width;
	int32_t height;
	int32_t refresh;
};

struct local_mode_modified{
	int32_t status;
	struct local_mode * mode;
};

struct position {
	int32_t status;
	int32_t x;
	int32_t y;
};

struct transform{
	int32_t status;
	uint32_t transform;
};

struct scale{
	int32_t status;
	wl_fixed_t scale;
};

struct adaptive_sync{
	int32_t status;
	uint32_t adaptive_sync;
};

struct local_mode{
	struct zwlr_output_mode_v1 * mode;
	struct wl_list link;
	int32_t height;
	int32_t width;
	int32_t refresh;
	char status;
};

struct local_head{
	struct wl_list link;
	struct zwlr_output_head_v1 * head;
	struct wl_list available_modes;
	char * name;
	char * description;
	int32_t physical_width;
	int32_t physical_height;
	struct local_mode * current_mode;
	int32_t enabled;
	int32_t pos_x;
	int32_t pos_y;
	int32_t transform;
	wl_fixed_t scale;
	char * make;
	char * model;
	char * serial_number;
	uint32_t adaptive_sync_state;
	struct zwlr_output_configuration_head_v1 * head_config;
};

struct config_context {
	int result;
};

// listeners

struct zwlr_output_head_v1_listener head_listener;
struct zwlr_output_mode_v1_listener mode_listener;
struct zwlr_output_manager_v1_listener output_manager_listener;
struct wl_registry_listener registry_listener;
struct zwlr_output_configuration_v1_listener configuration_object_listener;

// methods

void registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version);
void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name);

void output_manager_head(void * data, struct zwlr_output_manager_v1 * output_manager, struct zwlr_output_head_v1 * output_head);
void output_manager_done(void * data, struct zwlr_output_manager_v1 * output_manager, uint32_t serial);
void output_manager_finished(void *data, struct zwlr_output_manager_v1 *output_manager);

void head_name(void * data, struct zwlr_output_head_v1 * output_head, const char * name);
void head_description(void * data, struct zwlr_output_head_v1 * output_head, const char * description);
void head_physical_size(void *data, struct zwlr_output_head_v1 * output_head, int32_t width, int32_t height);
void head_mode(void *data, struct zwlr_output_head_v1 * output_head, struct zwlr_output_mode_v1 *mode);
void head_enabled(void *data, struct zwlr_output_head_v1 * output_head, int32_t enabled);
void head_current_mode(void *data, struct zwlr_output_head_v1 * output_head, struct zwlr_output_mode_v1 *mode);
void head_position(void *data, struct zwlr_output_head_v1 * output_head, int32_t x, int32_t y);
void head_transform(void *data, struct zwlr_output_head_v1 * output_head, int32_t transform);
void head_scale(void *data, struct zwlr_output_head_v1 * output_head, wl_fixed_t scale);
void head_finished(void *data, struct zwlr_output_head_v1 * output_head);
void head_make(void *data, struct zwlr_output_head_v1 * output_head, const char *make);
void head_model(void *data, struct zwlr_output_head_v1 * output_head, const char *model);
void head_serial_number(void *data, struct zwlr_output_head_v1 * output_head, const char * serial_number);
void head_adaptive_sync(void *data, struct zwlr_output_head_v1 * output_head, uint32_t enabled);

void mode_size(void * data, struct zwlr_output_mode_v1 * mode, int32_t width, int32_t height);
void mode_refresh(void * data, struct zwlr_output_mode_v1 * mode, int32_t refresh);
void mode_preferred(void * data, struct zwlr_output_mode_v1 * mode);
void mode_finished(void * data, struct zwlr_output_mode_v1 * mode);


int setup_log_file();
const char* get_timestamp();
void log_event(const char *log_file, int level, const char *format, ...);
void handle_print_outputs(struct wl_list *heads);
void free_sop(struct set_output_parser *sop);
struct command_result * fill_res (struct command_result * res, int cmd, int val, int err);
void free_res(struct command_result *res);
struct command_result * parse_command(char * cmd);
const char* get_error_message(uint32_t error_code);




#endif