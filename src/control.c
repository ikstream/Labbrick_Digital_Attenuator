#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <libgen.h>
#include "control.h"
#include "input.h"
#include "LDAhid.h"

#define _GNU_SOURCE
#define FALSE 0
#define TRUE !FALSE
#define STRING_LENGTH 12
#define SLEEP_TIME 1000000
#define MAX_USLEEP_TIME 1000000
#define LINE_LENGTH 256
#define TIME 1
#define ATT 2
#define RAMP 1
#define TRIANGLE 1
#define SIMPLE 0
#define SINGLE_DEV 0
#define SINGLE_DEV_ID 1
#define MAX_MSG_SIZE 64
#define MAX_PATH_LENGTH 512

/* pthread struct */
struct thread_arguments {
	char *path;
	int id;
};

/*
 * Get device id from serial number
 * @param serial: device serial number
 * @param device_count: number of devices connected
 */
int
get_id_by_serial(int serial, unsigned int device_count)
{
	DEVID id;
	int tmp_serial, serial_id;
	serial_id = -1;

	for (id = 1; id <= device_count; id++) {
		tmp_serial = fnLDA_GetSerialNumber(id);
		if (tmp_serial == serial)
			serial_id = (int)id;
	}

	return serial_id;
}

/*
 * Get the model name and serial number of connected devices
 * @param device_count: number of devices connected
 * @param device_name: storage location for device name
 */
void
get_serial_and_name(unsigned int device_count, char *device_name)
{
	DEVID id;
	unsigned int serial;

	for (id = 1; id <= device_count; id++) {
		printf(INFO "Device %d ==> Modelname: ", id);
		fnLDA_GetModelName(id, device_name);
		serial = fnLDA_GetSerialNumber(id);
		printf("%s - Serial Number: %d\n", device_name, serial);
	}
}

/*
 * use usleep in a safe way for systems not supporting more than 1,000,000
 * microseconds for usleep
 * @param usec: time to sleep
 * @return: returns 0 if time is up
 */
int
susleep(unsigned long usec)
{
	int res;
	unsigned long timeleft = usec;
	while (timeleft > MAX_USLEEP_TIME) {
		res = usleep(MAX_USLEEP_TIME);
		if (res != 0)
			return res;
		timeleft -= MAX_USLEEP_TIME;
	}
	return usleep(timeleft);
}

/*
 * print out device information of attenuator
 * @param id: attenuator id
 */
void
print_dev_info(int id)
{
	printf(INFO "You can set attenuation steps in %.2fdB steps\n",
		(double)(fnLDA_GetDevResolution(id)) / MULTIPLIER_STEP);
	printf(INFO "min attenuation: %.2fdB\n",
		(double)fnLDA_GetMinAttenuation(id) / MULTIPLIER_STEP);
	printf(INFO "max attenuation: %.2fdB\n",
		(double)fnLDA_GetMaxAttenuation(id) / MULTIPLIER_STEP);
}

/*
 * check device status and return error state if something is wrong
 * @param current_device: id of current device
 * @return: returns success, or error message
 */
