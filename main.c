#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include "main.h"
#include "wayland-client.h"
#include "protocols/wlr-output-management-client.h"
#include "protocols/wlr-output-management-protocol.c"


// global objects to store state

static struct wl_list heads;
static struct zwlr_output_manager_v1 * output_manager;
static uint32_t output_manager_name;
static struct zwlr_output_configuration_v1 * configuration_object;
static char log_file_path[256];
static volatile int result = 0;
static struct wl_registry * registry;
static uint32_t current_serial;
static uint32_t previous_serial = 0;


// events - registry

void registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
	log_event(log_file_path, 4, "RECEIVED: wl_registry - global, (name: %u, interface: %s)", name, interface);
    if (strcmp(interface, "zwlr_output_manager_v1") == 0) {
        output_manager = wl_registry_bind(reg, name, &zwlr_output_manager_v1_interface, version);
		output_manager_name = name;
		log_event(log_file_path, 5 , "SENT: wl_registry - bind, (name: %u, interface: %s)", name, interface);
        zwlr_output_manager_v1_add_listener(output_manager, &output_manager_listener, NULL);
		log_event(log_file_path, 1 , "Local reference to output manager - listeners added\n");    
	}
}

void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name) {
    log_event(log_file_path, 4, "RECEIVED: wl_registry - global_remove, (name: %u)", name);
	if (name == output_manager_name){
		if (output_manager){
			output_manager = NULL;
			log_event(log_file_path, 1 , "Local reference to output manager - destroyed\n");
		}
	}
}

// events - output_manager

void output_manager_head(void * data, struct zwlr_output_manager_v1 * output_manager, struct zwlr_output_head_v1 * output_head){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_manager_v1 - head\n");
	struct local_head * lh = malloc(sizeof(struct local_head));
	memset(lh, 0, sizeof(struct local_head));
	lh->head = output_head;
	wl_list_init(&lh->available_modes);
	wl_list_insert(&heads, &lh->link);
	log_event(log_file_path, 1 , "Local reference to head - created\n");
	zwlr_output_head_v1_add_listener(lh->head, &head_listener, lh);
	log_event(log_file_path, 1 , "Local reference to head - listeners added\n");
}

void output_manager_done(void * data, struct zwlr_output_manager_v1 * output_manager, uint32_t serial){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_manager_v1 - done\n");
	previous_serial = current_serial;
	current_serial = serial;
	log_event(log_file_path, 1 , "Local reference to output manager - serial updated\n");
}

void output_manager_finished(void *data, struct zwlr_output_manager_v1 *output_manager) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_manager_v1 - finished\n");
    if (output_manager) {
        output_manager = NULL; 
		log_event(log_file_path, 1 , "Local reference to output manager - destroyed\n");
    }
}

// events - head

void head_name(void * data, struct zwlr_output_head_v1 * output_head, const char * name){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - name\n");
	struct local_head * lh = data;
	if (lh->name){
		free(lh->name);
	}
	lh->name = strdup(name);
	log_event(log_file_path, 1 , "Local reference to head - name updated\n");
}

void head_description(void * data, struct zwlr_output_head_v1 * output_head, const char * description){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - description\n");
	struct local_head * lh = data;
	if (lh->description){
		free(lh->description);
	}
	lh->description = strdup(description);
	log_event(log_file_path, 1 , "Local reference to head - description updated\n");
}

void head_physical_size(void *data, struct zwlr_output_head_v1 * output_head, int32_t width, int32_t height) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - physical_size\n");
	struct local_head * lh = data;
	lh->physical_width = width;
	lh->physical_height = height;
	log_event(log_file_path, 1 , "Local reference to head - physical size updated\n");
}

void head_mode(void *data, struct zwlr_output_head_v1 * output_head, struct zwlr_output_mode_v1 *mode) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - mode\n");
	struct local_head * lh = data;
	struct local_mode * lm = malloc(sizeof(struct local_mode));
	log_event(log_file_path, 1 , "Local reference to mode - mode created\n");
	lm->mode = mode;
	wl_list_insert(&lh->available_modes, &lm->link);
	zwlr_output_mode_v1_add_listener(lm->mode, &mode_listener, lm);
	log_event(log_file_path, 1 , "Local reference to mode - listeners added\n");
	log_event(log_file_path, 1 , "Local reference to head - mode received\n");
}

void head_enabled(void *data, struct zwlr_output_head_v1 * output_head, int32_t enabled) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - enabled\n");
	struct local_head * lh = data;
	lh->enabled = enabled;
	log_event(log_file_path, 1 , "Local reference to head - enabling status updated\n");
}

