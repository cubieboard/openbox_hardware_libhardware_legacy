/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <poll.h>

#include "hardware_legacy/wifi.h"
#include "libwpa_client/wpa_ctrl.h"

#define LOG_TAG "WifiHW"
#include "cutils/log.h"
#include "cutils/memory.h"
#include "cutils/misc.h"
#include "cutils/properties.h"
#include "private/android_filesystem_config.h"
#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif

static struct wpa_ctrl *ctrl_conn;
static struct wpa_ctrl *monitor_conn;
/* socket pair used to exit from a blocking read */
static int exit_sockets[2] = { -1, -1 };

extern int do_dhcp();
extern int ifc_init();
extern void ifc_close();
extern char *dhcp_lasterror();
extern void get_dhcp_info();
extern int init_module(void *, unsigned long, const char *);
extern int delete_module(const char *, unsigned int);

static char iface[PROPERTY_VALUE_MAX];
// TODO: use new ANDROID_SOCKET mechanism, once support for multiple
// sockets is in

#if defined APM6xxx_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/unifi_sdio.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "unifi_sdio"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG          ""
    #endif
    
#elif defined AR6302_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/ar6302.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "ar6000"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "fwpath=/system/vendor/modules"
    #endif
    
#elif defined AR6003_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/ar6003.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "ar6000"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "fwpath=/system/vendor/modules"
    #endif

#elif defined USI_BM01A_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/usi4329_dhd.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "dhd"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "firmware_path=/system/vendor/modules/usi4329_fw.bin nvram_path=/system/vendor/modules/usi4329_nvram.txt"
    #endif
    
#elif defined HWMW269V2_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/bcm4330.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "bcm4330"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "firmware_path=/system/vendor/modules/bcm4330.bin nvram_path=/system/vendor/modules/bcm4330_nvram.txt"
    #endif

#elif defined HWMW269V3_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/bcm4330.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "bcm4330"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "firmware_path=/system/vendor/modules/mw269v3_fw.bin nvram_path=/system/vendor/modules/mw269v3_nvram.txt"
    #endif
    
#elif defined BCM40181_SDIO_WIFI_USED

    //#ifndef WIFI_DRIVER_MODULE_PATH
    //#define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/bcmdhd.ko"
    //#endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "bcmdhd"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "firmware_path=/system/vendor/modules/fw_bcm40181a2.bin nvram_path=/system/vendor/modules/bcm40181_nvram.txt"
    #endif
    
#elif defined BCM40183_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/bcm40183_dhd.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "dhd"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "firmware_path=/system/vendor/modules/bcm40183_fw.bin nvram_path=/system/vendor/modules/bcm40183_nvram.txt"
    #endif

#elif defined SWBB23_SDIO_WIFI_USED

    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/swbb23_dhd.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "dhd"
    #endif
    #ifndef WIFI_DRIVER_MODULE_ARG
    #define WIFI_DRIVER_MODULE_ARG         "firmware_path=/system/vendor/modules/swbb23_fw.bin nvram_path=/system/vendor/modules/swbb23_nvram.txt"
    #endif
    
#elif defined NANO_SDIO_WIFI_USED 

    /* nano sdio wifi */
    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/nano_ksdio.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "nano_ksdio"
    #endif


    #ifndef WIFI_FIRMWARE_MODULE_PATH
    #define WIFI_FIRMWARE_MODULE_PATH         "/system/vendor/modules/nano_if.ko"
    #endif
    #ifndef WIFI_FIRMWARE_MODULE_NAME
    #define WIFI_FIRMWARE_MODULE_NAME         "nano_if"
    #endif
    #ifndef WIFI_FIRMWARE_MODULE_ARG
    #define WIFI_FIRMWARE_MODULE_ARG          "nrx_config=/system/vendor/modules"
    #endif

static const char FIRMWARE_MODULE_NAME[]  = WIFI_FIRMWARE_MODULE_NAME;
static const char FIRMWARE_MODULE_PATH[]  = WIFI_FIRMWARE_MODULE_PATH;
static const char FIRMWARE_MODULE_ARG[]   = WIFI_FIRMWARE_MODULE_ARG;
    
#elif defined RTL_8192CU_WIFI_USED
    /* rtl8192cu usb wifi */
    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/lib/modules/8192cu.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "8192cu"
    #endif

#elif defined RTL_8188EU_WIFI_USED
    /* rtl8188eu usb wifi */
    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/8188eu.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "8188eu"
    #endif

#elif defined RTL_8723AS_WIFI_USED
    /* rtl8723AS sdio+bt wifi */
    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/8723as.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "8723as"
    #endif

#elif defined RTL_8189ES_WIFI_USED
   /* rtl8189es sdio wifi */
   #ifndef WIFI_DRIVER_MODULE_PATH
   #define WIFI_DRIVER_MODULE_PATH          "/system/vendor/modules/8189es.ko"
   #endif
   #ifndef WIFI_DRIVER_MODULE_NAME
   #define WIFI_DRIVER_MODULE_NAME          "8189es"
   #endif

