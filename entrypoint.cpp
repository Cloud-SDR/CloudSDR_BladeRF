/*
 * Adds RTLSDR Dongles capability to SDRNode
 * Copyright (C) 2016 Sylvain AZARIAN <sylvain.azarian@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "BladeRF/nuand/libbladeRF.h"
#include "jansson/jansson.h"

#include "entrypoint.h"
#define DEBUG_DRIVER (0)

char *driver_name ;
void* acquisition_thread( void *params ) ;

typedef struct __attribute__ ((__packed__)) _sCplx
{
    float re;
    float im;
} TYPECPX;


unsigned int lms_filters[] = { 1500000u, 1750000u, 2500000u, 2750000u, 3000000u,
                           3840000u, 5000000u, 5500000u, 6000000u, 7000000u,
                           8750000u, 10000000u, 12000000u, 14000000u, 18000000u,
                           20000000u };
#define FILTER_TAB_LENGTH (16)


struct t_sample_rates {
    unsigned int *sample_rates ;
    unsigned int *rf_filter_bw ;
    int enum_length ;
    int preffered_sr_index ;
};

#define STAGES_COUNT (3)

// this structure stores the device state
struct t_rx_device {
    bladerf *bladerf_device ;
    char *device_name ;
    char *device_serial_number ;

    struct t_sample_rates* rates;
    unsigned int current_sample_rate ;

    int64_t min_frq_hz ; // minimal frequency for this device
    int64_t max_frq_hz ; // maximal frequency for this device
    int64_t center_frq_hz ; // currently set frequency


    float gain[STAGES_COUNT] ;

    char *uuid ;
    bool running ;
    bool acq_stop ;
    sem_t mutex;

    pthread_t receive_thread ;
    struct ext_Context ext_context ;

    // for DC removal
    TYPECPX xn_1 ;
    TYPECPX yn_1 ;


};


int device_count ;
char *stage_name[STAGES_COUNT] ;
char *stage_unit ;

struct t_rx_device *rx;
json_t *root_json ;
_tlogFun* sdrNode_LogFunction ;
_pushSamplesFun *acqCbFunction ;


int setBladeRxGain( struct t_rx_device *dev, float value, int stage);

#ifdef _WIN64
#include <windows.h>
// Win  DLL Main entry
BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID *lpvReserved ) {
    return( TRUE ) ;
}
#endif

void log( int device_id, int level, char *msg ) {
    if( sdrNode_LogFunction != NULL ) {
        (*sdrNode_LogFunction)(rx[device_id].uuid,level,msg);
        return ;
    }
    printf("Trace:%s\n", msg );
}



/*
 * First function called by SDRNode - must return 0 if hardware is not present or problem
 */
/**
 * @brief initLibrary is called when the DLL is loaded, only for the first instance of the devices (when the getBoardCount() function returns
 *        more than 1)
 * @param json_init_params a JSOn structure to pass parameters from scripting to drivers
 * @param ptr pointer to function for logging
 * @param acqCb pointer to RF IQ processing function
 * @return
 */