void head_current_mode(void *data, struct zwlr_output_head_v1 * output_head, struct zwlr_output_mode_v1 *mode) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - current_mode\n");
	struct local_head * lh = data;
	struct local_mode * lm;
	int found = 0;

	// 'C' = Current
	// 'B' = Became Current
	// 'P' = Previously Current
	// 'N' = Neutral (unused)

	wl_list_for_each(lm, &lh->available_modes, link){
		if(lm->mode == mode){
			lh->current_mode = lm;
			if(lm->status == 'P'){
				lm->status = 'B';
			} else {
				lm->status = 'C';
			}
			found = 1;
		}
		 else {
			if (lm->status == 'C'){
				lm->status = 'N';
			}
			else if (lm->status == 'B'){
				lm->status = 'P';
			}
		 }		
	}

	if(found == 0){
		struct local_mode * lm = malloc(sizeof(struct local_mode));
		lm->mode = mode;
		wl_list_insert(&lh->available_modes, &lm->link);
		zwlr_output_mode_v1_add_listener(lm->mode, &mode_listener, lm);
		lm->status = 'C';
	}

	log_event(log_file_path, 1 , "Local reference to head - current mode event received\n");
}

void head_position(void *data, struct zwlr_output_head_v1 * output_head, int32_t x, int32_t y) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - position\n");
    struct local_head * lh = data;
	lh->pos_x = x;
	lh->pos_y = y;
	log_event(log_file_path, 1 , "Local reference to head - position updated\n");
}

void head_transform(void *data, struct zwlr_output_head_v1 * output_head, int32_t transform) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - transform\n");
	struct local_head * lh = data;
	lh->transform = transform;
	log_event(log_file_path, 1 , "Local reference to head - transform updated\n");
}

void head_scale(void *data, struct zwlr_output_head_v1 * output_head, wl_fixed_t scale) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - scale\n");
	struct local_head * lh = data;
	lh->scale = scale;
	log_event(log_file_path, 1 , "Local reference to head - scale updated\n");
}

void head_finished(void *data, struct zwlr_output_head_v1 * output_head) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - finished\n");
	struct local_head * lh = data;
	struct local_mode * lm, * tmp_lm;
	wl_list_for_each_safe(lm, tmp_lm, &lh->available_modes, link){
		zwlr_output_mode_v1_release(lm->mode);
		log_event(log_file_path, 5, "SENT: zwlr_output_mode_v1 - release\n");
		wl_list_remove(&lm->link);
		free(lm);
	}
	if(lh->name){
		free(lh->name);
	}
	if(lh->description){
		free(lh->description);
	}
	if(lh->make){
		free(lh->make);
	}
	if(lh->model){
		free(lh->model);
	}
	if(lh->serial_number){
		free(lh->serial_number);
	}
	if(lh->head){
		zwlr_output_head_v1_release(lh->head);
		log_event(log_file_path, 5, "SENT: zwlr_output_head_v1 - release\n");
		lh->head = NULL;
	}
	wl_list_remove(&lh->link);
	free(lh);
	log_event(log_file_path, 1 , "Local reference to head - freed\n");
}

void head_make(void *data, struct zwlr_output_head_v1 * output_head, const char *make) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - make\n");
	struct local_head * lh = data;
	if (lh->make){
		free(lh->make);
	}
	lh->make = strdup(make);
	log_event(log_file_path, 1 , "Local reference to head - make updated\n");
}

void head_model(void *data, struct zwlr_output_head_v1 * output_head, const char *model) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - model\n");
	struct local_head * lh = data;
	if (lh->model){
		free(lh->model);
	}
	lh->model = strdup(model);
	log_event(log_file_path, 1 , "Local reference to head - model updated\n");
}

void head_serial_number(void *data, struct zwlr_output_head_v1 * output_head, const char * serial_number) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - number\n");
	struct local_head * lh = data;
	if (lh->serial_number){
		free(lh->serial_number);
	}
	lh->serial_number = strdup(serial_number);
	log_event(log_file_path, 1 , "Local reference to head - serial_number updated\n");
}

void head_adaptive_sync(void *data, struct zwlr_output_head_v1 * output_head, uint32_t enabled) {
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_head_v1 - adaptive_sync\n");
	struct local_head * lh = data;
	lh->adaptive_sync_state = enabled;
	log_event(log_file_path, 1 , "Local reference to head - adaptive sync status updated\n");
}	

// events - mode

void mode_size(void * data, struct zwlr_output_mode_v1 * mode, int32_t width, int32_t height){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_mode_v1 - size\n");
	struct local_mode * lm = data;
	if (lm->mode == mode){
		lm->height = height;
		lm->width = width;
	}
	log_event(log_file_path, 1 , "Local reference to mode - size updated\n");
}

void mode_refresh(void * data, struct zwlr_output_mode_v1 * mode, int32_t refresh){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_mode_v1 - refresh\n");
	struct local_mode * lm = data;
	if(lm->mode == mode){
		lm->refresh = refresh;
	}
	log_event(log_file_path, 1 , "Local reference to mode - refresh rate updated\n");
}

void mode_preferred(void * data, struct zwlr_output_mode_v1 * mode){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_mode_v1 - preferred\n");
	struct local_mode * lm = data;
	if(lm->mode == mode){
		if (lm->status == 'C'){
			lm->status = 'B';
		} else {
			lm->status = 'P';
		}
	}
	log_event(log_file_path, 1 , "Local reference to mode - status updated\n");
}

void mode_finished(void * data, struct zwlr_output_mode_v1 * mode){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_mode_v1 - finished\n");
	struct local_mode * lm = data;
	if (lm->mode){
		zwlr_output_mode_v1_release(lm->mode);
		log_event(log_file_path, 5, "SENT: zwlr_output_mode_v1 - release\n");
		lm->mode = NULL;
	}
	wl_list_remove(&lm->link);
	if (lm){
		free(lm);
	}
	log_event(log_file_path, 1 , "Local reference to mode - freed\n");
}

// events - output configuration layout

void configuration_object_succeeded(void * data, struct zwlr_output_configuration_v1 * config){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_configuration_v1 - succeeded\n");
	result = 1;
    zwlr_output_configuration_v1_destroy(config);
	log_event(log_file_path, 5, "SENT:  zwlr_output_configuration_v1 - destroy\n");
	if (configuration_object == config) {
		configuration_object = NULL;
	}
}

void configuration_object_failed(void * data, struct zwlr_output_configuration_v1 * config){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_configuration_v1 - failed\n");
	result = -1;
	zwlr_output_configuration_v1_destroy(config);
	log_event(log_file_path, 5, "SENT:  zwlr_output_configuration_v1 - destroy\n");
	if (configuration_object == config) {
		configuration_object = NULL;
	}
}

void configuration_object_cancelled(void * data, struct zwlr_output_configuration_v1 * config){
	log_event(log_file_path, 4 , "RECEIVED: zwlr_output_configuration_v1 - cancelled\n");
	result = 0;
	zwlr_output_configuration_v1_destroy(config);
	log_event(log_file_path, 5, "SENT:  zwlr_output_configuration_v1 - destroy\n");
	if (configuration_object == config) {
		configuration_object = NULL;
	}
}

// listener definitions

struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

struct zwlr_output_manager_v1_listener output_manager_listener = {
	.head = output_manager_head,	
	.done = output_manager_done,
	.finished = output_manager_finished,
};

struct zwlr_output_head_v1_listener head_listener = {
	.name = head_name,
	.description = head_description,
	.physical_size = head_physical_size,
	.mode = head_mode,
	.enabled = head_enabled,
	.current_mode = head_current_mode,
	.position = head_position,
	.transform = head_transform,
	.scale = head_scale,
	.finished = head_finished,
	.make = head_make,
	.model = head_model,
	.serial_number = head_serial_number,
	.adaptive_sync = head_adaptive_sync,
};

struct zwlr_output_mode_v1_listener mode_listener = {
	.size = mode_size,
	.refresh = mode_refresh,
	.preferred = mode_preferred,
	.finished = mode_finished,
};

struct zwlr_output_configuration_v1_listener configuration_object_listener = {
	.succeeded = configuration_object_succeeded, 
	.failed = configuration_object_failed,
	.cancelled = configuration_object_cancelled,
};

// helper methods

int setup_log_file() {
    char dir[128];
    if (getcwd(dir, sizeof(dir)) != NULL) {
        snprintf(log_file_path, sizeof(log_file_path), "%s/log.txt", dir);
		return 1;
    } else {
        perror("Error with setting up log file");
		return 0;
    }
}

const char* get_timestamp() {
    static char timestamp[28];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d::%H:%M:%S", tm_info);
    return timestamp;
}