#elif defined RAL_USB_WIFI_USED
    /* ralink 3070,5370,... usb wifi */
    #ifndef WIFI_DRIVER_MODULE_PATH
    #define WIFI_DRIVER_MODULE_PATH         "/system/vendor/modules/rt5370sta.ko"
    #endif
    #ifndef WIFI_DRIVER_MODULE_NAME
    #define WIFI_DRIVER_MODULE_NAME         "rt5370sta"
    #endif

#endif

#ifndef WIFI_DRIVER_MODULE_ARG
#define WIFI_DRIVER_MODULE_ARG          ""
#endif
#ifndef WIFI_FIRMWARE_LOADER
#define WIFI_FIRMWARE_LOADER		""
#endif
#define WIFI_TEST_INTERFACE		"wlan0" //"sta" --> "wlan0"

#ifndef WIFI_DRIVER_FW_PATH_STA
#define WIFI_DRIVER_FW_PATH_STA		NULL
#endif
#ifndef WIFI_DRIVER_FW_PATH_AP
#define WIFI_DRIVER_FW_PATH_AP		NULL
#endif
#ifndef WIFI_DRIVER_FW_PATH_P2P
#define WIFI_DRIVER_FW_PATH_P2P		NULL
#endif

#if defined(RTL_WIFI_VENDOR)
#undef WIFI_DRIVER_FW_PATH_STA
#define WIFI_DRIVER_FW_PATH_STA         "STA"
#undef WIFI_DRIVER_FW_PATH_AP
#define WIFI_DRIVER_FW_PATH_AP          "AP"
#undef WIFI_DRIVER_FW_PATH_P2P
#define WIFI_DRIVER_FW_PATH_P2P         "P2P"
#endif

#ifndef WIFI_DRIVER_FW_PATH_PARAM
#define WIFI_DRIVER_FW_PATH_PARAM	"/sys/module/wlan/parameters/fwpath"
#endif

#define WIFI_DRIVER_LOADER_DELAY	1000000

static const char IFACE_DIR[]           = "/data/system/wpa_supplicant";
#ifdef WIFI_DRIVER_MODULE_PATH
static const char DRIVER_MODULE_NAME[]  = WIFI_DRIVER_MODULE_NAME;
static const char DRIVER_MODULE_TAG[]   = WIFI_DRIVER_MODULE_NAME " ";
static const char DRIVER_MODULE_PATH[]  = WIFI_DRIVER_MODULE_PATH;
static const char DRIVER_MODULE_ARG[]   = WIFI_DRIVER_MODULE_ARG;
#endif
static const char FIRMWARE_LOADER[]     = WIFI_FIRMWARE_LOADER;
static const char DRIVER_PROP_NAME[]    = "wlan.driver.status";
static const char SUPPLICANT_NAME[]     = "wpa_supplicant";
static const char SUPP_PROP_NAME[]      = "init.svc.wpa_supplicant";
static const char SUPP_CONFIG_TEMPLATE[]= "/system/etc/wifi/wpa_supplicant.conf";
static const char SUPP_CONFIG_FILE[]    = "/data/misc/wifi/wpa_supplicant.conf";
static const char P2P_CONFIG_FILE[]     = "/data/misc/wifi/p2p_supplicant.conf";
static const char CONTROL_IFACE_PATH[]  = "/data/misc/wifi";
static const char MODULE_FILE[]         = "/proc/modules";

static const char SUPP_ENTROPY_FILE[]   = WIFI_ENTROPY_FILE;
static unsigned char dummy_key[21] = { 0x02, 0x11, 0xbe, 0x33, 0x43, 0x35,
                                       0x68, 0x47, 0x84, 0x99, 0xa9, 0x2b,
                                       0x1c, 0xd3, 0xee, 0xff, 0xf1, 0xe2,
                                       0xf3, 0xf4, 0xf5 };

#if defined(RTL_WIFI_VENDOR)
#include <net/if.h>
typedef struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;

int rtw_issue_driver_cmd_fd(int sockfd, const char *ifname, char *cmd, char *buf, size_t buf_len)
{
	int ret;
	struct ifreq ifr;
	android_wifi_priv_cmd priv_cmd;
	
	memset(&ifr, 0, sizeof(ifr));
	memset(&priv_cmd, 0, sizeof(priv_cmd));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

	memcpy(buf, cmd, strlen(cmd) + 1);

	priv_cmd.buf = buf;
	priv_cmd.used_len = buf_len;
	priv_cmd.total_len = buf_len;
	ifr.ifr_data = &priv_cmd;

	if ((ret = ioctl(sockfd, SIOCDEVPRIVATE + 1, &ifr)) < 0) {
		LOGE("%s: failed to issue private command: error:%d %s", __func__, errno, strerror(errno));
	}else {
		ret = 0;
		if ((strcasecmp(cmd, "LINKSPEED") == 0) ||
		    (strcasecmp(cmd, "RSSI") == 0) ||
		    (strcasecmp(cmd, "GETBAND") == 0) ||
		    (strcasecmp(cmd, "P2P_GET_NOA") == 0))
			ret = strlen(buf);

		//LOGE("%s %s len = %d, %d", __func__, buf, ret, strlen(buf));
	}
	
	return ret;
}