LIBRARY_API int initLibrary(char *json_init_params,
                            _tlogFun* ptr,
                            _pushSamplesFun *acqCb ) {
    json_error_t error;
    root_json = NULL ;
    struct t_rx_device *tmp ;
    int rc ;

    sdrNode_LogFunction = ptr ;
    acqCbFunction = acqCb ;

    if( json_init_params != NULL ) {
        root_json = json_loads(json_init_params, 0, &error);

    }
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);

    driver_name = (char *)malloc( 100*sizeof(char));
    snprintf(driver_name,100,"BladeRF");

    // Step 1 : count how many devices we have
    bladerf_devinfo* g_devinfo ;
    device_count = bladerf_get_device_list(&g_devinfo);
    if( device_count == 0 ) {
        return(0); // no hardware
    }

    rx = (struct t_rx_device *)malloc( device_count * sizeof(struct t_rx_device));
    if( rx == NULL ) {
        return(0);
    }
    tmp = rx ;
    // iterate through devices to populate structure
    for( int k=0 ; k < device_count; k++ ) {
        tmp->device_name = (char *)malloc( 64 *sizeof(char));
        sprintf( tmp->device_name, "BladeRF");
        rc = bladerf_open_with_devinfo( &tmp->bladerf_device, &g_devinfo[k] );
        if( !bladerf_is_fpga_configured( tmp->bladerf_device )) {
            bladerf_fpga_size size = BLADERF_FPGA_UNKNOWN;
            rc = bladerf_get_fpga_size( tmp->bladerf_device, &size);
            if (!rc && (size == BLADERF_FPGA_UNKNOWN)) {
                size = BLADERF_FPGA_40KLE;
            }
            if( !rc ) {
                char fpgaFile[255];
                switch (size)
                {
                case BLADERF_FPGA_40KLE:
                    sprintf( fpgaFile , "./hostedx40.rbf" ) ;
                    break;
                case BLADERF_FPGA_115KLE:
                    sprintf(fpgaFile , "./hostedx115.rbf" );
                    break;
                default:
                case BLADERF_FPGA_UNKNOWN: continue;
                }
                rc = bladerf_load_fpga( tmp->bladerf_device, fpgaFile  );
                if( rc == 0 ) {
                    continue ;
                }
            }
        }
        sem_init(&tmp->mutex, 0, 0);
        tmp->uuid = NULL ;
        tmp->running = false ;
        tmp->acq_stop = false ;
        tmp->device_serial_number = (char *)malloc( 255 *sizeof(char));
        bladerf_get_serial( tmp->bladerf_device, tmp->device_serial_number );


        tmp->min_frq_hz = BLADERF_FREQUENCY_MIN ;
        tmp->max_frq_hz = BLADERF_FREQUENCY_MAX ;
        tmp->center_frq_hz = tmp->min_frq_hz + 1e6 ; // arbitrary startup freq

        // allocate rates
        tmp->rates = (struct t_sample_rates*)malloc( sizeof(struct t_sample_rates));

        tmp->rates->enum_length = 7 ; // we manage 5 different sampling rates
        tmp->rates->sample_rates = (unsigned int *)malloc( tmp->rates->enum_length * sizeof( unsigned int )) ;
        tmp->rates->rf_filter_bw = (unsigned int *)malloc( tmp->rates->enum_length * sizeof( unsigned int )) ;

        tmp->rates->sample_rates[0] = 2*1024*1000u ;
        tmp->rates->sample_rates[1] = 4*1024*1000u ;
        tmp->rates->sample_rates[2] = 6*1024*1000u ;
        tmp->rates->sample_rates[3] = 8*1024*1000u ;
        tmp->rates->sample_rates[4] = 10*1024*1000u ;
        tmp->rates->sample_rates[5] = 12*1024*1000u ;
        tmp->rates->sample_rates[6] = 14*1024*1000u ;

        tmp->rates->preffered_sr_index = 0 ; // our default sampling rate will be 2048 KHz
        // set startup freq
        rc = bladerf_set_frequency( tmp->bladerf_device, BLADERF_MODULE_RX, tmp->center_frq_hz );

        // check filters
        for( int f=0 ; f < tmp->rates->enum_length ; f++ ) {
            unsigned int rate = tmp->rates->sample_rates[f] ;
            unsigned int filter = rate*2 ;

            if( DEBUG_DRIVER ) fprintf(stderr,"\nSearching filter rate[%d]=%d \n", f, (int)rate  );
            for( int x=FILTER_TAB_LENGTH-1 ; x>=0; x--) {
                bladerf_set_bandwidth( tmp->bladerf_device, BLADERF_MODULE_RX, lms_filters[x], &filter );
                tmp->rates->rf_filter_bw[f] = filter ;
                if( filter*2 < rate ) {
                    break ;
                }
            }

            if( DEBUG_DRIVER ) fprintf(stderr,"For rate %3.1f, selected filter is %3.1f\n", rate/1000.0, 2*tmp->rates->rf_filter_bw[f]/1000.0 );
        }

        // set  SR
        rc = tmp->rates->preffered_sr_index ;
        tmp->current_sample_rate = tmp->rates->sample_rates[rc] ;
        bladerf_set_sample_rate( tmp->bladerf_device, BLADERF_MODULE_RX,
                                 tmp->current_sample_rate,
                                 &tmp->current_sample_rate);
        unsigned int actual_rx_hwfilter ;
        bladerf_set_bandwidth( tmp->bladerf_device, BLADERF_MODULE_RX, tmp->rates->rf_filter_bw[rc],&actual_rx_hwfilter);
        setBladeRxGain( tmp, BLADERF_LNA_GAIN_MID_DB, 0 ) ;
        setBladeRxGain( tmp, (float)(BLADERF_RXVGA1_GAIN_MIN+(BLADERF_RXVGA1_GAIN_MAX-BLADERF_RXVGA1_GAIN_MIN)/2) , 1 ) ;
        setBladeRxGain( tmp, (float)(BLADERF_RXVGA2_GAIN_MIN+(BLADERF_RXVGA2_GAIN_MAX-BLADERF_RXVGA2_GAIN_MIN)/2) , 2 ) ;

        tmp->gain[0] = (float)BLADERF_LNA_GAIN_MID_DB ;
        tmp->gain[1] = (float)(BLADERF_RXVGA1_GAIN_MIN+(BLADERF_RXVGA1_GAIN_MAX-BLADERF_RXVGA1_GAIN_MIN)/2)  ;
        tmp->gain[2] = (float)(BLADERF_RXVGA2_GAIN_MIN+(BLADERF_RXVGA2_GAIN_MAX-BLADERF_RXVGA2_GAIN_MIN)/2) ;

        tmp->ext_context.ctx_version = 0 ;
        tmp->ext_context.center_freq = tmp->center_frq_hz ;
        tmp->ext_context.sample_rate = tmp->current_sample_rate ;

        // create acquisition threads
        pthread_create(&tmp->receive_thread, NULL, acquisition_thread, tmp );

    }

    // set names for stages
    stage_name[0] = (char *)malloc( 10*sizeof(char));
    snprintf( stage_name[0],10,"LNA");
    stage_name[1] = (char *)malloc( 10*sizeof(char));
    snprintf( stage_name[1],10,"VGA1");
    stage_name[2] = (char *)malloc( 10*sizeof(char));
    snprintf( stage_name[2],10,"VGA2");


    stage_unit = (char *)malloc( 10*sizeof(char));
    snprintf( stage_unit,10,"dB");

    fflush(stderr);
    return(RC_OK);
}