void log_event(const char *log_file, int level, const char *format, ...) {
    FILE *file = fopen(log_file, "a");
    if (file == NULL) {
        perror("Error opening log file");
        return;
    }
    const char *timestamp = get_timestamp();
    const char *level_str = "";
    switch (level) {
        case LOG_LEVEL_INFO:
            level_str = "INFO";
            break;
        case LOG_LEVEL_ERROR:
            level_str = "ERROR";
            break;
        case LOG_LEVEL_SUCCESS:
            level_str = "SUCCESS";
            break;
        case LOG_LEVEL_EVENT_RECEIVED:
            level_str = "EVENT";
            break;
        case LOG_LEVEL_REQUEST_SENT:
            level_str = "REQUEST";
            break;
		case LOG_LEVEL_RESULT:
			level_str = "RESULT";
			break;
        default:
            level_str = "UNKNOWN";
            break;
    }
    fprintf(file, "[%s]  [%s]  ", timestamp, level_str);
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args); 
    va_end(args);
    fprintf(file, "\n");
    fclose(file);
}

void handle_print_outputs(struct wl_list *heads) {
    struct local_head *lh;
    wl_list_for_each(lh, heads, link) {
        printf("Output: %s\n", lh->name ? lh->name : "(unknown)");
        printf("  Description      : %s\n", lh->description ? lh->description : "(none)");
        printf("  Make             : %s\n", lh->make ? lh->make : "(unknown)");
        printf("  Model            : %s\n", lh->model ? lh->model : "(unknown)");
        printf("  Serial Number    : %s\n", lh->serial_number ? lh->serial_number : "(unknown)");
        printf("  Physical Size    : %dmm x %dmm\n", lh->physical_width, lh->physical_height);
        printf("  Enabled          : %s\n", lh->enabled ? "Yes" : "No");
        printf("  Position         : (%d, %d)\n", lh->pos_x, lh->pos_y);
        printf("  Transform        : %d\n", lh->transform);
        printf("  Scale Factor     : %.3f\n", wl_fixed_to_double(lh->scale));
        printf("  Adaptive Sync    : %s\n", lh->adaptive_sync_state ? "Enabled" : "Disabled");
        printf("  Available Modes:\n");
        struct local_mode *lm;
        wl_list_for_each(lm, &lh->available_modes, link) {
            const char *status_desc = "Normal";
            if (lm->status == 'C') status_desc = "Current";
            else if (lm->status == 'P') status_desc = "Preferred";
            else if (lm->status == 'B') status_desc = "Current+Preferred";

            printf("    %dx%d @ %dHz [%s]\n",
                   lm->width,
                   lm->height,
                   lm->refresh,
                   status_desc);
        }
        printf("--------------------------------------------------------\n\n");
    }
}

void free_sop(struct set_output_parser *sop) {
	if (!sop) return;
	if (sop->head) {
		sop->head = NULL;
	}
	if (sop->mode) {
		sop->mode->mode = NULL;
		free(sop->mode);
		sop->mode = NULL;
	}
	if (sop->cmode) {
		free(sop->cmode);
		sop->cmode = NULL;
	}

	if (sop->pos) {
		free(sop->pos);
		sop->pos = NULL;
	}

	if (sop->transform) {
		free(sop->transform);
		sop->transform = NULL;
	}
	if (sop->scale) {
		free(sop->scale);
		sop->scale = NULL;
	}
	if (sop->adaptive_sync) {
		free(sop->adaptive_sync);
		sop->adaptive_sync = NULL;
	}
	free(sop);
}

struct command_result * fill_res (struct command_result * res, int cmd, int val, int err){
	res->command = cmd;
	res->validity = val;
	res->error_code = err;
	res->data = NULL;
	return res;
}

void free_res(struct command_result *res) {
    if (res->data) free_sop(res->data);
    free(res);
}

struct command_result * parse_command(char * cmd){

	struct command_result * res = malloc(sizeof(struct command_result));
	res->data = NULL;
	char * param_one = strtok(cmd, " ");

	// CASE - NO COMMAND
	if (!param_one) {
		return fill_res(res, 0, 0, 1);
	}
	
	// CASE - LIST_OUTPUTS
	else if (strcmp(param_one,"list_outputs")==0){
		handle_print_outputs(&heads);
		return fill_res(res, 1, 1, 0);
	}

	// CASE - SET_OUTPUT

