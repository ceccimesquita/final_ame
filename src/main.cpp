#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "mpu6050.h"
#include "cnn_raw_har_model.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static constexpr int   WINDOW    = 128;
static constexpr int   CHANNELS  = 9;
static constexpr int   SAMPLE_MS = 20;

static constexpr float DEG2RAD = 0.01745329252f;

static const float MEAN[CHANNELS] = {
    -0.0006f,  0.0005f,  0.0003f,
    -0.0000f, -0.0006f,  0.0029f,
     0.7406f,  0.1230f,  0.1621f
};
static const float SCALE[CHANNELS] = {
    0.1121f, 0.0881f, 0.0747f,
    0.2530f, 0.2721f, 0.1727f,
    0.4275f, 0.4036f, 0.3659f
};

static const char* LABELS[4] = {"WALKING", "SITTING", "STANDING", "LAYING"};

struct Biquad {
    float b0, b1, b2, a1, a2;
    float w1 = 0.0f, w2 = 0.0f;
    float process(float x) {
        float w = x - a1 * w1 - a2 * w2;
        float y = b0 * w + b1 * w1 + b2 * w2;
        w2 = w1;
        w1 = w;
        return y;
    }
};

static Biquad sec1_x{6.45184913e-06f, 1.29036983e-05f, 6.45184913e-06f, -0.962994051f, 0.0f};
static Biquad sec1_y{6.45184913e-06f, 1.29036983e-05f, 6.45184913e-06f, -0.962994051f, 0.0f};
static Biquad sec1_z{6.45184913e-06f, 1.29036983e-05f, 6.45184913e-06f, -0.962994051f, 0.0f};
static Biquad sec2_x{1.0f, 1.0f, 0.0f, -1.96161218f, 0.963006955f};
static Biquad sec2_y{1.0f, 1.0f, 0.0f, -1.96161218f, 0.963006955f};
static Biquad sec2_z{1.0f, 1.0f, 0.0f, -1.96161218f, 0.963006955f};

static constexpr int TENSOR_ARENA_SIZE = 60 * 1024;
alignas(16) static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

static float window_buf[WINDOW][CHANNELS];

static constexpr uint LED_PINS[] = {17, 19, 21, 26};

int main() {
    stdio_init_all();

    for (uint pin : LED_PINS) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 1);
    }

    sleep_ms(2000);

    printf("HAR TinyML — Raspberry Pi Pico\n");
    stdio_flush();

    MPU6050_light mpu(i2c1, 2, 3, 100000);

    if (mpu.begin() != 0) {
        printf("ERRO: MPU6050 nao encontrado!\n");
        stdio_flush();
        while (true) tight_loop_contents();
    }
    printf("Calibrando sensor... mantenha parado.\n");
    stdio_flush();
    mpu.calcOffsets(true, true);
    printf("Sensor pronto!\n\n");
    stdio_flush();

    const tflite::Model* tfl_model = tflite::GetModel(cnn_raw_har_model);

    tflite::MicroMutableOpResolver<10> resolver;
    resolver.AddExpandDims();
    resolver.AddSqueeze();
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddMean();
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddQuantize();
    resolver.AddDequantize();

    tflite::MicroInterpreter interpreter(
        tfl_model, resolver, tensor_arena, TENSOR_ARENA_SIZE);

    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printf("ERRO: AllocateTensors falhou! Aumente TENSOR_ARENA_SIZE.\n");
        stdio_flush();
        while (true) tight_loop_contents();
    }

    printf("Modelo carregado! Arena usada: %u / %u bytes\n",
           (unsigned)interpreter.arena_used_bytes(), TENSOR_ARENA_SIZE);
    stdio_flush();

    TfLiteTensor* input  = interpreter.input(0);
    TfLiteTensor* output = interpreter.output(0);

    const float in_scale = input->params.scale;
    const int   in_zero  = input->params.zero_point;

    printf("Input: escala=%.6f  zero=%d\n", in_scale, in_zero);
    printf("Coletando janelas de %.1f s — classifica a cada janela.\n\n",
           WINDOW * SAMPLE_MS / 1000.0f);
    stdio_flush();

    float grav_x = 0.0f, grav_y = 0.0f, grav_z = 0.0f;
    bool  grav_init = false;

    int      sample_idx  = 0;
    int      print_count = 0;
    uint32_t next_ms     = to_ms_since_boot(get_absolute_time());

    while (true) {
        while (to_ms_since_boot(get_absolute_time()) < next_ms) {}
        next_ms += SAMPLE_MS;

        mpu.update();

        float ax = mpu.getAccX();
        float ay = mpu.getAccY();
        float az = mpu.getAccZ();

        if (++print_count >= 25) {
            print_count = 0;
            printf("ACC  x=%6.3f  y=%6.3f  z=%6.3f g  |  GYRO  x=%6.2f  y=%6.2f  z=%6.2f deg/s\n",
                   ax, ay, az,
                   mpu.getGyroX(), mpu.getGyroY(), mpu.getGyroZ());
            stdio_flush();
        }

        grav_x = sec2_x.process(sec1_x.process(ax));
        grav_y = sec2_y.process(sec1_y.process(ay));
        grav_z = sec2_z.process(sec1_z.process(az));

        window_buf[sample_idx][0] = ax - grav_x;
        window_buf[sample_idx][1] = ay - grav_y;
        window_buf[sample_idx][2] = az - grav_z;
        window_buf[sample_idx][3] = mpu.getGyroX() * DEG2RAD;
        window_buf[sample_idx][4] = mpu.getGyroY() * DEG2RAD;
        window_buf[sample_idx][5] = mpu.getGyroZ() * DEG2RAD;
        window_buf[sample_idx][6] = ax;
        window_buf[sample_idx][7] = ay;
        window_buf[sample_idx][8] = az;

        sample_idx++;
        if (sample_idx < WINDOW) continue;
        sample_idx = 0;

        for (int t = 0; t < WINDOW; t++) {
            for (int c = 0; c < CHANNELS; c++) {
                float norm = (window_buf[t][c] - MEAN[c]) / SCALE[c];
                int   q    = (int)(norm / in_scale + (float)in_zero + 0.5f);
                if (q >  127) q =  127;
                if (q < -128) q = -128;
                input->data.int8[t * CHANNELS + c] = (int8_t)q;
            }
        }

        if (interpreter.Invoke() != kTfLiteOk) {
            printf("ERRO: Invoke falhou!\n");
            stdio_flush();
            continue;
        }

        int    best_class = 0;
        int8_t best_val   = output->data.int8[0];
        for (int i = 1; i < 4; i++) {
            if (output->data.int8[i] > best_val) {
                best_val   = output->data.int8[i];
                best_class = i;
            }
        }

        for (uint pin : LED_PINS) gpio_put(pin, 0);
        gpio_put(LED_PINS[best_class], 1);

        printf("Atividade: %-10s  (scores: %4d %4d %4d %4d)\n",
               LABELS[best_class],
               (int)output->data.int8[0],
               (int)output->data.int8[1],
               (int)output->data.int8[2],
               (int)output->data.int8[3]);
        stdio_flush();
    }

    return 0;
}