char *
get_device_data(unsigned int current_device)
{
	char *success = "Successfully checked device\n";
	unsigned int status;

	status = fnLDA_GetAttenuation(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetMinAttenuation(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetMaxAttenuation(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetIdleTime(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetDwellTime(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetAttenuationStep(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetRF_On(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetRampStart(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));

	status = fnLDA_GetRampEnd(current_device);
	if (status == INVALID_DEVID
	    || status == DEVICE_NOT_READY)
		return strncpy(errmsg ,fnLDA_perror(status),
		     strlen(errmsg));
	return success;
}

/*
 * help function to display possible options and correct usage
 */
void
call_help(void)
{
	printf("-to show this overview use\n");
	printf("\t-h\n");
	printf("\r\n");

	printf("-set attenuation with\n");
	printf("\t-a <attenuation in dB>\n");
	printf("\r\n");

	printf("-set end attenuation strength in dB with\n");
	printf("\t-end <dB>\n");
	printf("\r\n");

	printf("-to use a .csv file\n");
	printf("\t-f path/to/file\n");
	printf("\tcsv file is expected to have time;attenuation format\n");
	printf("\r\n");

	printf("print additional device information\n");
	printf("\t-i\n");
	printf("\r\n");

	printf("-log attenuation changes to a .csv file\n");
	printf("\t-l path/to/logfile\n");
	printf("\r\n");

	printf("-remove [INFO] output\n");
	printf("\t-q\n");
	printf("\r\n");

	printf("-repeat form, or file input until canceled by user\n");
	printf("\t-r\n");
	printf("\r\n");

	printf("-set attenuation form with\n");
	printf("\t-ramp|-triangle\n");
	printf("\r\n");

	printf("-repeat form, or file input for several times\n");
	printf("\t-rr <#runs>\n");
	printf("\r\n");

	printf("-set starting attenuation strength in dB with\n");
	printf("\t-start <dB>\n");
	printf("\r\n");

	printf("-set size of steps for attenuation in dB with\n");
	printf("\t-step <dB>\n");
	printf("\r\n");

	printf("-set time for attenuation duration with\n");
	printf("\t-t <time in sec>\n");
	printf("\r\n");

	printf("\tyou can use s, ms or us to set time units\n");
	printf("\t\ts -> seconds\n");
	printf("\t\tms -> milliseconds\n");
	printf("\t\tus -> microseconds\n");
	printf("\r\n");

	printf("-use specific device detected by serial number\n");
	printf("\t-n <serial number>\n");
	printf("\r\n");

	printf("-to use more than one connected attenuator use\n");
	printf("\t-md <config_file1> <config_file2> ...\n");
	printf("\r\n");
	printf("\t every connected and detected attenuator will be used\n");
	printf("\t if there are more config files than attenuators detected\n");
	printf("\t the remaining files will be discarded\n");
	printf("\r\n");

	printf("-to use more than one connected attenuator (with serial number association) use\n");
	printf("\t-mds <serial_num_1.csv> <serial_num_2.csv> ...\n");
	printf("\r\n");
	printf("\t every attenuator matching with serial number specified in configuration file will be used\n");
	printf("\t not matching attenuators will be discarded\n");
	printf("\t [NOTE] file format is: serial number + csv extension, eg. 10314.csv\n");
	printf("\r\n");

	return;
}

/*
 * check if attenuation is above, or below device limits
 * @param id: device id
 * @param ud: user data struct to check attenuation from
 * @param check: 0 if only check for atttenuation boundaries
 * 		 1 if to check for start and end attenuation as well
 */
void
check_att_limits(int id, int serial, struct user_data *ud, int check)
{
	/* check for simple case */
	if (check == 0) {
		if (ud->attenuation < fnLDA_GetMinAttenuation(id)) {
			printf(WARN "%.2f is below minimal attenuation of %.2f (serial %i)\n",
				(double)ud->attenuation / MULTIPLIER_STEP,
				(double)fnLDA_GetMinAttenuation(id) / MULTIPLIER_STEP,
				serial);
			printf(WARN "attenuation has been set to %.2fdB (serial %i)\n",
				(double)fnLDA_GetMinAttenuation(id) / MULTIPLIER_STEP,
				serial);
			fnLDA_SetAttenuation(id, fnLDA_GetMinAttenuation(id));
			log_attenuation(fnLDA_GetMinAttenuation(id), ud);
		} else if (ud->attenuation > fnLDA_GetMaxAttenuation(id)) {
			printf(WARN "%.2f is above maximal attenuation of %.2f (serial %i)\n",
				(double)ud->attenuation / MULTIPLIER_STEP,
				(double)fnLDA_GetMaxAttenuation(id) / MULTIPLIER_STEP,
				serial);
			printf(WARN "attenuation has been set to %.2f (serial %i)\n",
				(double)fnLDA_GetMaxAttenuation(id) / MULTIPLIER_STEP,
				serial);
			fnLDA_SetAttenuation(id, fnLDA_GetMaxAttenuation(id));
			log_attenuation(fnLDA_GetMaxAttenuation(id), ud);
		} else {
			fnLDA_SetAttenuation(id, (ud->attenuation));
			log_attenuation(ud->attenuation, ud);
			if (!ud->quiet) {
				printf(INFO "set device (serial %i) to %.2fdB attenuation\n",
					serial, (double)(fnLDA_GetAttenuation(id)) / MULTIPLIER_STEP);
					if (ud->us == 1)
						printf(INFO "attenuation time: %ld us\n", ud->atime);
					else if (ud->ms == 1)
						printf(INFO "attenuation time: %ld ms\n", ud->atime);
					else
						printf(INFO "attenuation time: %ld s\n", ud->atime);
			}
		}
	}

	/* check for start and end attenuation */
	if (check == 1) {
		if (ud->start_att < fnLDA_GetMinAttenuation(id)) {
			printf(WARN "%.2f is below minimal attenuation of %.2f (serial %i)\n",
				(double)ud->start_att / MULTIPLIER_STEP,
				(double)fnLDA_GetMinAttenuation(id) / MULTIPLIER_STEP,
				serial);
			printf(WARN "start attenuation has been set to %.2fdB (serial %i)\n",
				(double)fnLDA_GetMinAttenuation(id) / MULTIPLIER_STEP,
				serial);
			ud->start_att = fnLDA_GetMinAttenuation(id);
		}
		if (ud->start_att > fnLDA_GetMaxAttenuation(id)) {
			printf(WARN "%.2f is above maximal attenuation of %.2f (serial %i)\n",
				(double)ud->start_att / MULTIPLIER_STEP, 
				(double)fnLDA_GetMaxAttenuation(id) / MULTIPLIER_STEP,
				serial);
			printf(WARN "start attenuation has been set to %.2f (serial %i)\n",
				(double)fnLDA_GetMaxAttenuation(id) / MULTIPLIER_STEP,
				serial);
			ud->start_att = fnLDA_GetMaxAttenuation(id);
		}
		if (ud->end_att < fnLDA_GetMinAttenuation(id)) {
			printf(WARN "%.2f is below minumal attenuation of %.2f (serial %i)\n",
				(double)ud->end_att / MULTIPLIER_STEP,
				(double)fnLDA_GetMinAttenuation(id) / MULTIPLIER_STEP,
				serial);
			printf(WARN "final attenuation has been set to %.2fdB (serial %i)\n",
				(double)fnLDA_GetMinAttenuation(id) / MULTIPLIER_STEP,
				serial);
			ud->end_att = fnLDA_GetMinAttenuation(id);
		}
		if (ud->end_att > fnLDA_GetMaxAttenuation(id)) {
			printf(WARN "%.2f is above maximal attenuation of %.2f (serial %i)\n",
				(double)ud->end_att / MULTIPLIER_STEP,
				(double)fnLDA_GetMaxAttenuation(id) / MULTIPLIER_STEP,
				serial);
			printf(WARN "final attenuation has been set to %.2f (serial %i)\n",
				(double)fnLDA_GetMaxAttenuation(id) / MULTIPLIER_STEP,
				serial);
			ud->end_att = fnLDA_GetMaxAttenuation(id);
		}
	}
}

void
check_stepsize(struct user_data *ud, int id)
{
	if (ud->ramp_steps > fnLDA_GetMaxAttenuation(id)) {
		ud->ramp_steps = fnLDA_GetMaxAttenuation(id);
		printf(WARN "step size was to large, reduced to MaxAttenuation size: %d\n", ud->ramp_steps);
	}

	if (ud->start_att < ud->end_att) {
		if (ud->ramp_steps > (ud->end_att - ud->start_att)) 
			ud->ramp_steps = ud->end_att - ud->start_att;
	} else {
		if (ud->ramp_steps > (ud->start_att - ud->end_att))
			ud->ramp_steps = ud->start_att - ud->end_att;
	}
}

/*
 * calculate number of steps for triangle and ramp function
 * @param ud: user data struct
 * @return: returns number of steps to get from low to high
 */
int
calc_nr_steps(struct user_data *ud)
{
	if (ud->start_att < ud->end_att)
		return ((ud->end_att - ud->start_att) / ud->ramp_steps);
	else
		return ((ud->start_att - ud->end_att) / ud->ramp_steps);
}

/*
 * keep attenuation for given time
 * @param ud: user data struct
 */
void
attenuation_time(struct user_data *ud)
{
	if (ud->us == 1)
		susleep(TIME_MICROS(ud->atime));
	else if (ud->ms == 1)
		susleep(TIME_MILLIS(ud->atime));
	else
		susleep(TIME_SECONDS(ud->atime));
}

/*
 * checks if attenutaion is outside of devices limits and sets
 * attenuation stepwise up or down to get a ramp like form
 * @param id: device id
 * @param ud: user data struct
 * @return: returns 1 on error else 0
 */
int
set_ramp(int id, struct user_data *ud)
{
	int i, cur_att, nr_steps, serial;
	serial = fnLDA_GetSerialNumber(id);
	check_att_limits(id, serial, ud, RAMP);
	
	check_stepsize(ud, id);
	nr_steps = calc_nr_steps(ud);
	
	if (!nr_steps) {
		printf(ERR "start and end attenuation are equal\n");
		return 1;
	}

	if (ud->cont && (ud->start_att < ud->end_att)) {
		for(;;) {
			fnLDA_SetAttenuation(id, ud->start_att);
			log_attenuation(ud->start_att, ud);
			for(i = 0; i < nr_steps; i++) {
				attenuation_time(ud);
				cur_att = fnLDA_GetAttenuation(id);
				fnLDA_SetAttenuation(id,
					cur_att + ud->ramp_steps);
				if (!ud->quiet)
					printf(INFO "attenuation set to %.2fdB\n",
						((double)cur_att) / MULTIPLIER_STEP);
				log_attenuation(cur_att + ud->ramp_steps, ud);
			}
			cur_att = fnLDA_GetAttenuation(id);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n", ((double)cur_att) / MULTIPLIER_STEP);
		}
	}
	if (ud->cont && (ud->start_att > ud->end_att)) {
		for(;;) {
			fnLDA_SetAttenuation(id, ud->start_att);
			log_attenuation(ud->start_att, ud);
			for(i = 0; i < nr_steps; i++) {
				attenuation_time(ud);
				cur_att = fnLDA_GetAttenuation(id);
				fnLDA_SetAttenuation(id,
					cur_att - ud->ramp_steps);
				if (!ud->quiet)
					printf(INFO "attenuation set to %.2fdB\n",
						((double)cur_att) / MULTIPLIER_STEP);
				log_attenuation(cur_att - ud->ramp_steps, ud);
			}
			cur_att = fnLDA_GetAttenuation(id);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n",
					((double)cur_att) / MULTIPLIER_STEP);
		}
	}
	if (ud->start_att < ud->end_att) {
		fnLDA_SetAttenuation(id, ud->start_att);
		log_attenuation(ud->start_att, ud);
		for(i = 0; i < nr_steps; i++) {
			attenuation_time(ud);
			cur_att = fnLDA_GetAttenuation(id);
			fnLDA_SetAttenuation(id,
				cur_att + ud->ramp_steps);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n",
					((double)cur_att) / MULTIPLIER_STEP);
			log_attenuation(cur_att + ud->ramp_steps, ud);
		}
	}
	if (ud->start_att > ud->end_att) {
		fnLDA_SetAttenuation(id, ud->start_att);
		log_attenuation(ud->start_att, ud);
		for(i = 0; i < nr_steps; i++) {
			attenuation_time(ud);
			cur_att = fnLDA_GetAttenuation(id);
			fnLDA_SetAttenuation(id,
				cur_att - ud->ramp_steps);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n",
					((double)cur_att) / MULTIPLIER_STEP);
			log_attenuation(cur_att - ud->ramp_steps, ud);
		}
	}
	attenuation_time(ud);
	cur_att = fnLDA_GetAttenuation(id);
	if (!ud->quiet)
		printf(INFO "attenuation set to %.2fdB\n",
			((double)cur_att) / MULTIPLIER_STEP);

	return 0;
}

/*
 * Sets attenuation to a level defined by user if
 * not above Max or below Min attenuation of the connected
 * attenuator. The Device will keep the attenuation for the
 * time given by the user or the standard sleeptime.
 * After the given time the attenuation is set to 0 again.
 * @param id: device id
 * @param ud: user data struct
 */
void
set_attenuation(int id, struct user_data *ud)
{
	int serial;

	serial = fnLDA_GetSerialNumber(id);
	check_att_limits(id, serial, ud, SIMPLE);
	attenuation_time(ud);
}

/*
 * Set attenuation stepwise from start attenuation to end attenuation and
 * log it.
 * @param id: device id
 * @param ud: user data struct
 * @return: returns 1 on error else 0
 */
int
set_triangle(int id, struct user_data *ud)
{
	int i, cur_att, nr_steps, serial;
	serial = fnLDA_GetSerialNumber(id);
	check_att_limits(id, serial, ud, TRIANGLE);

	check_stepsize(ud, id);
	nr_steps = calc_nr_steps(ud);

	if (!nr_steps) {
		printf(ERR "start and end attenuation are equal\n");
		return 1;
	}

	fnLDA_SetAttenuation(id, ud->start_att);
	log_attenuation(ud->start_att, ud);
	if (ud->cont && (ud->start_att < ud->end_att)) {
		for(;;) {
			for (i = 0; i < nr_steps; i++) {
				attenuation_time(ud);
				cur_att = fnLDA_GetAttenuation(id);
				if (!ud->quiet)
					printf(INFO "attenuation set to %.2fdB\n",
						((double)cur_att) / MULTIPLIER_STEP);
				fnLDA_SetAttenuation(id,
					cur_att + ud->ramp_steps);
				log_attenuation(cur_att + ud->ramp_steps, ud);
			}
			for (i = 1; i <= nr_steps; i++) {
				attenuation_time(ud);
				cur_att = fnLDA_GetAttenuation(id);
				if (!ud->quiet)
					printf(INFO "attenuation set to %.2fdB\n",
						((double)cur_att) / MULTIPLIER_STEP);
				fnLDA_SetAttenuation(id,
					cur_att - ud->ramp_steps);
				log_attenuation(cur_att - ud->ramp_steps, ud);
			}
			fnLDA_SetAttenuation(id, ud->start_att);
			log_attenuation(ud->start_att, ud);
		}
	}
	if (ud->start_att < ud->end_att) {
		for (i = 0; i < nr_steps; i++) {
			attenuation_time(ud);
			cur_att = fnLDA_GetAttenuation(id);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n",
					((double)cur_att) / MULTIPLIER_STEP);
			fnLDA_SetAttenuation(id, cur_att + ud->ramp_steps);
			log_attenuation(cur_att + ud->ramp_steps, ud);
		}
		for (i = 1; i <= nr_steps; i++) {
			attenuation_time(ud);
			cur_att = fnLDA_GetAttenuation(id);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n",
					((double)cur_att) / MULTIPLIER_STEP);
			fnLDA_SetAttenuation(id, cur_att - ud->ramp_steps);
			log_attenuation(cur_att - ud->ramp_steps, ud);
		}
		fnLDA_SetAttenuation(id, ud->start_att);
		log_attenuation(ud->start_att, ud);
	}
	if (ud->cont && (ud->start_att > ud->end_att)) {
		for(;;) {
			for (i = 0; i < nr_steps; i++) {
				attenuation_time(ud);
				cur_att = fnLDA_GetAttenuation(id);
				if (!ud->quiet)
					printf(INFO "attenuation set to %.2fdB\n",
						((double)cur_att) / MULTIPLIER_STEP);
				fnLDA_SetAttenuation(id,
					cur_att - ud->ramp_steps);
				log_attenuation(cur_att - ud->ramp_steps, ud);
			}
			for (i = 1; i <= nr_steps; i++) {
				attenuation_time(ud);
				cur_att = fnLDA_GetAttenuation(id);
				if (!ud->quiet)
					printf(INFO "attenuation set to %.2fdB\n",
						((double)cur_att) / MULTIPLIER_STEP);
				fnLDA_SetAttenuation(id,
					cur_att + ud->ramp_steps);
				log_attenuation(cur_att + ud->ramp_steps, ud);
			}
			fnLDA_SetAttenuation(id, ud->start_att);
			log_attenuation(ud->start_att, ud);
		}
	}
	if (ud->start_att > ud->end_att) {
		for (i = 0; i < nr_steps; i++) {
			attenuation_time(ud);
			cur_att = fnLDA_GetAttenuation(id);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n",
					((double)cur_att) / MULTIPLIER_STEP);
			fnLDA_SetAttenuation(id, cur_att - ud->ramp_steps);
			log_attenuation(cur_att - ud->ramp_steps, ud);
		}
		for (i = 1; i <= nr_steps; i++) {
			attenuation_time(ud);
			cur_att = fnLDA_GetAttenuation(id);
			if (!ud->quiet)
				printf(INFO "attenuation set to %.2fdB\n",
					((double)cur_att) / MULTIPLIER_STEP);
			fnLDA_SetAttenuation(id, cur_att + ud->ramp_steps);
			log_attenuation(cur_att + ud->ramp_steps, ud);
		}
		fnLDA_SetAttenuation(id, ud->start_att);
		log_attenuation(ud->start_att, ud);
	}
	attenuation_time(ud);
	cur_att = fnLDA_GetAttenuation(id);
	if (!ud->quiet)
		printf(INFO "attenuation set to %.2fdB\n", ((double)cur_att) / MULTIPLIER_STEP);
	return 0;
}