int rtw_issue_driver_cmd(const char *ifname, char *cmd, char *buf, size_t buf_len)
{
	int sockfd;
	int ret;

#if 0
	if (ifc_init() < 0)
		return -1;	
	if (ifc_up(ifname)) {
		LOGD("failed to bring up interface %s: %s\n", ifname, strerror(errno));
		return -1;
	}
#endif
	
	sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sockfd< 0) {
		LOGE("%s socket[PF_INET,SOCK_DGRAM] error:%d %s", __FUNCTION__, errno, strerror(errno));
		ret = -1;
		goto bad;
	}

	ret = rtw_issue_driver_cmd_fd(
		sockfd
		, ifname
		, cmd
		, buf
		, buf_len
	);
	
	close(sockfd);
bad:
	return ret;
}
#endif


static int insmod(const char *filename, const char *args)
{
    void *module;
    unsigned int size;
    int ret;

    module = load_file(filename, &size);
    if (!module)
        return -1;

    ret = init_module(module, size, args);

    free(module);

    return ret;
}

static int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        LOGD("Unable to unload driver module \"%s\": %s\n",
             modname, strerror(errno));
    return ret;
}

int do_dhcp_request(int *ipaddr, int *gateway, int *mask,
                    int *dns1, int *dns2, int *server, int *lease) {
    /* For test driver, always report success */
    if (strcmp(iface, WIFI_TEST_INTERFACE) == 0)
        return 0;

    if (ifc_init() < 0)
        return -1;

    if (do_dhcp(iface) < 0) {
        ifc_close();
        return -1;
    }
    ifc_close();
    get_dhcp_info(ipaddr, gateway, mask, dns1, dns2, server, lease);
    return 0;
}

const char *get_dhcp_error_string() {
    return dhcp_lasterror();
}

int is_wifi_driver_loaded() {
    char driver_status[PROPERTY_VALUE_MAX];
#ifdef WIFI_DRIVER_MODULE_PATH
    FILE *proc;
    char line[sizeof(DRIVER_MODULE_TAG)+10];
#endif

    if (!property_get(DRIVER_PROP_NAME, driver_status, NULL)
            || strcmp(driver_status, "ok") != 0) {
        return 0;  /* driver not loaded */
    }
#ifdef WIFI_DRIVER_MODULE_PATH
    /*
     * If the property says the driver is loaded, check to
     * make sure that the property setting isn't just left
     * over from a previous manual shutdown or a runtime
     * crash.
     */
    if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
        LOGW("Could not open %s: %s", MODULE_FILE, strerror(errno));
        property_set(DRIVER_PROP_NAME, "unloaded");
        return 0;
    }
    while ((fgets(line, sizeof(line), proc)) != NULL) {
        if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
            fclose(proc);
            return 1;
        }
    }
    fclose(proc);
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
#else
    return 1;
#endif
}

