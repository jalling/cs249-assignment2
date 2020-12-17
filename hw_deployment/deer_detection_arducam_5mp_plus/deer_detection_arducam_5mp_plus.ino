/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <TensorFlowLite.h>

#include "main_functions.h"

#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "deer_detect_day_model_data.h"
//#include "deer_detect_night_model_data.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

// Button setup
const int buttonPin = 10;
int buttonState;
int use_daytime_model = HIGH;

// Globals, used for compatibility with Arduino-style sketches.
namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model_day = nullptr;
tflite::MicroInterpreter* interpreter_day = nullptr;
//const tflite::Model* model_night = nullptr;
//tflite::MicroInterpreter* interpreter_night = nullptr;
TfLiteTensor* input = nullptr;

// In order to use optimized tensorflow lite kernels, a signed int8 quantized
// model is preferred over the legacy unsigned model format. This means that
// throughout this project, input images must be converted from unisgned to
// signed format. The easiest and quickest way to convert from unsigned to
// signed 8-bit integers is to subtract 128 from the unsigned value to get a
// signed value.

// An area of memory to use for input, output, and intermediate arrays.
constexpr int kTensorArenaSize = 115656;
//constexpr int kTensorArenaSize = 231312;
static uint8_t tensor_arena[kTensorArenaSize];
}  // namespace

// The name of this function is important for Arduino compatibility.
void setup() {
  //button setup
  pinMode(buttonPin, INPUT);

  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model_day = tflite::GetModel(g_deer_detect_day_model_data);
  //model_night = tflite::GetModel(g_deer_detect_night_model_data);
  if (model_day->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model_day->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  //
  // tflite::AllOpsResolver resolver;
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroMutableOpResolver<5> micro_op_resolver;
  micro_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
      tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                               tflite::ops::micro::Register_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_AVERAGE_POOL_2D,
                               tflite::ops::micro::Register_AVERAGE_POOL_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                               tflite::ops::micro::Register_RESHAPE());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                               tflite::ops::micro::Register_SOFTMAX());

  // Build an interpreter to run the model with.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroInterpreter static_interpreter_day(
      model_day, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter_day = &static_interpreter_day;
  //static tflite::MicroInterpreter static_interpreter_night(
  //    model_night, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  //interpreter_night = &static_interpreter_night;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status_day = interpreter_day->AllocateTensors();
  if (allocate_status_day != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() (day) failed");
    return;
  }
  //TfLiteStatus allocate_status_night = interpreter_night->AllocateTensors();
  //if (allocate_status_night != kTfLiteOk) {
  //  TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() (night) failed");
  //  return;
  //}

  // Get information about the memory area to use for the model's input.
  input = interpreter_day->input(0);
}

// The name of this function is important for Arduino compatibility.
void loop() {

  // Get the button state and determine which model to use
  int reading = digitalRead(buttonPin);
  if (reading == LOW) {
    TF_LITE_REPORT_ERROR(error_reporter, "Using daytime model");
    use_daytime_model = HIGH;
  }
  else  {
    TF_LITE_REPORT_ERROR(error_reporter, "Using nighttime model");
    use_daytime_model = LOW;
  }
  int start_image_time = millis();
  // Get image from provider.
  if (kTfLiteOk != GetImage(error_reporter, kNumCols, kNumRows, kNumChannels,
                            input->data.int8)) {
    TF_LITE_REPORT_ERROR(error_reporter, "Image capture failed.");
  }
  int end_image_time = millis();

  // Run the model on this input and make sure it succeeds.
  if (use_daytime_model == HIGH) {
    if (kTfLiteOk != interpreter_day->Invoke()) {
      TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed.");
    }
  }
  else
  {
    //if (kTfLiteOk != interpreter_night->Invoke()) {
      TF_LITE_REPORT_ERROR(error_reporter, "Model not Invoked");
    //}
  }

  TfLiteTensor* output = nullptr;
  if (use_daytime_model == HIGH)  {
    output = interpreter_day->output(0);
  }
  //else {
  //  output = interpreter_night->output(0);
  //}
  int inference_time = millis();

  TF_LITE_REPORT_ERROR(error_reporter, "Time for image capture (ms): %d",end_image_time-start_image_time);
  TF_LITE_REPORT_ERROR(error_reporter, "Time for inference (ms): %d",inference_time-end_image_time);

  // Process the inference results.
  int8_t deer_score = output->data.uint8[kPersonIndex];
  int8_t no_deer_score = output->data.uint8[kNotAPersonIndex];
  RespondToDetection(error_reporter, deer_score, no_deer_score);

}