/*
 * allocate memory for user data struct
 * return: allocated user data struct address
 */
struct user_data *
allocate_user_data(void)
{
	struct user_data *ud = malloc(sizeof(struct user_data));

	if (ud == NULL) {
		printf(ERR "could not allocate memory for user data\n");
		exit(1);
	}
	return ud;
}

/*
 * Set device as specified by user
 * @param ud: user data struct
 * @param id: device id
 */
void
set_data(struct user_data *ud, int id)
{
	unsigned int i;
	int res = 0;
	if (ud->simple == 1) {
		set_attenuation(id, ud);
	} else if (ud->triangle && ud->cont) {
		for(;;) {
			res = set_triangle(id, ud);
			if (res)
				return;
		}
	} else if (ud->triangle && ud->runs >= 1) {
		for(i = 0; i < ud->runs; i++) {
			res = set_triangle(id, ud);
			if (res)
				return;
		}
	} else if (ud->ramp && ud->cont) {
		for(;;) {
			res = set_ramp(id, ud);
			if (res)
				return;
		}
	} else if (ud->ramp && ud->runs >= 1) {
		for(i = 0; i < ud->runs; i++) {
			res = set_ramp(id, ud);
			if (res)
				return;
		}
	} else if (ud->file && ud->cont) {
		while (res == 0)
			res = read_file(ud->path, id, ud);
	} else if (ud->file && (ud->runs > 1)) {
		while (res == 0)
			res = read_file(ud->path, id, ud);
	} else if (ud->file && (ud->cont == 0)) {
		read_file(ud->path, id, ud);
	}

	if (ud->atime != 0) {
		fnLDA_SetAttenuation(id, 0);
		log_attenuation(0, ud);
	}
}