#define TIME_COUNT 20 // 200ms*20 = 4 seconds for completion
#if defined(RTL_WIFI_VENDOR)
int wifi_load_driver()
{
    char driver_status[PROPERTY_VALUE_MAX];
    int  count = 0;
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
   
    unsigned char tmp_buf[200] = {0};
    char *p_strstr  = NULL;
    int  ret        = 0;
    FILE *fp        = NULL;

    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
            && strcmp(supp_status, "stopping") == 0) {
        LOGD("supplicant status is stopping, try to stop supplicant...");    	
        wifi_stop_supplicant();// wifi on/off test will lead to unblocking problem
        property_get(SUPP_PROP_NAME, supp_status, NULL);
        LOGD("supplicant status = %s", supp_status);    	
    } 
    
    LOGD("start to isnmod %s.ko\n", WIFI_DRIVER_MODULE_NAME);
    
    if (insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG) < 0) {
        LOGE("insmod %s ko failed!", WIFI_DRIVER_MODULE_NAME);
        rmmod(DRIVER_MODULE_NAME);//it may be load driver already,try remove it.
        return -1;
    }    

    do{
       fp=fopen("/proc/net/wireless", "r");
       if (!fp) {
           LOGE("failed to fopen file: /proc/net/wireless\n");
           property_set(DRIVER_PROP_NAME, "failed");
           rmmod(DRIVER_MODULE_NAME);//try remove it.
           return -1;
       }
       ret = fread(tmp_buf, 200, 1, fp);
       if (ret==0){
           LOGD("faied to read proc/net/wireless");
       }
       fclose(fp);

       LOGD("insmod wifi driver...");
       p_strstr = strstr(tmp_buf, "wlan0");
       if (p_strstr != NULL) {
           property_set(DRIVER_PROP_NAME, "ok");
           break;
       }
       usleep(200000);// 200ms

   } while (count++ <= TIME_COUNT); 

   if(count > TIME_COUNT) {
       LOGE("timeout, register netdevice wlan0 failed.");
       property_set(DRIVER_PROP_NAME, "timeout");
       rmmod(DRIVER_MODULE_NAME); 
       return -1;
   }
   return 0;
}
#elif defined NANO_SDIO_WIFI_USED
int wifi_load_driver()
{
    char driver_status[PROPERTY_VALUE_MAX];
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 100; /* wait at most 20 seconds for completion */

    if (is_wifi_driver_loaded()) {
        return 0;
    }
    
    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
            && strcmp(supp_status, "stopping") == 0) {
        LOGD("supplicant status is stopping, try to stop supplicant...");    	
        wifi_stop_supplicant();// wifi on/off test will lead to unblocking problem
        property_get(SUPP_PROP_NAME, supp_status, NULL);
        LOGD("supplicant status = %s", supp_status);    	
    }     

    LOGD("begin insmod nano wifi fw.");
    
    if(insmod(FIRMWARE_MODULE_PATH, FIRMWARE_MODULE_ARG) < 0) {
        LOGE("insmod nano wifi fw failed!");
        rmmod(DRIVER_MODULE_NAME);
        rmmod(FIRMWARE_MODULE_NAME);
        return -1;
    }

    if (insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG) < 0){
        LOGE("insmod wifi ko failed!");
        rmmod(FIRMWARE_MODULE_NAME);          	
        return -1;
    }

    if (strcmp(FIRMWARE_LOADER,"") == 0) {    	
		char tmp_buf[200] = {0};  	
		FILE *profs_entry = NULL;
		int try_time = 0;	
		do {		
			profs_entry = fopen("/proc/net/wireless", "r");
			if(profs_entry == NULL){
				LOGE("open /proc/net/wireless failed!");
				property_set(DRIVER_PROP_NAME, "failed");
				break;
		    }
		    
	        if( 0 == fread(tmp_buf, 200, 1, profs_entry) ){
	            LOGD("faied to read proc/net/wireless");
	        }
			
			if(NULL != strstr(tmp_buf, "wlan0")) {
				LOGD("insmod okay,try_time(%d)", try_time);
			    fclose(profs_entry);
			    profs_entry = NULL;
			    property_set(DRIVER_PROP_NAME, "ok");
			    break;			    
			}else {
				LOGD("nano initial,try_time(%d)",try_time);
				property_set(DRIVER_PROP_NAME, "failed");				    		
			}			
	        fclose(profs_entry);
	        profs_entry = NULL;				
			usleep(200000);
		}while(try_time++ <= TIME_COUNT);// 4 seconds		       
    }
    else {
        property_set("ctl.start", FIRMWARE_LOADER);
    }
    sched_yield();
    while (count-- > 0) {
        if (property_get(DRIVER_PROP_NAME, driver_status, NULL)) {
            if (strcmp(driver_status, "ok") == 0)
                return 0;
            else if (strcmp(DRIVER_PROP_NAME, "failed") == 0) {
                wifi_unload_driver();
                return -1;
            }
        }
        usleep(200000);
    }
    property_set(DRIVER_PROP_NAME, "timeout");
    wifi_unload_driver();
    return -1;	
}
#else
int wifi_load_driver()
{
#ifdef WIFI_DRIVER_MODULE_PATH
    char driver_status[PROPERTY_VALUE_MAX];
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 100; /* wait at most 20 seconds for completion */

    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
            && strcmp(supp_status, "stopping") == 0) {
        LOGD("supplicant status is stopping, try to stop supplicant...");    	
        wifi_stop_supplicant();// wifi on/off test will lead to unblocking problem
        property_get(SUPP_PROP_NAME, supp_status, NULL);
        LOGD("supplicant status = %s", supp_status);    	
    } 

    if (is_wifi_driver_loaded()) {
        return 0;
    }
    
	LOGE("begin to insmod %s %s firmware!", DRIVER_MODULE_PATH, DRIVER_MODULE_ARG);
    if (insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG) < 0) {
        LOGE("insmod %s %s firmware failed!", DRIVER_MODULE_PATH, DRIVER_MODULE_ARG);
        rmmod(DRIVER_MODULE_NAME);//it may be load driver already,try remove it.
        return -1;
    }

    if (strcmp(FIRMWARE_LOADER,"") == 0) {
		char tmp_buf[200] = {0};  	
		FILE *profs_entry = NULL;
		int try_time = 0;	
		do {		
			profs_entry = fopen("/proc/net/wireless", "r");
			if(profs_entry == NULL){
				LOGE("open /proc/net/wireless failed!");
				property_set(DRIVER_PROP_NAME, "failed");
				break;
		    }
		    
	        if( 0 == fread(tmp_buf, 200, 1, profs_entry) ){
	            LOGD("faied to read proc/net/wireless");
	        }
			
			if(NULL != strstr(tmp_buf, "wlan0")) {
				LOGD("insmod okay,try_time(%d)", try_time);
			    fclose(profs_entry);
			    profs_entry = NULL;
			    property_set(DRIVER_PROP_NAME, "ok");
			    break;			    
			}else {
				LOGD("initial,try_time(%d)",try_time);
				property_set(DRIVER_PROP_NAME, "failed");				    		
			}			
	        fclose(profs_entry);
	        profs_entry = NULL;				
			usleep(200000);
		}while(try_time++ <= TIME_COUNT);// 4 seconds	
    }
    else {
        property_set("ctl.start", FIRMWARE_LOADER);
    }
    sched_yield();
    while (count-- > 0) {
        if (property_get(DRIVER_PROP_NAME, driver_status, NULL)) {
            if (strcmp(driver_status, "ok") == 0)
                return 0;
            else if (strcmp(DRIVER_PROP_NAME, "failed") == 0) {
                wifi_unload_driver();
                return -1;
            }
        }
        usleep(200000);
    }
    property_set(DRIVER_PROP_NAME, "timeout");
    wifi_unload_driver();
    return -1;