	else if (strcmp(param_one,"set_output")==0){

		char * param_two = strtok(NULL, " ");
		if (!param_two){
			return fill_res(res, 2, 0, 2);

		} else {
			int found = 0;
			struct local_head * lh;
			wl_list_for_each(lh, &heads, link){
				if (strcmp(param_two,lh->name)==0 && lh->enabled==1){
					found = 1;
					break;
				}
			}
		
			if (found == 0){
				return fill_res(res, 2, 0, 3);
			}

			int num_subcommands = 0;
			int num_cmd_mode = 0;
			int num_cmd_cmode = 0;
			int num_cmd_scale = 0;
			int num_cmd_transform = 0;
			int num_cmd_position = 0;
			int num_cmd_adaptive_sync = 0;
			struct set_output_parser * sop = malloc(sizeof(struct set_output_parser));
			
			memset(sop, 0, sizeof(struct set_output_parser));
			sop->head = lh;

			while(1){

				char * subcmd = strtok(NULL, " ");
				char * propval = NULL;

				if (num_subcommands >= MAX_SUBCMDS){
					free_sop(sop);
					return fill_res(res, 2, 0, 4);
				}
				if (!subcmd && num_subcommands==0){
					free_sop(sop);
					return fill_res(res, 2, 0, 5);
				}
				else if (! subcmd && num_subcommands > 0){
					res->command = 2;
					res->validity = 1;
					res->error_code = 0;
					res->data = sop;
					return res;
				}
				else {
					propval = strtok(NULL, " ");
					if (!propval){
						free_sop(sop);
						return fill_res(res, 2, 0, 6);
					}
				}
				

				if (subcmd && propval){

					num_subcommands++;

					if(strcmp(subcmd, "mode") == 0 ){
						if ((num_cmd_mode < 1) && (num_cmd_cmode < 1)){
							struct local_mode * lm;
							int width, height, refresh;
							int mode_found = 0;
							if (sscanf(propval, "%d,%d@%d", &width, &height, &refresh) == 3) {
								wl_list_for_each(lm, &lh->available_modes, link){
									if (lm->width == width && lm->height == height && lm->refresh == refresh){
										sop->mode = malloc(sizeof(struct local_mode_modified));
										if (sop->mode == NULL) {
											free_sop(sop);
											return fill_res(res, 2, 0, 7);
										}
										(sop->mode)->mode = lm;
										(sop->mode)->status = 1;
										num_cmd_mode++;	
										mode_found = 1;
										log_event(log_file_path, 1 , "Valid mode subcommand found\n");
										break;
									}
								}	
							} else {
								free_sop(sop);
								return fill_res(res, 2, 0, 8);
							}

							if (mode_found == 0){
								free_sop(sop);
								return fill_res(res, 2, 0, 8);
							}

						} 
						else {
							free_sop(sop);
							return fill_res(res, 2, 0, 8);
						}
					}


					else if(strcmp(subcmd, "cmode") == 0){
						if(num_cmd_cmode < 1 && num_cmd_mode < 1){
							struct custom_mode * cmode = malloc(sizeof(struct custom_mode));
							if (cmode == NULL) {
								free_sop(sop);
								return fill_res(res, 2, 0, 7);
							}
							if (sscanf(propval, "%d,%d@%d", &cmode->width, &cmode->height, &cmode->refresh) == 3){
								cmode->status = 1;
								sop->cmode = cmode;
								num_cmd_cmode++;
								log_event(log_file_path, 1 , "Valid custom mode subcommand found\n");

							} else {
								free_sop(sop);
								return fill_res(res, 2, 0, 9);
							}
						}
						else {
							free_sop(sop);
							return fill_res(res, 2, 0, 9);
						}
					}

					else if (strcmp(subcmd, "pos") == 0) {
						if (num_cmd_position < 1) {
							struct position *pos = malloc(sizeof(struct position));
							if (pos == NULL) {
								free_sop(sop);
								return fill_res(res, 2, 0, 7);
							}
					
							if (sscanf(propval, "%d,%d", &pos->x, &pos->y) == 2) {
								pos->status = 1;
								sop->pos = pos;
								num_cmd_position++;
								log_event(log_file_path, 1 , "Valid position subcommand found\n");

							} else {
								free_sop(sop); 
								return fill_res(res, 2, 0, 10);
							}

						} else {
							free_sop(sop);
							return fill_res(res, 2, 0, 10);
						}
					}
					

					else if(strcmp(subcmd, "transform") == 0){
						if (num_cmd_transform < 1){
							int transform;
							if (sscanf(propval, "%d", &transform) == 1){
								if ((transform <= 7) && (transform >= 0)){
									sop->transform = malloc(sizeof(struct transform));
									if (sop->transform == NULL) {
										free_sop(sop);
										return fill_res(res, 2, 0, 7);
									}
									sop->transform->transform = transform;
									sop->transform->status = 1;
									num_cmd_transform++;
									log_event(log_file_path, 1 , "Valid transform subcommand found\n");
								} else {
									free_sop(sop);
									return fill_res(res, 2, 0, 11);
								}
							} else {
								free_sop(sop);
								return fill_res(res, 2, 0, 11);
							}
						}
						else {
							free_sop(sop);
							return fill_res(res, 2, 0, 11);
						}
					}

					else if(strcmp(subcmd, "scale") == 0){
						if (num_cmd_scale < 1) {
							double scale_value;
							if (sscanf(propval, "%lf", &scale_value) == 1){
								if (scale_value > 0.0) { 
									sop->scale = malloc(sizeof(struct scale));
									if (sop->scale == NULL) {
										free_sop(sop);
										return fill_res(res, 2, 0, 7);
									}
									sop->scale->scale = wl_fixed_from_double(scale_value);
									sop->scale->status = 1;
									num_cmd_scale++;
									log_event(log_file_path, 1 , "Valid scale subcommand found\n");
								} else {
									free_sop(sop);
									return fill_res(res, 2, 0, 12);
								}
							} else {
								free_sop(sop);
								return fill_res(res, 2, 0, 12);
							}
						} else {
							free_sop(sop);
							return fill_res(res, 2, 0, 12);
						}
					}

					else if(strcmp(subcmd, "adaptivesync") == 0){
						if (num_cmd_adaptive_sync < 1) {
							uint32_t adaptive_sync_value;
							if (sscanf(propval, "%d", &adaptive_sync_value) == 1) {
								if (adaptive_sync_value == 0 || adaptive_sync_value == 1) {
									sop->adaptive_sync = malloc(sizeof(struct adaptive_sync));
									if (sop->adaptive_sync == NULL) {
										free_sop(sop);
										return fill_res(res, 2, 0, 7);
									}
									sop->adaptive_sync->adaptive_sync = adaptive_sync_value;
									sop->adaptive_sync->status = 1;
									num_cmd_adaptive_sync++;
									log_event(log_file_path, 1 , "Valid adaptive sync subcommand found\n");
								} else {
									free_sop(sop);
									return fill_res(res, 2, 0, 13);
								}
							} else {
								free_sop(sop);
								return fill_res(res, 2, 0, 13);
							}
						} else {
							free_sop(sop);
							return fill_res(res, 2, 0, 13);
						}
					}
					
					else {
						free_sop(sop);
						return fill_res(res, 2, 0, 6);
					}
				}		
			}
	 	}
	}
	