/*
 * check if the user wants to use multiple attenuators
 * @param argc: argument count
 * @param *argv: arguments passed to the program
 * return returns 1 or 2 on multiple devices, else 0
 */
int
check_multi_device(char *argv[])
{
	if (strncmp(argv[1], "-md", strlen(argv[1])) == 0)
		return 1;
	else if (strncmp(argv[1], "-mds", strlen(argv[1])) == 0)
		return 2;
	else
		return 0;
}

/*
 * get instructions for attenuator from file and start it
 * @param arguments: pthread argument struct
 */
void *
start_device(void *arguments)
{
	char *path;
	int id;
	struct thread_arguments *args = arguments;
	struct user_data *ud = allocate_user_data();
	clear_userdata(ud);

	path = args->path;
	id = args->id;

	pthread_mutex_unlock(&device_mutex);

	read_file(path, id, ud);
	free(ud);
	pthread_exit((void *)(intptr_t)args->id);
}

/*
 * close specific device
 * @param id: device id
 * @param working_devices: array of active devices
 */
void
close_single_device(int id, DEVID *working_devices, int quiet)
{
	int status, serial = 0;

	status = fnLDA_CloseDevice(working_devices[id - 1]);
	serial = fnLDA_GetSerialNumber(working_devices[id - 1]);
	if (status != 0) {
		printf(ERR "shutting down device %d (serial %i) failed\n",
		       id, serial);
	} else if (!quiet) {
		printf(INFO "shut down of device %d (serial %i) "
		       "was successful\n", id, serial);
	}
}

