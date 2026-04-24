#include "model_runner.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cinttypes>

#include "esp_log.h"
#include "model_data.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char *TAG = "model_runner";

static constexpr size_t kTensorArenaSize = 64 * 1024;
static uint8_t g_tensor_arena[kTensorArenaSize];

static const tflite::Model *g_model = nullptr;
static tflite::MicroMutableOpResolver<10> g_op_resolver;

static tflite::MicroInterpreter *g_interpreter = nullptr;
static TfLiteTensor *g_input = nullptr;
static TfLiteTensor *g_output = nullptr;

static bool g_initialized = false;

esp_err_t model_runner_init(void)
{
    if (g_initialized) {
        return ESP_OK;
    }

    g_model = tflite::GetModel(edgeguard_small_person_classifier_int8);
    if (g_model == nullptr) {
        ESP_LOGE(TAG, "GetModel returned null");
        return ESP_FAIL;
    }

    if (g_model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "model schema mismatch: model=%" PRIu32 " runtime=%" PRIu32,
                 static_cast<uint32_t>(g_model->version()),
                 static_cast<uint32_t>(TFLITE_SCHEMA_VERSION));
        return ESP_FAIL;
    }

    if (g_op_resolver.AddConv2D() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddMaxPool2D() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddMean() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddFullyConnected() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddLogistic() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddMul() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddQuantize() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddDequantize() != kTfLiteOk) return ESP_FAIL;
    if (g_op_resolver.AddReshape() != kTfLiteOk) return ESP_FAIL;

    static tflite::MicroInterpreter static_interpreter(
        g_model,
        g_op_resolver,
        g_tensor_arena,
        kTensorArenaSize
    );
    g_interpreter = &static_interpreter;

    if (g_interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors failed");
        return ESP_FAIL;
    }

    g_input = g_interpreter->input(0);
    g_output = g_interpreter->output(0);

    if (g_input == nullptr || g_output == nullptr) {
        ESP_LOGE(TAG, "input/output tensor is null");
        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "model ready | input=(%d,%d,%d,%d) type=%d output_type=%d arena=%u used=%u",
        g_input->dims->data[0],
        g_input->dims->data[1],
        g_input->dims->data[2],
        g_input->dims->data[3],
        g_input->type,
        g_output->type,
        (unsigned)kTensorArenaSize,
        (unsigned)g_interpreter->arena_used_bytes()
    );

    g_initialized = true;
    return ESP_OK;
}

esp_err_t model_runner_get_info(model_runner_info_t *info)
{
    if (!g_initialized || info == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    info->initialized = true;
    info->input_width = g_input->dims->data[2];
    info->input_height = g_input->dims->data[1];
    info->input_channels = g_input->dims->data[3];
    info->input_scale = g_input->params.scale;
    info->input_zero_point = g_input->params.zero_point;
    info->output_scale = g_output->params.scale;
    info->output_zero_point = g_output->params.zero_point;
    info->tensor_arena_size = kTensorArenaSize;

    return ESP_OK;
}

esp_err_t model_runner_infer_u8(const uint8_t *input_u8, size_t input_len, float *person_score)
{
    if (!g_initialized || g_input == nullptr || g_output == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (input_u8 == nullptr || person_score == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const int expected_len =
        g_input->dims->data[1] *
        g_input->dims->data[2] *
        g_input->dims->data[3];

    if ((int)input_len != expected_len) {
        ESP_LOGE(TAG, "input length mismatch: got=%u expected=%d",
                 (unsigned)input_len, expected_len);
        return ESP_ERR_INVALID_SIZE;
    }

    if (g_input->type != kTfLiteInt8) {
        ESP_LOGE(TAG, "unexpected input type=%d", g_input->type);
        return ESP_FAIL;
    }

    const float scale = g_input->params.scale;
    const int zero_point = g_input->params.zero_point;

    for (int i = 0; i < expected_len; ++i) {
        const float real_value = static_cast<float>(input_u8[i]);
        int32_t q = static_cast<int32_t>(std::lround(real_value / scale)) + zero_point;
        if (q < -128) q = -128;
        if (q > 127) q = 127;
        g_input->data.int8[i] = static_cast<int8_t>(q);
    }

    if (g_interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke failed");
        return ESP_FAIL;
    }

    if (g_output->type == kTfLiteInt8) {
        const int8_t q = g_output->data.int8[0];
        *person_score = (static_cast<int>(q) - g_output->params.zero_point) * g_output->params.scale;
        return ESP_OK;
    }

    if (g_output->type == kTfLiteFloat32) {
        *person_score = g_output->data.f[0];
        return ESP_OK;
    }

    ESP_LOGE(TAG, "unexpected output type=%d", g_output->type);
    return ESP_FAIL;
}