	else if (strcmp(param_one, "monitor") == 0) {
		char *param_two = strtok(NULL, " ");

		char line[1024];

		FILE *file = fopen(log_file_path, "r");
		if (!file) {
			perror("Error opening log file for reading");
			return fill_res(res, 3, 0, 15);
		}

		if (!param_two) {
			while (fgets(line, sizeof(line), file)) {
				printf("%s", line);
			}
			
			fclose(file);
    		return fill_res(res, 3, 1, 0);
		}
	
		else if (strcmp(param_two, "single") == 0) {
			char timestamp[100]; 
			char *param_three = strtok(NULL, " ");
			if (!param_three) {
				fclose(file);
				return fill_res(res, 3, 0, 16);
			}
	
			while (fgets(line, sizeof(line), file)) {
				if (sscanf(line, "[%[^]]]", timestamp) == 1) {
					if (strcmp(timestamp, param_three) == 0) {
						printf("%s", line);
					}
				}
			}
		}
	
		else if (strcmp(param_two, "period") == 0) {
			char timestamp[100]; 
			char *param_three = strtok(NULL, " ");
			char *param_four = strtok(NULL, " ");

			if (!param_three || !param_four) {
				fclose(file);
				return fill_res(res, 3, 0, 17); 
			}
	
			while (fgets(line, sizeof(line), file)) {
				if (sscanf(line, "[%[^]]]", timestamp) == 1) {
					if (strcmp(timestamp, param_three) >= 0 && strcmp(timestamp, param_four) <= 0) {
						printf("%s", line);
					}
				}
			}
		}
	
		else {
			fclose(file);
			return fill_res(res, 3, 0, 14);  
		}
	
		fclose(file);
		return fill_res(res, 3, 1, 0); 
	}

	
	else if (strcmp(param_one, "exit")==0){
		return fill_res(res, 4, 1, 0);
	}

	else {
		return fill_res(res, 0, 0, 1);
	}
}

const char* get_error_message(uint32_t error_code) {
    switch (error_code) {
        case 0: return "NO_ERROR";
        case 1: return "INVALID_MAIN_COMMAND";
        case 2: return "COMMAND_INCOMPLETE";
        case 3: return "INVALID_OUTPUT_HEAD";
        case 4: return "EXCEEDED_MAX_SUBCMDS";
        case 5: return "NO_SUBCOMMANDS";
        case 6: return "INVALID_SUBCOMMAND";
        case 7: return "MEMORY_NOT_ALLOCATED";
        case 8: return "INVALID_SUBCOMMAND_MODE";
        case 9: return "INVALID_SUBCOMMAND_CMODE";
        case 10: return "INVALID_SUBCOMMAND_POS";
        case 11: return "INVALID_SUBCOMMAND_TRANSFORM";
        case 12: return "INVALID_SUBCOMMAND_SCALE";
        case 13: return "INVALID_SUBCOMMAND_ADAPTIVE_SYNC";
        case 14: return "INVALID_MONITOR_COMMAND";
        case 15: return "LOG_FILE_ERROR";
        case 16: return "INVALID_MONITOR_SINGLE";
        case 17: return "INVALID_MONITOR_PERIOD";
        case 18: return "INVALID_MONITOR_MULTIPLE";
        default: return "UNKNOWN_ERROR";
    }
}

	

int main(){

	int lof_file_status = setup_log_file();
	if (lof_file_status == 0){
		return -1;
	}
	log_event(log_file_path, 1, "Log File set up done in CWD.\n");

	struct wl_display * display = wl_display_connect(NULL);
	if (!display){
		log_event(log_file_path, 2, "Connection to Wayland display failed");
		perror("Connection to wayland display failed");
		return -1;
	}
	log_event(log_file_path, 1 , "Connected to Wayland Socket: %s\n", getenv("WAYLAND_DISPLAY"));
	wl_list_init(&heads);	

	registry = wl_display_get_registry(display);
	log_event(log_file_path, 5 , "Local reference to registry - created\n");
	wl_registry_add_listener(registry, &registry_listener, 0);
	log_event(log_file_path, 1 , "Local reference to registry - listeners added\n");

	wl_display_roundtrip(display);

	char input[256];
	while(1){
		wl_display_roundtrip(display);
		wl_display_roundtrip(display);

		printf("$ " );

		if (fgets(input, sizeof(input), stdin)!=NULL){
			input[strcspn(input, "\n")] = '\0';
			struct command_result * cmd = parse_command(input);

			if (cmd->validity == 0) {
				log_event(log_file_path, 1, "Invalid Command");
				log_event(log_file_path, 7, "Error: %s", get_error_message(cmd->error_code));
				perror("Invalid Command\n");
			}

			else if (cmd->command == 1){
				log_event(log_file_path, 1, "List Output command received");
				log_event(log_file_path, 7, "%s", get_error_message(cmd->error_code));
			}

			else if (cmd->command == 2){
				log_event(log_file_path, 1, "Set Output command received");
				struct set_output_parser * sop = cmd->data;
				struct local_head *lh = sop->head;

				configuration_object = zwlr_output_manager_v1_create_configuration(output_manager, current_serial);
				log_event(log_file_path, 5 , "SENT: zwlr_output_manager_v1 - create_configuration\n");
				zwlr_output_configuration_v1_add_listener(configuration_object, &configuration_object_listener, 0);
				log_event(log_file_path, 1 , "Local reference to configuration object - created\n");
				log_event(log_file_path, 1 , "Local reference to configuration object - listeners added\n");


				if (lh->enabled){
					lh->head_config = zwlr_output_configuration_v1_enable_head(configuration_object, lh->head);
					log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_v1 - enable_head\n");
					log_event(log_file_path, 1 , "Local reference to head config - created\n");

					if (sop->mode){
						if (sop->mode->status == 1){
							zwlr_output_configuration_head_v1_set_mode(lh->head_config, sop->mode->mode->mode);
							log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_head_v1 - set_mode\n");
						}
					}

					if (sop->pos){
						if (sop->pos->status == 1){
							zwlr_output_configuration_head_v1_set_position(lh->head_config, sop->pos->x, sop->pos->y);
							log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_head_v1 - set_position\n");
						}
					}

					if (sop->cmode){
						if (sop->cmode->status == 1){
							zwlr_output_configuration_head_v1_set_custom_mode(lh->head_config, sop->cmode->width, sop->cmode->height, sop->cmode->refresh);
							log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_head_v1 - set_custom_mode\n");
						}
					}

					if (sop->transform){
						if (sop->transform->status == 1){
							zwlr_output_configuration_head_v1_set_transform(lh->head_config, sop->transform->transform);
							log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_head_v1 - set_transform\n");
						}
					}
					if (sop->scale){
						if (sop->scale->status == 1){
							zwlr_output_configuration_head_v1_set_scale(lh->head_config, sop->scale->scale);
							log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_head_v1 - set_scale\n");
						}
					}
					if (sop->adaptive_sync){
						if (sop->adaptive_sync->status == 1){
							zwlr_output_configuration_head_v1_set_adaptive_sync(lh->head_config, sop->adaptive_sync->adaptive_sync);
							log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_head_v1 - set_adaptive_sync\n");
						}
					}

					zwlr_output_configuration_v1_apply(configuration_object);
					log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_v1 - apply\n");
					wl_display_roundtrip(display);
				}
			}

			else if (cmd->command == 3){
				log_event(log_file_path, 1, "Monitor command received");
				log_event(log_file_path, 7, "%s", get_error_message(cmd->error_code));
			}

			else if (cmd->command == 4){
				log_event(log_file_path, 1, "Exit command received");
				log_event(log_file_path, 7, "%s", get_error_message(cmd->error_code));
				break;
			}

			else {
				log_event(log_file_path, 1, "Invalid command");
				perror("Please type 'exit' to exit the program");
			}

		}
	}

	// CLEAN UP

	log_event(log_file_path, 1, "Cleaning up...\n");
	

	struct local_head *lh, *tmp_lh;
	wl_list_for_each_safe(lh, tmp_lh, &heads, link) {
		if(lh->name){
			free(lh->name);
		}
		if(lh->description){
			free(lh->description);
		}
		if(lh->make){
			free(lh->make);
		}
		if(lh->model){
			free(lh->model);
		}
		if(lh->serial_number){
			free(lh->serial_number);
		}
		if (lh->head) {
			zwlr_output_head_v1_release(lh->head);
			log_event(log_file_path, 5, "SENT: zwlr_output_head_v1 - release\n");
		}
		if(lh->head_config){
			lh->head_config = NULL;
		}

		struct local_mode *lm, *tmp_lm;
		wl_list_for_each_safe(lm, tmp_lm, &lh->available_modes, link) {
			zwlr_output_mode_v1_release(lm->mode);
			log_event(log_file_path, 5, "SENT: zwlr_output_mode_v1 - release\n");
			wl_list_remove(&lm->link);
			free(lm);
			log_event(log_file_path, 1 , "Local reference to mode - freed\n");
		}

		wl_list_remove(&lh->link);
		free(lh);
		log_event(log_file_path, 1 , "Local reference to head - freed\n");
	}

	if (configuration_object){
		zwlr_output_configuration_v1_destroy(configuration_object);
		log_event(log_file_path, 5 , "SENT: zwlr_output_configuration_v1 - destroy\n");
		configuration_object = NULL;
	}
	wl_display_roundtrip(display);
	zwlr_output_manager_v1_stop(output_manager);
	log_event(log_file_path, 5 , "SENT: zwlr_output_manager_v1 - stop\n");


	wl_display_roundtrip(display);
	wl_display_disconnect(display);

	return 0;
}