/*
 * close any open devices
 * @param nr_active_devices: number of active devices
 * @param working_devices: array of active devices
 */
void
close_devices(int nr_active_devices, DEVID *working_devices, int quiet)
{
	int i, status, serial = 0;

	for (i = 1; i <= nr_active_devices; i++) {
		status = fnLDA_CloseDevice(working_devices[i - 1]);
		serial = fnLDA_GetSerialNumber(working_devices[i - 1]);
		if (status != 0) {
			printf(ERR "shutting down device %d (serial %i) failed\n",
			       i, serial);
		} else if (!quiet) {
			printf(INFO "shut down of device %d (serial %i) "
			       "was successful\n", i, serial);
		}
	}
}

/*
 * manage termination signal
 * @param sig: signal type
 */
void sighandler(int sig)
{
	DEVID working_devices[MAXDEVICES];
	int nr_active_devices;

	nr_active_devices = fnLDA_GetDevInfo(working_devices);
	close_devices(nr_active_devices, working_devices, 0);
	exit(0);
}

/*
 * check if quiet flag is enabled
 * @param argc: argument count
 * @param argv: array of function arguments
 * @return returns 1 if -q is set else 0
 */
int
check_quiet(int argc, char *argv[])
{
	int i = 0;
	for (;i < argc; i++)
		if (strncmp(argv[i], "-q", strlen(argv[i])) == 0)
			return 1;
	return 0;
}