#else
    property_set(DRIVER_PROP_NAME, "ok");
    return 0;
#endif
}
#endif

int wifi_unload_driver()
{
	LOGD("wifi unload driver.\n");
    usleep(200000); /* allow to finish interface down */
#ifdef WIFI_DRIVER_MODULE_PATH
    if (rmmod(DRIVER_MODULE_NAME) == 0 
#ifdef     NANO_SDIO_WIFI_USED
            && (rmmod(FIRMWARE_MODULE_NAME) == 0) 
#endif
        ) {    	
        int count = 20; /* wait at most 10 seconds for completion */
        while (count-- > 0) {
            if (!is_wifi_driver_loaded())
                break;
            usleep(500000);
        }
        usleep(500000); /* allow card removal */
        if (count) {
            return 0;
        }
        return -1;
    } else
        return -1;
#else
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
#endif
}

int ensure_entropy_file_exists()
{
    int ret;
    int destfd;

    ret = access(SUPP_ENTROPY_FILE, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
            (chmod(SUPP_ENTROPY_FILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            LOGE("Cannot set RW to \"%s\": %s", SUPP_ENTROPY_FILE, strerror(errno));
            return -1;
        }
        return 0;
    }
    destfd = open(SUPP_ENTROPY_FILE, O_CREAT|O_RDWR, 0660);
    if (destfd < 0) {
        LOGE("Cannot create \"%s\": %s", SUPP_ENTROPY_FILE, strerror(errno));
        return -1;
    }

    if (write(destfd, dummy_key, sizeof(dummy_key)) != sizeof(dummy_key)) {
        LOGE("Error writing \"%s\": %s", SUPP_ENTROPY_FILE, strerror(errno));
        close(destfd);
        return -1;
    }
    close(destfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(SUPP_ENTROPY_FILE, 0660) < 0) {
        LOGE("Error changing permissions of %s to 0660: %s",
             SUPP_ENTROPY_FILE, strerror(errno));
        unlink(SUPP_ENTROPY_FILE);
        return -1;
    }

    if (chown(SUPP_ENTROPY_FILE, AID_SYSTEM, AID_WIFI) < 0) {
        LOGE("Error changing group ownership of %s to %d: %s",
             SUPP_ENTROPY_FILE, AID_WIFI, strerror(errno));
        unlink(SUPP_ENTROPY_FILE);
        return -1;
    }
    return 0;
}

int update_ctrl_interface(const char *config_file) {

    int srcfd, destfd;
    int nread;
    char ifc[PROPERTY_VALUE_MAX];
    char *pbuf;
    char *sptr;
    struct stat sb;

    if (stat(config_file, &sb) != 0)
        return -1;

    pbuf = malloc(sb.st_size + PROPERTY_VALUE_MAX);
    if (!pbuf)
        return 0;
    srcfd = open(config_file, O_RDONLY);
    if (srcfd < 0) {
        LOGE("Cannot open \"%s\": %s", config_file, strerror(errno));
        free(pbuf);
        return 0;
    }
    nread = read(srcfd, pbuf, sb.st_size);
    close(srcfd);
    if (nread < 0) {
        LOGE("Cannot read \"%s\": %s", config_file, strerror(errno));
        free(pbuf);
        return 0;
    }

    if (!strcmp(config_file, SUPP_CONFIG_FILE)) {
        property_get("wifi.interface", ifc, WIFI_TEST_INTERFACE);
    } else {
        strcpy(ifc, CONTROL_IFACE_PATH);
    }
    if ((sptr = strstr(pbuf, "ctrl_interface="))) {
        char *iptr = sptr + strlen("ctrl_interface=");
        int ilen = 0;
        int mlen = strlen(ifc);
        int nwrite;
        if (strncmp(ifc, iptr, mlen) != 0) {
            LOGE("ctrl_interface != %s", ifc);
            while (((ilen + (iptr - pbuf)) < nread) && (iptr[ilen] != '\n'))
                ilen++;
            mlen = ((ilen >= mlen) ? ilen : mlen) + 1;
            memmove(iptr + mlen, iptr + ilen + 1, nread - (iptr + ilen + 1 - pbuf));
            memset(iptr, '\n', mlen);
            memcpy(iptr, ifc, strlen(ifc));
            destfd = open(config_file, O_RDWR, 0660);
            if (destfd < 0) {
                LOGE("Cannot update \"%s\": %s", config_file, strerror(errno));
                free(pbuf);
                return -1;
            }
            write(destfd, pbuf, nread + mlen - ilen -1);
            close(destfd);
        }
    }
    free(pbuf);
    return 0;
}

int ensure_config_file_exists(const char *config_file)
{
    char buf[2048];
    int srcfd, destfd;
    struct stat sb;
    int nread;
    int ret;

    ret = access(config_file, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
            (chmod(config_file, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            LOGE("Cannot set RW to \"%s\": %s", config_file, strerror(errno));
            return -1;
        }
        /* return if filesize is at least 10 bytes */
        if (stat(config_file, &sb) == 0 && sb.st_size > 10) {
            return update_ctrl_interface(config_file);
        }
    } else if (errno != ENOENT) {
        LOGE("Cannot access \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    srcfd = open(SUPP_CONFIG_TEMPLATE, O_RDONLY);
    if (srcfd < 0) {
        LOGE("Cannot open \"%s\": %s", SUPP_CONFIG_TEMPLATE, strerror(errno));
        return -1;
    }

    destfd = open(config_file, O_CREAT|O_RDWR, 0660);
    if (destfd < 0) {
        close(srcfd);
        LOGE("Cannot create \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    while ((nread = read(srcfd, buf, sizeof(buf))) != 0) {
        if (nread < 0) {
            LOGE("Error reading \"%s\": %s", SUPP_CONFIG_TEMPLATE, strerror(errno));
            close(srcfd);
            close(destfd);
            unlink(config_file);
            return -1;
        }
        write(destfd, buf, nread);
    }

    close(destfd);
    close(srcfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(config_file, 0660) < 0) {
        LOGE("Error changing permissions of %s to 0660: %s",
             config_file, strerror(errno));
        unlink(config_file);
        return -1;
    }

    if (chown(config_file, AID_SYSTEM, AID_WIFI) < 0) {
        LOGE("Error changing group ownership of %s to %d: %s",
             config_file, AID_WIFI, strerror(errno));
        unlink(config_file);
        return -1;
    }
    return update_ctrl_interface(config_file);
}

/**
 * wifi_wpa_ctrl_cleanup() - Delete any local UNIX domain socket files that
 * may be left over from clients that were previously connected to
 * wpa_supplicant. This keeps these files from being orphaned in the
 * event of crashes that prevented them from being removed as part
 * of the normal orderly shutdown.
 */
void wifi_wpa_ctrl_cleanup(void)
{
    DIR *dir;
    struct dirent entry;
    struct dirent *result;
    size_t dirnamelen;
    size_t maxcopy;
    char pathname[PATH_MAX];
    char *namep;
    char *local_socket_dir = CONFIG_CTRL_IFACE_CLIENT_DIR;
    char *local_socket_prefix = CONFIG_CTRL_IFACE_CLIENT_PREFIX;

    if ((dir = opendir(local_socket_dir)) == NULL)
        return;

    dirnamelen = (size_t)snprintf(pathname, sizeof(pathname), "%s/", local_socket_dir);
    if (dirnamelen >= sizeof(pathname)) {
        closedir(dir);
        return;
    }
    namep = pathname + dirnamelen;
    maxcopy = PATH_MAX - dirnamelen;
    while (readdir_r(dir, &entry, &result) == 0 && result != NULL) {
        if (strncmp(entry.d_name, local_socket_prefix, strlen(local_socket_prefix)) == 0) {
            if (strlcpy(namep, entry.d_name, maxcopy) < maxcopy) {
                unlink(pathname);
            }
        }
    }
    closedir(dir);
}

int wifi_start_supplicant_common(const char *config_file)
{
    char daemon_cmd[PROPERTY_VALUE_MAX];
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 200; /* wait at most 20 seconds for completion */
#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
    const prop_info *pi;
    unsigned serial = 0;
#endif

    wifi_stop_supplicant();
    wifi_close_supplicant_connection();

    /* Check whether already running */
    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
            && strcmp(supp_status, "running") == 0) {
        return 0;
    }

    /* Before starting the daemon, make sure its config file exists */
    if (ensure_config_file_exists(config_file) < 0) {
        LOGE("Wi-Fi will not be enabled");
        return -1;
    }

    if (ensure_entropy_file_exists() < 0) {
        LOGE("Wi-Fi entropy file was not created");
    }

    /* Clear out any stale socket files that might be left over. */
    wifi_wpa_ctrl_cleanup();

#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
    /*
     * Get a reference to the status property, so we can distinguish
     * the case where it goes stopped => running => stopped (i.e.,
     * it start up, but fails right away) from the case in which
     * it starts in the stopped state and never manages to start
     * running at all.
     */
    pi = __system_property_find(SUPP_PROP_NAME);
    if (pi != NULL) {
        serial = pi->serial;
    }
#endif
    property_get("wifi.interface", iface, WIFI_TEST_INTERFACE);
    snprintf(daemon_cmd, PROPERTY_VALUE_MAX, "%s:-i%s -c%s", SUPPLICANT_NAME, iface, config_file);
    property_set("ctl.start", daemon_cmd);
    sched_yield();

    while (count-- > 0) {
#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
        if (pi == NULL) {
            pi = __system_property_find(SUPP_PROP_NAME);
        }
        if (pi != NULL) {
            __system_property_read(pi, NULL, supp_status);
            if (strcmp(supp_status, "running") == 0) {
                return 0;
            } else if (pi->serial != serial &&
                    strcmp(supp_status, "stopped") == 0) {
                return -1;
            }
        }
#else
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
            if (strcmp(supp_status, "running") == 0)
                return 0;
        }
#endif
        usleep(100000);
    }
    return -1;
}

int wifi_start_supplicant()
{
	int ret;
#if defined(RTL_WIFI_VENDOR)
	char ifname[IFNAMSIZ];
	char buf[256];
	
	property_get("wifi.interface", iface, WIFI_TEST_INTERFACE);
	strlcpy(ifname, iface, sizeof(ifname));
	
	rtw_issue_driver_cmd(ifname, "BLOCK 1", buf, 256);
#endif
	ret = wifi_start_supplicant_common(SUPP_CONFIG_FILE);
	
    return ret;
}

int wifi_start_p2p_supplicant()
{
    return wifi_start_supplicant_common(P2P_CONFIG_FILE);
}

int wifi_stop_supplicant()
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 50; /* wait at most 5 seconds for completion */

    /* Check whether supplicant already stopped */
    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
        && strcmp(supp_status, "stopped") == 0) {
        return 0;
    }

    property_set("ctl.stop", SUPPLICANT_NAME);
    sched_yield();

    while (count-- > 0) {
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
            if (strcmp(supp_status, "stopped") == 0)
                return 0;
        }
        usleep(100000);
    }
    return -1;
}

