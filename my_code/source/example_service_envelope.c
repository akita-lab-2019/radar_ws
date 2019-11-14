// Copyright (c) Acconeer AB, 2015-2019
// All rights reserved

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc_driver_hal.h"
#include "acc_rss.h"
#include "acc_service.h"
#include "acc_service_envelope.h"
#include "acc_sweep_configuration.h"

#include "acc_version.h"

/**
 * @brief Example that shows how to use the envelope service
 *
 * This is an example on how the envelope service can be used.
 * The example executes as follows:
 *   - Activate Radar System Software (RSS)
 *   - Create an envelope service configuration (with blocking mode as default)
 *   - Create an envelope service using the previously created configuration
 *   - Activate the envelope service
 *   - Get the result and print it 5 times, where the last result is intentionally late
 *   - Create and activate envelope service
 *   - Destroy the envelope service configuration
 *   - Deactivate Radar System Software
 */

static acc_service_status_t execute_envelope_with_blocking_calls(acc_service_configuration_t envelope_configuration);

static void configure_sweeps(acc_service_configuration_t envelope_configuration);

static acc_hal_t hal;

FILE *fp;

int main(void)
{
    // ログファイル名の生成処理
    char filename[64] = "data.csv";
    char time_str[32] = "00_00_00_00_00";

    time_t t = time(NULL);
    strftime(time_str, sizeof(time_str), "%m_%d_%H_%M_%S", localtime(&t));

    sprintf(filename, "log/envelope_%s.csv", time_str);
    printf("Log will be saved to \"%s\"\n", filename);

    // ファイルを開く
    if ((fp = fopen(filename, "w")) == NULL)
    {
        fprintf(stderr, "ファイルのオープンに失敗しました.\n");
        return EXIT_FAILURE;
    }

    if (!acc_driver_hal_init())
    {
        return EXIT_FAILURE;
    }

    printf("Acconeer software version %s\n", ACC_VERSION);

    hal = acc_driver_hal_get_implementation();

    if (!acc_rss_activate_with_hal(&hal))
    {
        return EXIT_FAILURE;
    }

    // サービスコンフィグレーションを包絡線用に定義
    acc_service_configuration_t envelope_configuration = acc_service_envelope_configuration_create();
    if (envelope_configuration == NULL)
    {
        printf("acc_service_envelope_configuration_create()に失敗\n");
        return EXIT_FAILURE;
    }

    // スイープの設定
    configure_sweeps(envelope_configuration);

    // ブロッキングコールでエンベロープを実行する
    acc_service_status_t service_status;
    service_status = execute_envelope_with_blocking_calls(envelope_configuration);

    if (service_status != ACC_SERVICE_STATUS_OK)
    {
        printf("execute_envelope_with_blocking_calls() => (%u) %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
        acc_service_envelope_configuration_destroy(&envelope_configuration);
        return EXIT_FAILURE;
    }

    acc_service_envelope_configuration_destroy(&envelope_configuration);

    acc_rss_deactivate();

    return EXIT_SUCCESS;
}

// ブロッキングコールでエンベロープを実行する
acc_service_status_t execute_envelope_with_blocking_calls(acc_service_configuration_t envelope_configuration)
{
    // ハンドルの生成
    acc_service_handle_t handle = acc_service_create(envelope_configuration);
    if (handle == NULL)
    {
        printf("acc_service_createに失敗\n");
        return ACC_SERVICE_STATUS_FAILURE_UNSPECIFIED;
    }

    // メタデータの取得
    acc_service_envelope_metadata_t envelope_metadata;
    acc_service_envelope_get_metadata(handle, &envelope_metadata);
    double measure_start_dis = (double)envelope_metadata.actual_start_m;
    double measure_len = (double)envelope_metadata.actual_length_m;
    double measure_end_dis = (double)(measure_start_dis + measure_len);
    uint16_t data_len = envelope_metadata.data_length;
    double index_to_meter = (double)(measure_len / data_len); // [m / point]

    printf("始点    : %f [m]\n", measure_start_dis);
    printf("測定距離: %f [m]\n", measure_len);
    printf("終点    : %f [m]\n", measure_end_dis);
    printf("データ長: %u    \n", data_len);
    printf("分解能  : %f [m / point]\n", index_to_meter);

    uint16_t data[data_len];

    // センサの状態を取得
    acc_service_envelope_result_info_t result_info;
    acc_service_status_t service_status = acc_service_activate(handle);

    // センサの状態に応じて処理を振り分け
    if (service_status == ACC_SERVICE_STATUS_OK)
    {
        // センサがアクティブ
        // 5回測定してみる
        uint_fast8_t sweep_count = 5;
        while (sweep_count > 0)
        {
            // センサから包絡線データを取得する
            // この関数は，次のスイープがセンサーから到着し，包絡線データが「data」配列にコピーされるまでブロックする
            service_status = acc_service_envelope_get_next(handle, data, data_len, &result_info);

            if (service_status == ACC_SERVICE_STATUS_OK)
            {
                // 計測データの表示
                // printf("包絡線データシーケンス番号: %u\n", (unsigned int)result_info.sequence_number);
                // printf("包絡線データ:\n");
                for (uint_fast16_t index = 0; index < data_len; index++)
                {
                    double now_depth = measure_start_dis + index * index_to_meter; // [m]
                    // // 8データおきに改行
                    // if (index && !(index % 8))
                    // {
                    //     printf("\n");
                    // }

                    printf("%6u", (unsigned int)(data[index]));

                    // CSVに書き込むのは最後の測定の結果のみ
                    if (sweep_count == 1)
                    {
                        fprintf(fp, "%f, %u\n", now_depth, data[index]);
                    }
                }

                printf("\n");
            }
            else
            {
                printf("エンベロープデータが正しく取得されませんでした\n");
            }

            sweep_count--;

            // Show what happens if application is late
            // アプリが遅れた場合，なにが起こるのかを見せる
            // ＝最後だけワンテンポ時間をおいて測定してみる
            if (sweep_count == 1)
            {
                hal.os.sleep_us(200000);
            }
        }

        // 測定を終了
        service_status = acc_service_deactivate(handle);
    }
    else
    {
        printf("acc_service_activate() %u => %s\n", (unsigned int)service_status, acc_service_status_name_get(service_status));
    }

    acc_service_destroy(&handle);

    return service_status;
}

// スイープの設定
void configure_sweeps(acc_service_configuration_t envelope_configuration)
{
    acc_sweep_configuration_t sweep_configuration = acc_service_get_sweep_configuration(envelope_configuration);

    if (sweep_configuration == NULL)
    {
        printf("スイープ構成は利用できません\n");
    }
    else
    {
        float start_m = 0.1f;
        float length_m = 0.5f;
        float update_rate_hz = 100;
        // Actual start  : 99  mm
        // Actual length : 500 mm
        // Actual end    : 600 mm
        // Data length   : 1034

        // サンプルコードのデフォルト値
        // float start_m = 0.4f;
        // float length_m = 0.5f;
        // float update_rate_hz = 100;
        // Actual start  : 400 mm
        // Actual length : 499 mm
        // Actual end    : 899 mm
        // Data length   : 1033

        acc_sweep_configuration_requested_start_set(sweep_configuration, start_m);
        acc_sweep_configuration_requested_length_set(sweep_configuration, length_m);
        acc_sweep_configuration_repetition_mode_streaming_set(sweep_configuration, update_rate_hz);
    }
}
