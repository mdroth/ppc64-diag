#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encl_common.h"
#include "slider.h"
#include "test_utils.h"

#define TEMPERATURE_OFFSET	20

extern struct slider_lff_diag_page2 healthy_page;
static struct slider_lff_diag_page2 page;

int main(int argc, char **argv)
{
	int i;
	struct fan_status *fs;
	struct slider_power_supply_status *ps;
	struct slider_voltage_sensor_status *vs;
	struct slider_temperature_sensor_status *ts;

	if (argc != 2) {
		fprintf(stderr, "usage: %s pathname\n", argv[0]);
		exit(1);
	}

	convert_htons(&healthy_page.page_length);
	convert_htons(&healthy_page.overall_input_power_status.input_power);

	for (i = 0; i < SLIDER_NR_INPUT_POWER; i++)
		convert_htons(&healthy_page.input_power_element[i].input_power);

	for (i = 0; i < SLIDER_NR_ESC; i++) {
		convert_htons
		(&healthy_page.voltage_sensor_sets[i].sensor_3V3.voltage);
		convert_htons
		(&healthy_page.voltage_sensor_sets[i].sensor_1V0.voltage);
		convert_htons
		(&healthy_page.voltage_sensor_sets[i].sensor_1V8.voltage);
		convert_htons
		(&healthy_page.voltage_sensor_sets[i].sensor_0V92.voltage);
	}

	memcpy(&page, &healthy_page, sizeof(page));

	page.non_crit = 1;

	ps = &page.ps_status[1];
	ps->byte0.status = ES_NONCRITICAL;
	ps->dc_over_voltage = 1;
	ps->fail = 1;
	slider_roll_up_power_supply_status(&page);

	vs = &page.voltage_sensor_sets[1].sensor_3V3;
	vs->byte0.status = ES_NONCRITICAL;
	vs->warn_over = 1;
	slider_roll_up_voltage_sensor_status(&page);

	fs = &page.fan_sets[1].fan_element[0];
	fs->byte0.status = ES_NONCRITICAL;
	fs->fail = 1;
	fs->speed_code = 1;
	slider_roll_up_fan_status(&page);

	ts = &page.temp_sensor_sets.temp_enclosure;
	ts->byte0.status = ES_NONCRITICAL;
	ts->ot_warning = 1;
	ts->temperature = TEMPERATURE_OFFSET + 60;
	slider_roll_up_temperature_sensor_status(&page);
	page.overall_temp_sensor_status.temperature =
		slider_mean_temperature(&page);

	if (write_page2_to_file(argv[1], &page, sizeof(page)) != 0)
		exit(2);

	exit(0);
}