#define SUPPLICANT_TIMEOUT      3000000  // microseconds
#define SUPPLICANT_TIMEOUT_STEP  100000  // microseconds
int wifi_connect_to_supplicant()
{
    char ifname[256];
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int  supplicant_timeout = SUPPLICANT_TIMEOUT;

    /* Make sure supplicant is running */
    if (!property_get(SUPP_PROP_NAME, supp_status, NULL)
            || strcmp(supp_status, "running") != 0) {
        LOGE("Supplicant not running, cannot connect");
        return -1;
    }

    if (access(IFACE_DIR, F_OK) == 0) {
        snprintf(ifname, sizeof(ifname), "%s/%s", IFACE_DIR, iface);
    } else {
        strlcpy(ifname, iface, sizeof(ifname));
    }

    ctrl_conn = wpa_ctrl_open(ifname);
    while (ctrl_conn == NULL && supplicant_timeout > 0) {
        usleep(SUPPLICANT_TIMEOUT_STEP);
        supplicant_timeout -= SUPPLICANT_TIMEOUT_STEP;
        ctrl_conn = wpa_ctrl_open(ifname);
    }
    if (ctrl_conn == NULL) {
        LOGE("1.Unable to open connection to supplicant on \"%s\": %s",
             ifname, strerror(errno));
        return -1;
    }
    monitor_conn = wpa_ctrl_open(ifname);
    if (monitor_conn == NULL) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
        LOGE("2. Unable to open connection to supplicant on \"%s\": %s",
             ifname, strerror(errno));        
        return -1;
    }
    if (wpa_ctrl_attach(monitor_conn) != 0) {
        wpa_ctrl_close(monitor_conn);
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = monitor_conn = NULL;
        return -1;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, exit_sockets) == -1) {
        wpa_ctrl_close(monitor_conn);
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = monitor_conn = NULL;
        return -1;
    }