/*
 * check if serial number flag is enabled
 * @param argc: argument count
 * @param argv: array of function arguments
 * @return returns 1 if -n is set else 0
 */
int
check_serial_number(int argc, char *argv[])
{
	int i = 0;
	for (;i < argc; i++)
		if (strncmp(argv[i], "-n", strlen(argv[i])) == 0)
			return 1;
	return 0;
}

/*
 * check if info flag is enabled
 * @param argc: argument count
 * @param argv: array of function arguments
 * @return returns 1 if -i is set else 0
 */
int
check_info(int argc, char *argv[])
{
	int i = 0;
	for (;i < argc; i++)
		if (strncmp(argv[i], "-i", strlen(argv[i])) == 0)
			return 1;
	return 0;
}

/*
 * start thread for each active device
 * @param argc: argument count
 * @param argv: arguments given by the user
 * @param file_serial_check: flag to parse serial number from filename
 */
void
handle_multi_dev(int argc, char *argv[], int file_serial_check)
{
	struct thread_arguments args;
	pthread_t threads[MAXDEVICES];
	unsigned int device_count = 0;
	int length;
	int i, nr_active_devices, file_count, ret, state, quiet, info, serial;
	DEVID working_devices[MAXDEVICES];
	DEVID id;
	char message[MAX_MSG_SIZE];
	char device_name[MAX_MODELNAME];
	void *status;

	device_count = (unsigned int)fnLDA_GetNumDevices();

	quiet = check_quiet(argc, argv);
	info = check_info(argc, argv);

	if (device_count == 0) {
		printf(ERR "There is no attenuator connected\n");
	} else if (device_count > 1 && !quiet) {
			printf(INFO "There are %d attenuators connected\n", device_count);
	} else if (!quiet) {
			printf(INFO "There is %d attenuator connected\n", device_count);
	}

	nr_active_devices = fnLDA_GetDevInfo(working_devices);
	if (!quiet) {
		get_serial_and_name(device_count, device_name);
		printf(INFO "%d active devices found\n", nr_active_devices);
	}

	/*
	 * initiate devices
	 */
	for (i = 0; i < nr_active_devices; i++) {
		id = working_devices[i];
		state = fnLDA_InitDevice(id);
		serial = fnLDA_GetSerialNumber(id);

		if (state != 0) {
			printf(ERR "initialising device %d (serial %i) failed\n",
			       id, serial);
			continue;
		}
		
		if (!quiet)
			printf(INFO "initialized device %d (serial %i) successfully\n",
			       id, serial);

		if (info)
			print_dev_info(id);
	}

	/*
	 * check devices
	 */
	for (i = 0; i < nr_active_devices; i++) {
		id = working_devices[i];
		serial = fnLDA_GetSerialNumber(id);

		strncpy(message, get_device_data(id),
			MAX_MSG_SIZE - 1);
		if (!strncmp(message,"Successfully checked device\n",
		    strlen(message)) == 0) {
			printf(ERR "check failed for device %d (serial %i)\n", id, serial);
			printf(ERR "%s\n", message);
		}
	}

	/* check number of available files */
	if ((argc - 2) < nr_active_devices)
		file_count = argc - 2;
	else
		file_count = nr_active_devices;

	if (pthread_mutex_init(&device_mutex, NULL)) {
		printf(ERR "Failed to initialize device mutex\n");
		return;
	}

	for (i = 0; i < file_count; i++) {
		pthread_mutex_lock(&device_mutex);
		args.path = argv[i + 2];

		if (file_serial_check) {
			/* Get serial using filename */
			int file_serial_int, tmp_id;
			char *file_serial = malloc(sizeof(char) *
						   MAX_PATH_LENGTH);
			if ((strlen(args.path) - 4) >= MAX_PATH_LENGTH) {
				length = MAX_PATH_LENGTH;
			} else {
				length = strlen(args.path) - 4;
			}
			strncpy(file_serial, args.path, length);
			file_serial_int = atoi(basename(file_serial));
			tmp_id = get_id_by_serial(file_serial_int, device_count);
			free(file_serial);
			if (tmp_id < 0) {
				printf(ERR "Filename %s not matching with any device\n", args.path);
				return;
			}
			args.id = tmp_id;
		} else {
			args.id = i + 1;
		}

		ret = pthread_create(&threads[i], NULL, start_device, (void *)&args);
		if (ret)
			printf(ERR "Failed to create thread! Error Code: %d\n", ret);
	}

	for (i = 0; i < file_count; i++) {
		ret = pthread_join(threads[i], &status);

		if (ret)
			printf(ERR "Failed to join thread! Error Code: %d\n", ret);
	}

	close_devices(nr_active_devices, working_devices, quiet);
	return;
}

