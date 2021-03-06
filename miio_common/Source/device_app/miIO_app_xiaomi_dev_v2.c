/******************************************************************************
 *
 * Model: xiaomi.dev.v1
 *
 * Property:
 *
 * Method:
 *
 * Event:
 *
 *
 ******************************************************************************
 */
#include <lwip/lwipopts.h>
#include <lwip/arch.h>
#include <wm_os.h>
#include <ot/d0_otd.h>
#include <lib/jsmn/jsmn_api.h>
#include <lib/arch_os.h>
#include <lib/arch_io.h>
#include <lib/button_detect_framework.h>
#include <misc/led_indicator.h>
#include <appln_dbg.h>
#include <board.h>
#include <mdev_gpt.h>
#include <lowlevel_drivers.h>
#include <app_framework.h>
#include <main_extension.h>
#include <lib/miio_up_manager.h>
#include <lib/uap_diagnosticate.h>
#include <lib/local_timer.h>

const int MIIO_APP_VERSION = 0;

//*****************************button about*****************************************/
#define PERIOD_CHECK_BUTTON 20//msec

static os_semaphore_t btn_pressed_sem;
static os_timer_t g_reset_prov_timer;
mum g_mum;

static void reset_prov(os_timer_arg_t arg)
{
    wmprintf("start deprovisioning\r\n");
    if(is_provisioned()) {
        app_reset_saved_network();
    }
    else{
        pm_reboot_soc(); //reset again,just reboot
    }
}

static void button_1_press_handle(GPIO_IO_Type pin_level)
{
    if (pin_level == GPIO_IO_LOW) {
        LOG_INFO("button pressed\r\n");

        if (os_timer_is_running(&g_reset_prov_timer)) {
            if (WM_SUCCESS != os_timer_deactivate(&g_reset_prov_timer)) {
                LOG_ERROR("deactivating reset provision timer failed\r\n");
            }
        }
		
        if(WM_SUCCESS != os_timer_activate(&g_reset_prov_timer)) {
			LOG_ERROR("activating reset provision timer failed\r\n");
		}

    } else {
        LOG_INFO("button released\r\n");
        os_semaphore_put(&btn_pressed_sem);

        if (os_timer_is_running(&g_reset_prov_timer)) {
            if (WM_SUCCESS != os_timer_deactivate(&g_reset_prov_timer)) {
                LOG_ERROR("deactivating reset provision timer failed\r\n");
            }
        }
    }
}
//****************************button about end**********************************/

void miio_app_thread(void* arg)
{
    static bool has_sync_time = false;
    g_mum = mum_create();

//*****************************button init*******************************/
    os_timer_create(&g_reset_prov_timer, "reset-prov-timer", os_msec_to_ticks(4000), reset_prov, NULL,
            OS_TIMER_ONE_SHOT, OS_TIMER_NO_ACTIVATE);

    button_frame_work_init();
    register_button_callback(0, button_1_press_handle);

    if (WM_SUCCESS != os_semaphore_create(&btn_pressed_sem, "btn_pressed_sem")) {
        LOG_ERROR("btn_pressed_sem creation failed\r\n");
    }
    else if (WM_SUCCESS != os_semaphore_get(&btn_pressed_sem, OS_WAIT_FOREVER)) {
        LOG_ERROR("first get btn_pressed_sem failed.\r\n");
    }
//******************************button init end*******************************************/

    local_timer_init();//add by xuemulong@2016.02.14

    miio_led_on();
    LOG_INFO("miio_app_thread while loop start.\r\n");

    while (1) {
        if (WM_SUCCESS == os_semaphore_get(&btn_pressed_sem, OS_NO_WAIT)) {
            mum_set_property(g_mum,"button_pressed","\"wifi_rst\"");
        }

        if(has_sync_time)//only after sync time, we start check timer
            check_schedule_and_set_timer();
        else if(otn_is_online())
            has_sync_time = true;

        os_thread_sleep(os_msec_to_ticks(PERIOD_CHECK_BUTTON));
    }

    local_timer_deinit();//add by xuemulong@2016.02.14

//*****************************button deinit*******************************/
    mum_destroy(&g_mum);
    os_timer_delete(&g_reset_prov_timer);
    os_semaphore_delete(&btn_pressed_sem);
//******************************button deinit end*******************************************/

}

void miio_app_test_thread(void* arg)
{
    /*if(STATE_OK == sell_ready()){
     LOG_INFO("==================\r\n");
     LOG_INFO(" OK. Sell ready. \r\n");
     LOG_INFO("==================\r\n");
     }*/

    while (1) {
        os_thread_sleep(os_msec_to_ticks(500));
    }
}

void (*app_thread)(void* arg) = miio_app_thread;
void (*app_test_thread)(void* arg) = miio_app_test_thread;

// TO override the pin start mode in mc200_startup.c, so that LED are off at startup
void SetPinStartMode(void)
{
    GPIO_PinModeConfig(board_led_1().gpio, PINMODE_PULLUP);
    GPIO_PinModeConfig(board_led_2().gpio, PINMODE_PULLUP);
}

void product_set_wifi_cal_data(void)
{

}
