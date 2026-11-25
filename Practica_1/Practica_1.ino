/* Edge Impulse ingestion SDK - Ventana deslizante optimizada para Arduino Nano 33 BLE */

// Optimizaciones de memoria y velocidad
#define EIDSP_QUANTIZE_FILTERBANK       1
#define EI_CLASSIFIER_ALLOCATION_STATIC 1

/* Includes ---------------------------------------------------------------- */
#include <PDM.h>
#include <mloren-project-1_inferencing.h>

// Evaluar cada 250 ms (4 veces por segundo) sobre la ventana de 1 segundo
#define SLICE_INTERVAL_MS   250
#define SLICE_SAMPLES       ((EI_CLASSIFIER_FREQUENCY * SLICE_INTERVAL_MS) / 1000)

typedef struct {
    int16_t *buffer;
    volatile uint8_t slice_ready;
    volatile uint32_t slice_count;
} inference_t;

static inference_t inference;
static signed short sampleBuffer[2048];
static bool debug_nn = false;

// Control no bloqueante del LED integrado
static bool led_active = false;
static unsigned long led_turn_on_time = 0;

/* Declaraciones de funciones */
static bool microphone_inference_start(void);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static void microphone_inference_end(void);

void setup()
{
    Serial.begin(115200);
    // Espera máximo 2 segundos al Monitor Serie para no bloquear si va con batería
    unsigned long start = millis();
    while (!Serial && (millis() - start < 2000));

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    if (!microphone_inference_start()) {
        Serial.println("ERR: No se pudo iniciar el microfono o asignar memoria");
        return;
    }

    Serial.println("Listo. Escuchando de forma continua...");
}

void loop()
{
    // 1. Apagar el LED tras 2 segundos encendido
    if (led_active && (millis() - led_turn_on_time >= 2000)) {
        digitalWrite(LED_BUILTIN, LOW);
        led_active = false;
    }

    // 2. Esperar a que entren 250 ms nuevos de audio
    if (inference.slice_ready == 0) {
        delay(1);
        return;
    }
    inference.slice_ready = 0;

    // 3. Ejecutar inferencia sobre la ventana completa de 1 segundo
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        Serial.print("ERR: Error en run_classifier: ");
        Serial.println(r);
        return;
    }

    // 4. Imprimir todas las clases y activar el LED si se detecta "ACTIVA"
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Serial.print(result.classification[ix].label);
        Serial.print(": ");
        Serial.print(result.classification[ix].value, 3);
        Serial.print("  ");

        if (strcmp(result.classification[ix].label, "ACTIVA") == 0 ||
            strcmp(result.classification[ix].label, "activa") == 0) {
            
            // Umbral de activación (ajústalo entre 0.60 y 0.80 según lo que veas en Serial)
            if (result.classification[ix].value > 0.90f) {
                digitalWrite(LED_BUILTIN, HIGH);
                led_turn_on_time = millis();
                led_active = true;
            }
        }
    }
    Serial.println();
}

static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);
    int samplesRead = bytesRead >> 1;

    // Desplazar el buffer hacia la izquierda y añadir las muestras nuevas al final
    if (samplesRead > 0 && samplesRead < EI_CLASSIFIER_RAW_SAMPLE_COUNT) {
        memmove(inference.buffer,
                &inference.buffer[samplesRead],
                (EI_CLASSIFIER_RAW_SAMPLE_COUNT - samplesRead) * sizeof(int16_t));

        memcpy(&inference.buffer[EI_CLASSIFIER_RAW_SAMPLE_COUNT - samplesRead],
               sampleBuffer,
               samplesRead * sizeof(int16_t));

        inference.slice_count += samplesRead;
        if (inference.slice_count >= SLICE_SAMPLES) {
            inference.slice_count = 0;
            inference.slice_ready = 1;
        }
    }
}

static bool microphone_inference_start(void)
{
    inference.buffer = (int16_t *)calloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT, sizeof(int16_t));
    if (inference.buffer == NULL) {
        return false;
    }

    inference.slice_count = 0;
    inference.slice_ready = 0;

    PDM.onReceive(&pdm_data_ready_inference_callback);
    PDM.setBufferSize(2048);

    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        microphone_inference_end();
        return false;
    }

    // Ganancia del micrófono (prueba entre 30 y 70 según la distancia a la que hables)
    PDM.setGain(50);

    return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
    return 0;
}

static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif