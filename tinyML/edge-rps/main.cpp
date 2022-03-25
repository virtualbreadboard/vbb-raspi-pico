/**
 * Rock Paper Scissors Classifier
 * 
 * X,Y classification data captured using Analog MUX Decoder 
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
 
//OUTPUT STATUS 
const int LED_PAPER       = 2;
const int LED_ROCK        = 3;
const int LED_SCISSORS    = 4;
 
const int NO_TIMEOUT =  -1;

//ANALOG MUX 
const int CHANNEL_PIN_COUNT = 1;
const int CHANNEL_COUNT = 2;  
//ANALOG MUX PINS
const int MUX_CLK      = 0;
int ADC_CHANNEL_PINS[ CHANNEL_PIN_COUNT ] = { 1 } ; //MSB First Pin

//ANALOG DAC PIN
const int ADC0_PIN     = 26;

// Callback function declaration
static int get_signal_data(size_t offset, size_t length, float *out_ptr);

static float  input_buf[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] ;

float _prevDx;
float _prevDy;
int _samplesSinceChange;

/**
 * Decodes a binary channel from the pins
 */
static int currentChannel(){
    int channel = 0;
    for(int i = 0; i < CHANNEL_PIN_COUNT; i++ ){
        channel = channel << 1;
        channel+=  (gpio_get(ADC_CHANNEL_PINS[i]) ? 1 : 0 );
    }
    return channel;
}
  
  
/**
 * Average 8 from last 10 samples removing max/min (glitch).
 */
static int smoothedSample(){
    
    uint16_t max ;
    uint16_t min ;
    uint16_t sum;
    uint16_t sample;

    min = adc_read();
    max = adc_read();
     
    if( min > max){
        sample = min;
        min = max;
        max = sample;
    }

    for(int i = 0; i < 8; i++ ){
        sample = adc_read();
        if( sample > max){
            sum+=max;
            max = sample;
        }else if (sample < min ){
            sum+=min;
            min = sample;
        }else{
            sum+=sample;
        }
    }

    sum = sum >> 3; //Average dive by 8
    
    return sum;

}
  
static void initIO(){

    //3 Status and 4 DAC drivers.
    for(int i = 0; i < 3 ; i++ ){
        gpio_init(LED_ROCK + i);
        gpio_set_dir(LED_ROCK + i, GPIO_OUT);
        gpio_put(LED_ROCK + i,0);
    }

    //ADC Input
    for(int i = 0; i < 2 ; i++ ){
        gpio_init(MUX_CLK + i);
        gpio_set_dir(MUX_CLK + i, GPIO_IN);
    }
  
    //ADC Pins
    adc_init();
    adc_gpio_init(ADC0_PIN);
    adc_select_input(0);
 
}

/**
 *           _________
 *   _______/
 */
static void waitCLKRisingEdge(int periodMs){
    bool blocking = (periodMs == NO_TIMEOUT);
    periodMs = 15 * periodMs; //Wait 1.5 the expected period before exiting

    int timeout =0;

     //Wait until LOW
    while(  gpio_get(MUX_CLK) ){
        sleep_us(100);
        if(!blocking && ( timeout++ > periodMs )){
            printf("timeout\n");
            return;
        }
    }; 
    timeout = 0;
    while(  !gpio_get(MUX_CLK)){
        sleep_us(100);
        if( !blocking && ( timeout++ > periodMs) ){
            printf("timeout\n");
            return;
        }
    };  //Wait until HIGH
 
}

static float readADC(){

     const float conversion_factor = 1.0f / (1 << 12);
    uint16_t result = smoothedSample(); //adc_read();
    //printf(" Raw value: 0x%d, Normalised: %f V\n",  result, result * conversion_factor);

    return result * conversion_factor;

}