#if defined(RTL_WIFI_VENDOR)
	{
		char ifname[IFNAMSIZ];
		char buf[256];
	
		strlcpy(ifname, iface, sizeof(ifname));
		rtw_issue_driver_cmd(ifname, "BLOCK 0", buf, 256);
	}
#endif
    return 0;
}

int wifi_send_command(struct wpa_ctrl *ctrl, const char *cmd, char *reply, size_t *reply_len)
{
    int ret;

    if (ctrl_conn == NULL) {
        LOGV("Not connected to wpa_supplicant - \"%s\" command dropped.\n", cmd);
        return -1;
    }
    ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), reply, reply_len, NULL);
    if (ret == -2) {
        LOGD("'%s' command timed out.\n", cmd);
        /* unblocks the monitor receive socket for termination */
        write(exit_sockets[0], "T", 1);
        return -2;
    } else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
        return -1;
    }
    if (strncmp(cmd, "PING", 4) == 0) {
        reply[*reply_len] = '\0';
    }
    return 0;
}

int wifi_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len)
{
    int res;
    int ctrlfd = wpa_ctrl_get_fd(ctrl);
    struct pollfd rfds[2];

    memset(rfds, 0, 2 * sizeof(struct pollfd));
    rfds[0].fd = ctrlfd;
    rfds[0].events |= POLLIN;
    rfds[1].fd = exit_sockets[1];
    rfds[1].events |= POLLIN;
    res = poll(rfds, 2, -1);
    if (res < 0) {
        LOGE("Error poll = %d", res);
        return res;
    }
    if (rfds[0].revents & POLLIN) {
        return wpa_ctrl_recv(ctrl, reply, reply_len);
    } else {
        LOGD("Received on exit socket, terminate");
        return -1;
    }
    return 0;
}