// else if (strcmp(param_one, "enable")==0){
// 	printf("Command to enable outputs\n");
// 	char * param_two = strtok(NULL, " ");

// 	if (!param_two){
// 		printf("No output listed to enable\n");
// 		res->command = 2;
// 		res->result = 0;
// 		res->message = "No output listed to enable\n";
// 		return res;
// 	}

// 	int found = 0;
// 	struct local_head * lh;
// 	wl_list_for_each(lh, &heads, link){
// 		if (strcmp(param_two,lh->name)==0 && lh->enabled==0){
// 			found = 1;
// 			break;
// 		}
// 	}

// 	if (found == 0){
// 		printf("No output of given name found\n");
// 		res->command = 2;
// 		res->result = 0;
// 		res->message = "No output of given name found \n";
// 		return res;
// 	} else {
// 		res->command = 2;
// 		res->result = 1;
// 		res->data = lh;
// 		return  res;

// 	}

// }

// else if (strcmp(param_one, "disable")==0){
// 	printf("Command to disable outputs\n");
// 	char * param_two = strtok(NULL, " ");

// 	if (!param_two){
// 		printf("No output listed to disable\n");
// 		res->command = 3;
// 		res->result = 0;
// 		res->message = "No output listed to disable\n";
// 		return res;
// 	}

// 	int found = 0;
// 	struct local_head * lh;
// 		wl_list_for_each(lh, &heads, link){
// 			if (strcmp(param_two,lh->name)==0 && lh->enabled==1){
// 				found = 1;
// 				break;
// 			}
// 		}

// 	if (found == 0){
// 		printf("No output of given name found\n");
// 		res->command = 3;
// 		res->result = 0;
// 		res->message = "No output of given name found \n";
// 		return res;
// 	}

// 	res->command = 3;
// 	res->result = 1;
// 	res->data = lh;
// 	return  res;

// }

		