static bool IsIdle(int index){

 
    const float movement_detection_threshold = 0.01f;
    const int idle_samples_count = 5;
    int SAMPLE_WINDOW = idle_samples_count * CHANNEL_COUNT;

    if( index < SAMPLE_WINDOW){
        return false;
    }else{
 
        for(int i = 0, l = CHANNEL_COUNT * idle_samples_count; i < l; i+= CHANNEL_COUNT){
            for(int ch = 1; ch <= CHANNEL_COUNT; ch++ ){
                float dsignal =  abs( input_buf[index - i - ch] - input_buf[index - ch ]) ;
          
                if( dsignal > movement_detection_threshold){
                    return false;
                }
            }
           
        }

        return true;

    }
 
}

static void clearInference(){
    gpio_put(LED_ROCK       ,false);
    gpio_put(LED_PAPER      ,false);
    gpio_put(LED_SCISSORS   ,false);
}

/*
    Captures Frame 

    ____||||||_______||||||________

    In synchronous data mode the frame data comes in bursts. So to capture a frame need to to synch to the 
    start of the frame. Without a timeout the framing can get stuck so if the signal becomes idle it times outs
    and waits for the next frame. If enough samples are in the frame we complete the capture even if idle


*/
static void captureFrame(){

     const int canInferenceThreshold = ( 9 * EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE ) / 10;

      //Sync to the first channel by waiting for the last channel
    do
    {
        waitCLKRisingEdge(NO_TIMEOUT);
    } while ( currentChannel() != ( CHANNEL_COUNT - 1) );
     
    clearInference();
  
    int index = 0;
    while( index < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE){

        for(int ch = 0; ch < CHANNEL_COUNT; ch++ ){
             waitCLKRisingEdge(EI_CLASSIFIER_INTERVAL_MS);

             input_buf[index++] = readADC();
        }
 
        if(index < canInferenceThreshold){
            if( IsIdle(index)){
                index = 0;
                printf("idle\n");
            }
        }
        
    }


/*
    Display Recorded Data.
    for (size_t i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i+=CHANNEL_COUNT) {
 
        printf("%f %f\n",  input_buf[ i ], input_buf[ i + 1 ] );

    }
*/

}
 
int main(int argc, char **argv) {
    
    signal_t signal;            // Wrapper for raw input buffer
    ei_impulse_result_t result; // Used to store inference output
    EI_IMPULSE_ERROR res;       // Return code from inference
 
    stdio_init_all();

    initIO();
  
    printf("Hello, Edgey Rock Paper Scissors Start!\n");

    sleep_ms(100);
 
    // Assign callback function to fill buffer used for preprocessing/inference
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &get_signal_data;

    while(true){
 
        captureFrame();
        
        // Perform DSP pre-processing and inference
        res = run_classifier(&signal, &result, false);

        // Print return code and how long it took to perform inference
        printf("run_classifier returned: %d\r\n", res);
        printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n", 
                result.timing.dsp, 
                result.timing.classification, 
                result.timing.anomaly);
 

    // Print the prediction results (object detection)
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    printf("Object detection bounding boxes:\r\n");
    for (uint32_t i = 0; i < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n", 
                bb.label, 
                bb.value, 
                bb.x, 
                bb.y, 
                bb.width, 
                bb.height);
    }

    // Print the prediction results (classification)
#else
    printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        printf("  %s: ", ei_classifier_inferencing_categories[i]);
        printf("%.5f\r\n", result.classification[i].value);

        gpio_put(LED_ROCK + i,result.classification[i].value > 0.7f );
    
    }
    sleep_ms(100);
#endif
         
       
    }
    // Print anomaly result (if it exists)
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    printf("Anomaly prediction: %.3f\r\n", result.anomaly);
#endif
while(1){
             printf("Hello, Edgey End!\n");
            
            sleep_ms(1000);
         }
    return 0;
}


// Callback: fill a section of the out_ptr buffer when requested
static int get_signal_data(size_t offset, size_t length, float *out_ptr) {

    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (input_buf + offset)[i];
    }

    return EIDSP_OK;
   
}