int setBladeRxGain( struct t_rx_device *dev, float value, int stage) {
    int v = (int)value ;
    bladerf *bladerf_device = dev->bladerf_device ;
    switch( stage ) {
    // LNA
    case 0 :
        dev->gain[0] = value ;
        switch( v ) {
        case 0:
            bladerf_set_lna_gain( bladerf_device, BLADERF_LNA_GAIN_BYPASS);
            break ;
        case 1:
        case 2:
        case 3:
            bladerf_set_lna_gain( bladerf_device, BLADERF_LNA_GAIN_MID);
            break ;
        default:
            bladerf_set_lna_gain( bladerf_device, BLADERF_LNA_GAIN_MAX);
            break ;
        }

        break ;
        // RXVGA1
    case 1:
        if( v > 30 ) v = 30 ;
        dev->gain[1] = v ;
        bladerf_set_rxvga1( bladerf_device, v );
        break ;
    case 2:
        dev->gain[2] = v ;
        bladerf_set_rxvga2( bladerf_device, v);
        break ;
    }
    //semHW->release(1);
    return(1);
}

/**
 * @brief setBoardUUID this function is called by SDRNode to assign a unique ID to each device managed by the driver
 * @param device_id [0..getBoardCount()[
 * @param uuid the unique ID
 * @return
 */