int wifi_wait_for_event(char *buf, size_t buflen)
{
    size_t nread = buflen - 1;
    int fd;
    fd_set rfds;
    int result;
    struct timeval tval;
    struct timeval *tptr;

    if (monitor_conn == NULL) {
        LOGD("Connection closed\n");
        strncpy(buf, WPA_EVENT_TERMINATING " - connection closed", buflen-1);
        buf[buflen-1] = '\0';
        return strlen(buf);
    }

    result = wifi_ctrl_recv(monitor_conn, buf, &nread);
    if (result < 0) {
        LOGD("wifi_ctrl_recv failed: %s\n", strerror(errno));
        strncpy(buf, WPA_EVENT_TERMINATING " - recv error", buflen-1);
        buf[buflen-1] = '\0';
        return strlen(buf);
    }
    buf[nread] = '\0';
    /* LOGD("wait_for_event: result=%d nread=%d string=\"%s\"\n", result, nread, buf); */
    /* Check for EOF on the socket */
    if (result == 0 && nread == 0) {
        /* Fabricate an event to pass up */
        LOGD("Received EOF on supplicant socket\n");
        strncpy(buf, WPA_EVENT_TERMINATING " - signal 0 received", buflen-1);
        buf[buflen-1] = '\0';
        return strlen(buf);
    }
    /*
     * Events strings are in the format
     *
     *     <N>CTRL-EVENT-XXX 
     *
     * where N is the message level in numerical form (0=VERBOSE, 1=DEBUG,
     * etc.) and XXX is the event name. The level information is not useful
     * to us, so strip it off.
     */
    if (buf[0] == '<') {
        char *match = strchr(buf, '>');
        if (match != NULL) {
            nread -= (match+1-buf);
            memmove(buf, match+1, nread+1);
        }
    }
    return nread;
}

void wifi_close_supplicant_connection()
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 50; /* wait at most 5 seconds to ensure init has stopped stupplicant */

    if (ctrl_conn != NULL) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
    }
    if (monitor_conn != NULL) {
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
    }

    if (exit_sockets[0] >= 0) {
        close(exit_sockets[0]);
        exit_sockets[0] = -1;
    }

    if (exit_sockets[1] >= 0) {
        close(exit_sockets[1]);
        exit_sockets[1] = -1;
    }

    while (count-- > 0) {
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
            if (strcmp(supp_status, "stopped") == 0)
                return;
        }
        usleep(100000);
    }
}

int wifi_command(const char *command, char *reply, size_t *reply_len)
{
    return wifi_send_command(ctrl_conn, command, reply, reply_len);
}

const char *wifi_get_fw_path(int fw_type)
{
	LOGD("Enter: %s function, fw_type=%d,", __func__, fw_type);
    switch (fw_type) {
    case WIFI_GET_FW_PATH_STA:
        return WIFI_DRIVER_FW_PATH_STA;
    case WIFI_GET_FW_PATH_AP:
        return WIFI_DRIVER_FW_PATH_AP;
    case WIFI_GET_FW_PATH_P2P:
        return WIFI_DRIVER_FW_PATH_P2P;
    }
    return NULL;
}

int wifi_change_fw_path(const char *fwpath)
{
    int len;
    int fd;
    int ret = 0;    
    static char previous_fwpath[4];

#if defined(RTL_WIFI_VENDOR)

    LOGE("%s: %s\n", __FUNCTION__, fwpath);
    if(strncmp("P2P", fwpath, 3) == 0) {
        ret = wifi_load_driver();
    } else if(strncmp("P2P", previous_fwpath, 3) == 0) {
        ret = wifi_unload_driver();
    }

    strncpy(previous_fwpath, fwpath, 3);

#else
    if (!fwpath)
    	return ret;
	
    fd = open(WIFI_DRIVER_FW_PATH_PARAM, O_WRONLY);
    if (fd < 0) {
        LOGE("Failed to open wlan fw path param (%s)", strerror(errno));
        return -1;
    }
    len = strlen(fwpath) + 1;
    if (write(fd, fwpath, len) != len) {
        LOGE("Failed to write wlan fw path param (%s)", strerror(errno));
        ret = -1;
    }
    close(fd);
#endif
    return ret;
}
