#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "acc_definitions.h"
#include "acc_detector_distance_basic.h"
#include "acc_driver_hal.h"
#include "acc_rss.h"
#include "acc_version.h"

const double period_sec = 0.1; // 実行周期[s]

/**
 * @brief Example that shows how to use the distance basic detector
 *
 * This is an example on how the envelope service can be used.
 * The example executes as follows:
 *   - Initiate HAL layer
 *   - Activate Radar System Software (RSS)
 *   - Create a distance basic detector
 *   - Get the result and print it forever
 *   - Destroy the distance basic detector
 *   - Deactivate Radar System Software (RSS)
 */

typedef struct
{
	uint32_t count;
	acc_sensor_id_t sensor;
	float range_start;
	float range_length;
} input_t;

static bool parse_input(int argc, char *argv[], input_t *input);

static acc_hal_t hal;

int main(int argc, char *argv[])
{
	// ログファイル名の生成処理
	FILE *fp;
	char filename[64] = "data.csv";
	char time_str[32] = "00_00_00_00_00";

	time_t t = time(NULL);
	strftime(time_str, sizeof(time_str), "%m_%d_%H_%M_%S", localtime(&t));

	sprintf(filename, "log/%s.csv", time_str);
	printf("Log will be saved to \"%s\"\n", filename);

	// ファイルを開く
	if ((fp = fopen(filename, "w")) == NULL)
	{
		fprintf(stderr, "ファイルのオープンに失敗しました.\n");
		return EXIT_FAILURE;
	}

	input_t input;
	if (!acc_driver_hal_init())
	{
		return EXIT_FAILURE;
	}

	printf("Acconeer software version %s\n", ACC_VERSION);
	printf("Acconeer RSS version %s\n", acc_rss_version());

	if (!parse_input(argc, argv, &input))
	{
		return EXIT_FAILURE;
	}

	hal = acc_driver_hal_get_implementation();

	hal.log.log_level = ACC_LOG_LEVEL_ERROR;

	if (!acc_rss_activate_with_hal(&hal))
	{
		return EXIT_FAILURE;
	}

	acc_detector_distance_basic_handle_t *handle = acc_detector_distance_basic_create(input.sensor, input.range_start, input.range_length);
	if (handle == NULL)
	{
		fprintf(stderr, "acc_detector_distance_basic_create() failed\n");
		return EXIT_FAILURE;
	}

	acc_detector_distance_basic_reflection_t reflection;

	uint32_t count = input.count;

	while ((input.count == 0) || count > 0)
	{
		reflection = acc_detector_distance_basic_get_reflection(handle);
		double sec = (double)count * period_sec;
		double dis = (double)reflection.distance;
		int amp = reflection.amplitude;

		printf("time:%5.2lf, dis: %f[m], amp:%u\n", sec, dis, amp);
		fprintf(fp, "%5.2lf, %f, %u\n", sec, dis, amp);

		hal.os.sleep_us(period_sec * 1000000);

		count++;
	}

	fclose(fp);

	acc_detector_distance_basic_destroy(&handle);
	acc_rss_deactivate();

	return EXIT_SUCCESS;
}

bool parse_input(int argc, char *argv[], input_t *input)
{
	input->count = 0;
	input->sensor = 1;
	input->range_start = 0.2f;
	input->range_length = 0.4f;

	static struct option long_options[] =
		{
			{"count", required_argument, 0, 'c'},
			{"range-start", required_argument, 0, 'r'},
			{"range-length", required_argument, 0, 'l'},
			{"sensor", required_argument, 0, 's'},
			{"help", no_argument, 0, 'h'},
			{NULL, 0, NULL, 0}};

	int character_code;
	int option_index = 0;

	while ((character_code = getopt_long(argc, argv, "c:r:l:s:h?", long_options, &option_index)) != -1)
	{
		switch (character_code)
		{
		case 'c':
		{
			input->count = atoi(optarg);
			break;
		}

		case 'r':
		{
			input->range_start = strtof(optarg, NULL);
			break;
		}

		case 'l':
		{
			input->range_length = strtof(optarg, NULL);
			break;
		}

		case 's':
		{
			input->sensor = atoi(optarg);
			break;
		}

		case 'h':
		case '?':
		{
			fprintf(stderr, "-c, --count         The number of reflections to get, 0 is infinite, default 0\n");
			fprintf(stderr,
					"-r, --range-start   The start range in meters where the detector will look for reflections, default 0.2\n");
			fprintf(stderr,
					"-l, --range-length  The range length in meters where the detector will look for reflections, default 0.2\n");
			fprintf(stderr, "-s, --sensor        The position where the sensor is connected, default 1\n");
			return false;
		}
		}
	}

	return true;
}