/*
 * handle single connected device
 * @param ud: user data struct
 * @param argc: argument count
 * @param argv: arguments given by the user
 * @param working_devices: array of active devices
 * @param get_serial: check if we need to get device id from serial number
 * return: 0 on success, 1 on error
 */
int
handle_single_dev(struct user_data *ud, int argc, char *argv[], DEVID *working_devices,
		  int get_serial, int device_count)
{
	int status, id, serial;
	char message[64];
	char *version;

	clear_userdata(ud);

	if (!get_parameters(argc, argv, ud)) {
		printf(WARN "Usage: %s [options]\n", argv[0]);
		call_help();
		exit(1);
	}

	if (get_serial) {
		id = get_id_by_serial(ud->serial_number, device_count);
		if (id < 0) {
			printf(ERR "unable to find device with serial %i\n", ud->serial_number);
			return 0;
		}
	} else {
		id = SINGLE_DEV_ID;
	}
	
	if (!ud->quiet) {
		version = fnLDA_LibVersion();
		printf(INFO "you are using libversion %s\n", version);
	}

	status = fnLDA_InitDevice(working_devices[id - 1]);
	serial = fnLDA_GetSerialNumber(working_devices[id - 1]);

	if (status != 0) {
		printf(ERR "initialising device %d (serial %i) failed\n",
		       id, serial);
		return 0;
	}
	else
		if (!ud->quiet)
			printf(INFO "initialized device %d (serial %i) successfully\n",
			       id, serial);

	if (ud->info)
		print_dev_info(id);

	strncpy(message, get_device_data(working_devices[id - 1]),
		MAX_MSG_SIZE - 1);
	if (strncmp(message,"Successfully checked device\n",
	    strlen(message)) == 0) {
		if (!ud->quiet)
			printf(INFO "%s",message);
	} else {
		printf(ERR "check failed for the device (serial %i)\n", serial);
		printf(ERR "%s\n", message);
		return 0;
	}

	set_data(ud, id);
	close_single_device(id, working_devices, ud->quiet);
	return 1;
}