LIBRARY_API int setBoardUUID( int device_id, char *uuid ) {
    int len = 0 ;

    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%s)\n", __func__, device_id, uuid );

    if( uuid == NULL ) {
        return(RC_NOK);
    }
    if( device_id >= device_count )
        return(RC_NOK);

    len = strlen(uuid);
    if( rx[device_id].uuid != NULL ) {
        free( rx[device_id].uuid );
    }
    rx[device_id].uuid = (char *)malloc( len * sizeof(char));
    strcpy( rx[device_id].uuid, uuid);
    return(RC_OK);
}

/**
 * @brief getHardwareName called by SDRNode to retrieve the name for the nth device
 * @param device_id [0..getBoardCount()[
 * @return a string with the hardware name, this name is listed in the 'devices' admin page and appears 'as is' in the scripts
 */
LIBRARY_API char *getHardwareName(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(NULL);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->device_name );
}

/**
 * @brief getBoardCount called by SDRNode to retrieve the number of different boards managed by the driver
 * @return the number of devices managed by the driver
 */
LIBRARY_API int getBoardCount() {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    return(device_count);
}

/**
 * @brief getPossibleSampleRateCount called to know how many sample rates are available. Used to fill the select zone in admin
 * @param device_id
 * @return sample rate in Hz
 */
LIBRARY_API int getPossibleSampleRateCount(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->rates->enum_length );
}

/**
 * @brief getPossibleSampleRateValue
 * @param device_id
 * @param index
 * @return
 */
LIBRARY_API unsigned int getPossibleSampleRateValue(int device_id, int index) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, index );
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;

    struct t_sample_rates* rates = dev->rates ;
    if( index > rates->enum_length )
        return(0);

    return( rates->sample_rates[index] );
}

LIBRARY_API unsigned int getPrefferedSampleRateValue(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    struct t_sample_rates* rates = dev->rates ;
    int index = rates->preffered_sr_index ;
    return( rates->sample_rates[index] );
}
//-------------------------------------------------------------------
LIBRARY_API int64_t getMin_HWRx_CenterFreq(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->min_frq_hz ) ;
}

LIBRARY_API int64_t getMax_HWRx_CenterFreq(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->max_frq_hz ) ;
}

//-------------------------------------------------------------------
// Gain management
// devices have stages (LNA, VGA, IF...) . Each stage has its own gain
// range, its own name and its own unit.
// each stage can be 'continuous gain' or 'discrete' (on/off for example)
//-------------------------------------------------------------------
LIBRARY_API int getRxGainStageCount(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    return(3);
}

LIBRARY_API char* getRxGainStageName( int device_id, int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    switch( stage ) {
    case 0 : return((char*)"LNA");
    case 1 : return((char*)"VGA1");
    case 2 : return((char*)"VGA2");
    }
    return((char*)"LNA");
}

LIBRARY_API char* getRxGainStageUnitName( int device_id, int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    // RTLSDR have only one stage so the unit is same for all
    return((char*)"dB");
}

LIBRARY_API int getRxGainStageType( int device_id, int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    // continuous value
    return(0);
}

LIBRARY_API float getMinGainValue(int device_id,int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    if( device_id >= device_count )
        return(0);
    switch( stage ) {
    case 0 : return(0);
    case 1 : return((float)BLADERF_RXVGA1_GAIN_MIN);
    case 2 : return((float)BLADERF_RXVGA2_GAIN_MIN);
    }
    return(0);
}

LIBRARY_API float getMaxGainValue(int device_id,int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    if( device_id >= device_count )
        return(0);
    switch( stage ) {
    case 0 : return((float)BLADERF_LNA_GAIN_MAX_DB);
    case 1 : return((float)BLADERF_RXVGA1_GAIN_MAX);
    case 2 : return((float)BLADERF_RXVGA2_GAIN_MAX);
    }
    return(0);
}

LIBRARY_API int getGainDiscreteValuesCount( int device_id, int stage ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage);
    return(0);
}

LIBRARY_API float getGainDiscreteValue( int device_id, int stage, int index ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d, %d,%d)\n", __func__, device_id, stage, index);
    return(0);
}

/**
 * @brief getSerialNumber returns the (unique for this hardware name) serial number. Serial numbers are useful to manage more than one unit
 * @param device_id
 * @return
 */
