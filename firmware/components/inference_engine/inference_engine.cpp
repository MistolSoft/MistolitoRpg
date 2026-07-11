#include "inference_engine.h"
#include "esp_log.h"
#include "dl_model_base.hpp"
#include "fbs_model.hpp"
#include "dl_module_creator.hpp"
#include <math.h>

static const char *TAG = "INFERENCE";

static dl::Model *g_model = nullptr;
static dl::TensorBase *g_input_tensor = nullptr;
static dl::TensorBase *g_output_tensor = nullptr;
static bool g_model_built = false;

extern "C" {

esp_err_t inference_engine_init(void)
{
    dl::module::ModuleCreator::get_instance()->register_dl_modules();

    g_model = nullptr;
    g_input_tensor = nullptr;
    g_output_tensor = nullptr;
    g_model_built = false;

    ESP_LOGI(TAG, "Inference engine initialized (ESP-DL)");
    return ESP_OK;
}

esp_err_t inference_engine_load_model(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_model) {
        inference_engine_unload();
    }

    dl::module::ModuleCreator::get_instance()->register_dl_modules();

    ESP_LOGI(TAG, "Loading ESP-DL model from: %s", path);

    g_model = new dl::Model(path, fbs::MODEL_LOCATION_IN_SDCARD);
    if (!g_model) {
        ESP_LOGE(TAG, "Failed to create dl::Model");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = g_model->load(path, fbs::MODEL_LOCATION_IN_SDCARD, nullptr, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load model: %s", esp_err_to_name(ret));
        delete g_model;
        g_model = nullptr;
        return ret;
    }

    g_model->build(0, dl::MEMORY_MANAGER_GREEDY, true);
    if (!g_model->get_inputs().size() || !g_model->get_outputs().size()) {
        ESP_LOGE(TAG, "Model has no inputs or outputs");
        delete g_model;
        g_model = nullptr;
        return ESP_FAIL;
    }

    auto inputs = g_model->get_inputs();
    auto outputs = g_model->get_outputs();

    g_input_tensor = inputs.begin()->second;
    g_output_tensor = outputs.begin()->second;

    if (!g_input_tensor || !g_output_tensor) {
        ESP_LOGE(TAG, "Failed to get input/output tensors");
        delete g_model;
        g_model = nullptr;
        return ESP_FAIL;
    }

    g_model_built = true;

    ESP_LOGI(TAG, "Model loaded: %s (input: %d, output: %d)",
             path,
             g_input_tensor->get_size(),
             g_output_tensor->get_size());

    return ESP_OK;
}

esp_err_t inference_engine_run(const float *input, uint8_t input_size,
                               float *output, uint8_t output_size)
{
    if (!g_model || !g_model_built) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!g_input_tensor || !g_output_tensor) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_input_tensor->get_size() != input_size) {
        ESP_LOGE(TAG, "Input size mismatch: expected %d, got %d",
                 g_input_tensor->get_size(), input_size);
        return ESP_ERR_INVALID_ARG;
    }

    if (g_output_tensor->get_size() != output_size) {
        ESP_LOGE(TAG, "Output size mismatch: expected %d, got %d",
                 g_output_tensor->get_size(), output_size);
        return ESP_ERR_INVALID_ARG;
    }

    if (g_input_tensor->get_dtype() == dl::DATA_TYPE_INT8) {
        int8_t *in_data = (int8_t *)g_input_tensor->data;
        float scale = powf(2.0f, -(float)g_input_tensor->get_exponent());
        for (uint8_t i = 0; i < input_size; i++) {
            float val = input[i] * scale;
            if (val > 127.0f) val = 127.0f;
            if (val < -128.0f) val = -128.0f;
            in_data[i] = (int8_t)roundf(val);
        }
    } else {
        memcpy(g_input_tensor->data, input, input_size * sizeof(float));
    }

    g_model->run(g_input_tensor, dl::RUNTIME_MODE_SINGLE_CORE);

    if (g_output_tensor->get_dtype() == dl::DATA_TYPE_INT8) {
        int8_t *out_data = (int8_t *)g_output_tensor->data;
        float scale = powf(2.0f, (float)g_output_tensor->get_exponent());
        for (uint8_t i = 0; i < output_size; i++) {
            output[i] = (float)out_data[i] * scale;
        }
    } else {
        memcpy(output, g_output_tensor->data, output_size * sizeof(float));
    }

    return ESP_OK;
}

void inference_engine_unload(void)
{
    if (g_model) {
        delete g_model;
        g_model = nullptr;
    }
    g_input_tensor = nullptr;
    g_output_tensor = nullptr;
    g_model_built = false;
    ESP_LOGI(TAG, "Model unloaded");
}

bool inference_engine_is_loaded(void)
{
    return (g_model != nullptr && g_model_built);
}

} // extern "C"