/*
 * returns 0 on success, 1 on error
 */
int
main(int argc, char *argv[])
{
	int device_count, get_serial, mdc = 0;
	int nr_active_devices, quiet;
	DEVID working_devices[MAXDEVICES];
	char device_name[MAX_MODELNAME];

	/* get the uid of caller */
	uid_t uid = geteuid();
	fnLDA_Init();

	fnLDA_SetTestMode(FALSE);
	quiet = check_quiet(argc, argv);

	if (uid != 0) {
		printf(ERR "This tool needs to be run as root to access USB ports\n");
		printf("Please run again as root\n");
		exit(1);
	}

	if (argc < 2) {
		printf(ERR "Usage: %s [options]\n", argv[0]);
		call_help();
		exit(1);
	}

	if ((strncmp(argv[1], "-h", strlen(argv[1]))) == 0) {
		call_help();
		exit(0);
	}

	/* Manage termination signal */
	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGABRT, sighandler);

	mdc = check_multi_device(argv);
	if (mdc) {
		if (!quiet)
			printf(INFO "multidevice support enabled\n");
		if (mdc == 2)
			handle_multi_dev(argc, argv, 1);
		else
			handle_multi_dev(argc, argv, 0);
		exit(0);
	}

	struct user_data *ud = allocate_user_data();
	device_count = fnLDA_GetNumDevices();

	if (device_count == 0) {
		printf(ERR "There is no attenuator connected\n");
	} else if (device_count > 1) {
		if (!quiet)
			printf(INFO "There are %d attenuators connected\n", device_count);
	} else {
		if (!quiet)
			printf(INFO "There is %d attenuator connected\n", device_count);
	}

	if (!quiet)
		get_serial_and_name(device_count, device_name);

	nr_active_devices = fnLDA_GetDevInfo(working_devices);
	if (!quiet)
		printf(INFO "%d active device(s) found\n", nr_active_devices);

	get_serial = check_serial_number(argc, argv);
	handle_single_dev(ud, argc, argv, working_devices, get_serial, device_count);

	free(ud);
	return 0;
}