LIBRARY_API char* getSerialNumber( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->device_serial_number );
}

//----------------------------------------------------------------------------------
// Manage acquisition
// SDRNode calls 'prepareRxEngine(device)' to ask for the start of acquisition
// Then, the driver shall call the '_pushSamplesFun' function passed at initLibrary( ., ., _pushSamplesFun* fun , ...)
// when the driver shall stop, SDRNode calls finalizeRXEngine()

/**
 * @brief prepareRXEngine trig on the acquisition process for the device
 * @param device_id
 * @return RC_OK if streaming has started, RC_NOK otherwise
 */
LIBRARY_API int prepareRXEngine( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);

    // here we keep it simple, just fire the relevant mutex
    struct t_rx_device *dev = &rx[device_id] ;
    dev->acq_stop = false ;
    sem_post(&dev->mutex);

    return(RC_OK);
}

/**
 * @brief finalizeRXEngine stops the acquisition process
 * @param device_id
 * @return
 */
LIBRARY_API int finalizeRXEngine( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    dev->acq_stop = true ;
    return(RC_OK);
}

/**
 * @brief setRxSampleRate configures the sample rate for the device (in Hz). Can be different from the enum given by getXXXSampleRate
 * @param device_id
 * @param sample_rate
 * @return
 */
LIBRARY_API int setRxSampleRate( int device_id , int sample_rate) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id,sample_rate);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    if( (unsigned int)sample_rate == dev->current_sample_rate ) {
        return(RC_OK);
    }

    bladerf_set_sample_rate( dev->bladerf_device, BLADERF_MODULE_RX,
                             sample_rate,
                             &dev->current_sample_rate);

    unsigned int filter ;
    for( int f=0 ; f < dev->rates->enum_length ; f++ ) {
        unsigned int rate = dev->rates->sample_rates[f] ;
        if( rate == sample_rate ) {
            bladerf_set_bandwidth( dev->bladerf_device, BLADERF_MODULE_RX,
                                     dev->rates->rf_filter_bw[f],
                                     &filter );
            if( DEBUG_DRIVER ) fprintf(stderr,"setRxSampleRate rate %3.1f, filter set %3.1f\n", rate/1000.0, 2*dev->rates->rf_filter_bw[f]/1000.0 );
        }

    }
    fflush(stderr);
    return(RC_OK);
}

/**
 * @brief getActualRxSampleRate called to know what is the actual sampling rate (hz) for the given device
 * @param device_id
 * @return
 */
LIBRARY_API int getActualRxSampleRate( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);
    struct t_rx_device *dev = &rx[device_id] ;
    return(dev->current_sample_rate);
}

/**
 * @brief setRxCenterFreq tunes device to frq_hz (center frequency)
 * @param device_id
 * @param frq_hz
 * @return
 */
LIBRARY_API int setRxCenterFreq( int device_id, int64_t frq_hz ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%ld)\n", __func__, device_id, (long)frq_hz);
    if( DEBUG_DRIVER ) fflush(stderr);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    int rc = bladerf_set_frequency( dev->bladerf_device, BLADERF_MODULE_RX, frq_hz );
    if( rc == 0 ) {
        dev->center_frq_hz = frq_hz ;
        return(RC_OK);
    }
    if( DEBUG_DRIVER ) fprintf(stderr,"ERROR : %s(%d,%ld)\n", __func__, device_id, (long)frq_hz);
    if( DEBUG_DRIVER ) fflush(stderr);
    return(RC_NOK);
}

/**
 * @brief getRxCenterFreq retrieve the current center frequency for the device
 * @param device_id
 * @return
 */
LIBRARY_API int64_t getRxCenterFreq( int device_id ) {
    unsigned int frequency ;
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    bladerf_get_frequency( dev->bladerf_device, BLADERF_MODULE_RX, &frequency );
    if( frequency > 0 ) {
        dev->center_frq_hz = (int64_t)frequency ;
    }
    return( dev->center_frq_hz ) ;
}

/**
 * @brief setRxGain sets the current gain
 * @param device_id
 * @param stage_id
 * @param gain_value
 * @return
 */
LIBRARY_API int setRxGain( int device_id, int stage_id, float gain_value ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d,%f)\n", __func__, device_id,stage_id,gain_value);
    if( device_id >= device_count )
        return(RC_NOK);
    if( stage_id >= 1 )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    return( setBladeRxGain( dev, gain_value, stage_id ));
}

/**
 * @brief getRxGainValue reads the current gain value
 * @param device_id
 * @param stage_id
 * @return
 */
LIBRARY_API float getRxGainValue( int device_id , int stage_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id,stage_id);

    if( device_id >= device_count )
        return(RC_NOK);
    if( stage_id >= STAGES_COUNT )
        return(RC_NOK);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->gain[stage_id]) ;
}

LIBRARY_API bool setAutoGainMode( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(false);
    return(false);
}

//-----------------------------------------------------------------------------------------
// functions below are RTLSDR specific
// One thread is started by device, and each sample frame calls rtlsdr_callback() with a block
// of IQ samples as bytes.
// Samples are converted to float, DC is removed and finally samples are passed to SDRNode
//


#define ALPHA_DC (0.9996)


/**
 * @brief acquisition_thread This function is locked by the mutex and waits before starting the acquisition in asynch mode
 * @param params
 * @return
 */
#define DEFAULT_STREAM_XFERS 64
#define DEFAULT_STREAM_BUFFERS 32
#define DEFAULT_STREAM_SAMPLES 8192
#define DEFAULT_STREAM_TIMEOUT 5000

#define DEFAULT_STREAM_BUFFERSIZE 8192*4
#define DEFAULT_STREAM_NUMTRANSFERS 16
void* acquisition_thread( void *params ) {
    int rc ;
    int16_t *ptr;
    struct bladerf_metadata meta;
    struct t_rx_device* dev = (struct t_rx_device*)params ;
    bladerf *bladerf_device = dev->bladerf_device ;
    float I,Q ;
    TYPECPX tmp;

    // calibration procedure
    rc = bladerf_enable_module(  bladerf_device, BLADERF_MODULE_TX, true);
    if( rc != 0 ) {
        goto cmd_calibrate_err;
    }
    rc = bladerf_enable_module( bladerf_device, BLADERF_MODULE_RX, true);
    if (rc != 0) {
        fprintf( stderr,"doCalibrate failed bladerf_enable_module BLADERF_MODULE_RX: %s\n",  bladerf_strerror(rc));
        goto cmd_calibrate_err;
    }

    /* Calibrate LPF Tuning Module */
    rc = bladerf_calibrate_dc( bladerf_device, BLADERF_DC_CAL_LPF_TUNING);
    if (rc != 0) {
        fprintf( stderr,"doCalibrate failed bladerf_calibrate_dc: %s\n",  bladerf_strerror(rc));
        goto cmd_calibrate_err;
    }

    /* Calibrate TX LPF Filter */
    rc = bladerf_calibrate_dc( bladerf_device, BLADERF_DC_CAL_TX_LPF);
    if (rc != 0) {
        fprintf( stderr,"doCalibrate failed bladerf_calibrate_dc: %s\n",  bladerf_strerror(rc));
        goto cmd_calibrate_err;
    }

    /* Calibrate RX LPF Filter */
    rc = bladerf_calibrate_dc( bladerf_device, BLADERF_DC_CAL_RX_LPF);
    if (rc != 0) {
        fprintf( stderr,"doCalibrate failed bladerf_calibrate_dc: %s\n",  bladerf_strerror(rc));
        goto cmd_calibrate_err;
    }

    /* Calibrate RX VGA2 */
    rc = bladerf_calibrate_dc( bladerf_device, BLADERF_DC_CAL_RXVGA2);
    if (rc != 0) {
        fprintf( stderr, "doCalibrate failed bladerf_calibrate_dc: %s\n",  bladerf_strerror(rc));
        goto cmd_calibrate_err;
    }
    //-------------------------

    bladerf_set_lpf_mode( bladerf_device, BLADERF_MODULE_RX, BLADERF_LPF_NORMAL);
    /* Configure the device's RX module for use with the sync interface.
     * SC16 Q11 samples *with* metadata are used. */
    rc = bladerf_sync_config( bladerf_device,
                              BLADERF_MODULE_RX,
                              BLADERF_FORMAT_SC16_Q11_META,
                              DEFAULT_STREAM_BUFFERS,
                              DEFAULT_STREAM_BUFFERSIZE,
                              DEFAULT_STREAM_NUMTRANSFERS,
                              DEFAULT_STREAM_TIMEOUT);
    if (rc != 0) {
        if( DEBUG_DRIVER ) {
            fprintf( stderr, "Error failed for configure %s\n", __func__);
            return(NULL);
        }
    }

    ptr = (int16_t *)malloc(DEFAULT_STREAM_SAMPLES * 2 * sizeof(int16_t));
    dev->running = false ;
    for( ; ; ) {

        if( DEBUG_DRIVER ) fprintf(stderr,"- %s() thread waiting\n", __func__ );
        if( DEBUG_DRIVER ) fflush(stderr);

        sem_wait( &dev->mutex );
        if( DEBUG_DRIVER ) fprintf(stderr,"+ %s() thread starting\n", __func__ );

        dev->running = true ;
        // We must always enable the RX module before attempting to RX samples
        rc = bladerf_enable_module( bladerf_device, BLADERF_MODULE_RX, true);
        if (rc != 0) {
            if( DEBUG_DRIVER ) {
                fprintf( stderr, "Error failed for bladerf_enable_module %s\n", __func__);
                return(NULL);
            }
        }
        while( !dev->acq_stop ) {
            /* Perform a read immediately, and have the bladerf_sync_rx function
             * provide the timestamp of the read samples */
            memset(&meta, 0, sizeof(meta));
            meta.flags = BLADERF_META_FLAG_RX_NOW;
            rc = bladerf_sync_rx( bladerf_device, ptr, DEFAULT_STREAM_SAMPLES, &meta, 5000);
            if( rc == 0 ) {
                // we have samples
                TYPECPX *samples = (TYPECPX*)malloc( meta.actual_count * sizeof(TYPECPX));
                for( int i=0 ; i < meta.actual_count ; i++ ) {
                    int j=2*i;
    #ifndef _WINDOWS
                    I = (float)( le16toh( ptr[ j   ] ))* 1.0/2048.0 ;
                    Q = (float)( le16toh( ptr[ j+1 ] ))* 1.0/2048.0 ;
    #else
                    I = (float)( ptr[ j   ]) * 1.0/2048.0 ;
                    Q = (float)( ptr[ j+1 ]) * 1.0/2048.0 ;
    #endif
                    // DC
                    // y[n] = x[n] - x[n-1] + alpha * y[n-1]
                    // see http://peabody.sapp.org/class/dmp2/lab/dcblock/
                    tmp.re = I - dev->xn_1.re + ALPHA_DC * dev->yn_1.re ;
                    tmp.im = Q - dev->xn_1.im + ALPHA_DC * dev->yn_1.im ;

                    dev->xn_1.re = I ;
                    dev->xn_1.im = Q ;
                    dev->yn_1.re = tmp.re ;
                    dev->yn_1.im = tmp.im ;

                    samples[i] = tmp ;
                }
                // push samples to SDRNode callback function
                // we only manage one channel per device
                if( (*acqCbFunction)( dev->uuid, (float *)samples, meta.actual_count, 1, &dev->ext_context ) <= 0 ) {
                      free(samples);
                }
            }
        }
        rc = bladerf_enable_module( bladerf_device, BLADERF_MODULE_RX, false);
        dev->running = false ;
    }
    return(NULL);

cmd_calibrate_err:
    bladerf_enable_module( bladerf_device, BLADERF_MODULE_TX, false);
    bladerf_enable_module( bladerf_device, BLADERF_MODULE_RX, false);
    return(NULL);
}

