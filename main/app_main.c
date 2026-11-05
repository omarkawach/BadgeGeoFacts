#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "display.h"
#include "summary.h"
#include "world.h"
#include "projection.h"
#include "scheduler.h"
#include "config.h"
#define SIDE_BUTTON 3
#define SHARE_BUTTON 2
#define CONNECTED BIT0
#define CONNECT_FAILED BIT1
static const char *TAG="GEOFACTS";
static const char *topics[]={
 "Okavango_Delta","Richat_Structure","Aral_Sea","Lake_Baikal","Salar_de_Uyuni","Great_Barrier_Reef",
 "Namib","Lake_Titicaca","Mount_Etna","Grand_Canyon","Nile_Delta","Sundarbans","Dead_Sea","Lake_Vostok",
 "Danakil_Depression","Lake_Natron","Mount_Kilimanjaro","Iguazu_Falls","Aorounga_crater","Manicouagan_Reservoir",
 "Mount_St._Helens","Lena_Delta","Ganges_Delta","Mekong_Delta","Lake_Eyre","Lake_Chad","Mount_Fuji","Mauna_Loa",
 "Lake_Tekapo","Vatnajokull","Perito_Moreno_Glacier","Mount_Everest","Mariana_Trench","Lake_Tanganyika",
 "Socotra","Galapagos_Islands","Bering_Strait","Strait_of_Gibraltar","Lake_Malawi","Victoria_Falls",
 "Atacama_Desert","Dallol_(hydrothermal_system)","Wadden_Sea","Bay_of_Fundy","Lake_Toba","Yellowstone_Caldera",
 "Gulf_Stream","Panama_Canal","Suez_Canal","Niger_Delta","Athabasca_oil_sands","Gobi_Desert",
 "Amazon_River","Congo_River","Patagonia","Tibetan_Plateau","Great_Rift_Valley","Deccan_Traps",
 "Landsat_1","Kennedy_Space_Center","Guiana_Space_Centre","Esrange","Svalbard_Satellite_Station",
 "Hawaii","Loess_Plateau","Lake_Urmia","Dasht-e_Lut","Lencois_Maranhenses_National_Park",
 "Zhangye_National_Geopark","Lake_Retba","Wolfe_Creek_Crater","Meteor_Crater","Uluru","Mount_Teide",
 "Rwenzori_Mountains","Giant%27s_Causeway","Aletsch_Glacier","Hubbard_Glacier","Fraser_Island","Everglades"
};
#define TOPIC_COUNT (sizeof(topics)/sizeof(topics[0]))
static Fact current;
static SemaphoreHandle_t state_lock;
static EventGroupHandle_t events;
static nvs_handle_t store;
static volatile bool want_fact=false,want_setup=false,want_resume=false,credentials_changed=false;
static bool ap_active=false,has_wifi=false,wifi_started=false;
static volatile bool station_enabled=false,connection_changed=false;
static unsigned retries_left=0;
static int64_t station_deadline=0,credentials_apply_after=0;
static char ap_ssid[33],ap_password[17],local_ip[24]="not connected",token[33];
static int64_t ap_deadline=0;
static GeoSchedule fact_schedule;
static uint32_t topic_index=0;
static char error_text[96]="";
static bool cache_valid=false;
static int64_t reconnect_due=0;
static bool displayed_wifi=false;

static void copy_json(cJSON *root,const char *key,char *out,size_t size){cJSON *v=cJSON_GetObjectItemCaseSensitive(root,key);if(cJSON_IsString(v)&&v->valuestring)snprintf(out,size,"%s",v->valuestring);}
static void persist_schedule(void){
 if(fact_schedule.last_success_epoch>=GEO_VALID_EPOCH){
  nvs_set_i64(store,"last_fetch",fact_schedule.last_success_epoch);
  nvs_set_i64(store,"next_epoch",fact_schedule.last_success_epoch+fact_schedule.interval_seconds);
  nvs_set_u32(store,"interval_s",fact_schedule.interval_seconds);
 }
 esp_err_t result=nvs_commit(store);if(result!=ESP_OK)ESP_LOGW(TAG,"SCHEDULE_SAVE_FAILED %s",esp_err_to_name(result));
}
static void persist(void){
 nvs_set_blob(store,"fact",&current,sizeof(current));nvs_set_u32(store,"topic",topic_index);persist_schedule();
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data){
 if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_START&&station_enabled){
  esp_err_t e=esp_wifi_connect();ESP_LOGI(TAG,"WIFI_CONNECT_START result=%s",esp_err_to_name(e));
 }
 if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_CONNECTED){
  wifi_event_sta_connected_t *d=data;ESP_LOGI(TAG,"WIFI_ASSOCIATED channel=%u authmode=%u",d->channel,d->authmode);
 }
 if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){
  wifi_event_sta_disconnected_t *d=data;xEventGroupClearBits(events,CONNECTED);
  if(!station_enabled)return;
  ESP_LOGW(TAG,"WIFI_DISCONNECTED reason=%u rssi=%d retries_left=%u",d->reason,d->rssi,retries_left);
  if(d->reason==WIFI_REASON_AUTH_FAIL||d->reason==WIFI_REASON_HANDSHAKE_TIMEOUT||d->reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT)
   snprintf(error_text,sizeof(error_text),"Wi-Fi password handshake failed (code %u)",d->reason);
  else if(d->reason==WIFI_REASON_NO_AP_FOUND)
   snprintf(error_text,sizeof(error_text),"Wi-Fi network not found (code %u)",d->reason);
  else snprintf(error_text,sizeof(error_text),"Wi-Fi connection failed (code %u)",d->reason);
  if(retries_left){--retries_left;esp_wifi_connect();}
  else{station_enabled=false;xEventGroupSetBits(events,CONNECT_FAILED);}
 }
 if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){
  if(!station_enabled)return;
  ip_event_got_ip_t *ip=data;snprintf(local_ip,sizeof(local_ip),IPSTR,IP2STR(&ip->ip_info.ip));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));esp_netif_dns_info_t dns={0};
  if(esp_netif_get_dns_info(ip->esp_netif,ESP_NETIF_DNS_MAIN,&dns)==ESP_OK&&dns.ip.type==ESP_IPADDR_TYPE_V4)
   ESP_LOGI(TAG,"DNS_DHCP_MAIN " IPSTR,IP2STR(&dns.ip.u_addr.ip4));
  dns.ip.type=ESP_IPADDR_TYPE_V4;esp_netif_str_to_ip4("1.1.1.1",&dns.ip.u_addr.ip4);
  ESP_ERROR_CHECK(esp_netif_set_dns_info(ip->esp_netif,ESP_NETIF_DNS_FALLBACK,&dns));
  error_text[0]=0;station_deadline=0;retries_left=2;xEventGroupSetBits(events,CONNECTED);connection_changed=true;
  ESP_LOGI(TAG,"WIFI_CONNECTED http://%s",local_ip);
 }
}
static void stop_radio(void){
 station_enabled=false;
 if(wifi_started){esp_wifi_stop();wifi_started=false;}
 xEventGroupClearBits(events,CONNECTED|CONNECT_FAILED);
}
static void apply_wifi(void){
 char ssid[33]={0},pass[65]={0};size_t n=sizeof(ssid);esp_err_t sr=nvs_get_str(store,"ssid",ssid,&n);
 n=sizeof(pass);esp_err_t pr=nvs_get_str(store,"password",pass,&n);
 has_wifi=sr==ESP_OK&&ssid[0];if(!has_wifi)return;
 if(pr!=ESP_OK){snprintf(error_text,sizeof(error_text),"Saved Wi-Fi password could not be read");return;}
 stop_radio();ap_active=false;reconnect_due=0;
 // Match Espruino: C3 uses station-only mode, zeroed defaults and three attempts.
 wifi_config_t cfg={0};memcpy(cfg.sta.ssid,ssid,strlen(ssid));memcpy(cfg.sta.password,pass,strlen(pass));
 cfg.sta.bssid_set=false;cfg.sta.scan_method=WIFI_ALL_CHANNEL_SCAN;cfg.sta.sort_method=WIFI_CONNECT_AP_BY_SIGNAL;
 ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&cfg));
 wifi_config_t check={0};ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA,&check));
 bool matches=memcmp(cfg.sta.ssid,check.sta.ssid,sizeof(cfg.sta.ssid))==0&&memcmp(cfg.sta.password,check.sta.password,sizeof(cfg.sta.password))==0;
 memset(pass,0,sizeof(pass));memset(&check,0,sizeof(check));memset(&cfg,0,sizeof(cfg));
 ESP_LOGI(TAG,"WIFI_DRIVER_CREDENTIAL_READBACK matches=%s",matches?"true":"false");
 if(!matches){snprintf(error_text,sizeof(error_text),"Wi-Fi driver settings mismatch");xEventGroupSetBits(events,CONNECT_FAILED);return;}
 retries_left=2;station_enabled=true;station_deadline=esp_timer_get_time()+45LL*1000000;
 snprintf(error_text,sizeof(error_text),"Connecting to saved Wi-Fi...");
 ESP_ERROR_CHECK(esp_wifi_start());wifi_started=true;
}
static void setup_start(void){
 if(!ap_active){
  stop_radio();wifi_config_t ap={0};memcpy(ap.ap.ssid,ap_ssid,strlen(ap_ssid));
  snprintf((char*)ap.ap.password,sizeof(ap.ap.password),"%s",ap_password);ap.ap.ssid_len=strlen(ap_ssid);
  ap.ap.channel=1;ap.ap.max_connection=2;ap.ap.authmode=WIFI_AUTH_WPA2_PSK;
  // Never combine AP and STA on ESP32-C3 (same restriction as Espruino).
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
  ESP_ERROR_CHECK(esp_wifi_start());wifi_started=true;ap_active=true;station_deadline=0;
 }
 ap_deadline=esp_timer_get_time()+15LL*60*1000000;
 display_setup(ap_ssid,ap_password);display_refresh();
 ESP_LOGI(TAG,"SETUP_READY: AP-only; join network shown on badge, open http://192.168.4.1");
}
static void setup_stop(void){if(ap_active){stop_radio();esp_wifi_set_mode(WIFI_MODE_NULL);ap_active=false;}}

struct Body {char *data;size_t size;bool overflow;};
static esp_err_t http_event(esp_http_client_event_t *ev){struct Body *b=ev->user_data;if(ev->event_id==HTTP_EVENT_ON_CONNECTED)ESP_LOGI(TAG,"HTTP_CONNECTED");if(ev->event_id==HTTP_EVENT_ON_DATA){if(!b->size)ESP_LOGI(TAG,"HTTP_FIRST_DATA bytes=%d",ev->data_len);if(b->size+ev->data_len>=16384){b->overflow=true;return ESP_FAIL;}memcpy(b->data+b->size,ev->data,ev->data_len);b->size+=ev->data_len;b->data[b->size]=0;}return ESP_OK;}
static bool fetch_fact(Fact *out){
 if(!(xEventGroupGetBits(events)&CONNECTED)){snprintf(error_text,sizeof(error_text),"Wi-Fi offline; keeping saved fact");return false;}
 // Time must be valid before validating HTTPS certificates.
 for(int i=0;time(NULL)<1700000000&&i<30;i++)vTaskDelay(pdMS_TO_TICKS(500));
 if(time(NULL)<1700000000){snprintf(error_text,sizeof(error_text),"Clock sync pending; retrying soon");return false;}
 struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM},*address=NULL;
 int dns_result=getaddrinfo("en.wikipedia.org","443",&hints,&address);
 if(dns_result||!address){ESP_LOGW(TAG,"DNS_LOOKUP_FAILED code=%d",dns_result);snprintf(error_text,sizeof(error_text),"Wikipedia address lookup failed; retrying soon");return false;}
 char resolved[24];inet_ntop(AF_INET,&((struct sockaddr_in*)address->ai_addr)->sin_addr,resolved,sizeof(resolved));
 ESP_LOGI(TAG,"DNS_LOOKUP_OK host=en.wikipedia.org ip=%s",resolved);freeaddrinfo(address);
 for(int attempt=0;attempt<4;attempt++){
 uint32_t index=(topic_index+attempt)%TOPIC_COUNT;char url[256];snprintf(url,sizeof(url),"https://en.wikipedia.org/api/rest_v1/page/summary/%s",topics[index]);
 struct Body body={.data=calloc(1,16384)};if(!body.data)return false;
 esp_http_client_config_t cfg={.url=url,.crt_bundle_attach=esp_crt_bundle_attach,.event_handler=http_event,.user_data=&body,.timeout_ms=45000,.buffer_size=2048,.user_agent="BadgeGeoFacts/1.4 (personal ePaper geography reader)",.keep_alive_enable=false};
 esp_http_client_handle_t client=esp_http_client_init(&cfg);if(!client){free(body.data);return false;}
 esp_http_client_set_header(client,"Accept","application/json");esp_http_client_set_header(client,"Accept-Encoding","identity");esp_http_client_set_header(client,"Connection","close");
 ESP_LOGI(TAG,"FETCH_START free_heap=%lu",(unsigned long)esp_get_free_heap_size());
 esp_err_t err=esp_http_client_perform(client);int status=esp_http_client_get_status_code(client);esp_http_client_cleanup(client);
 cJSON *json=(err==ESP_OK&&status==200&&!body.overflow)?cJSON_Parse(body.data):NULL;
 free(body.data);bool ok=false;
 if(json){cJSON *coords=cJSON_GetObjectItemCaseSensitive(json,"coordinates");cJSON *lat=cJSON_GetObjectItemCaseSensitive(coords,"lat"),*lon=cJSON_GetObjectItemCaseSensitive(coords,"lon");
 memset(out,0,sizeof(*out));copy_json(json,"title",out->title,sizeof(out->title));copy_json(json,"extract",out->text,sizeof(out->text));copy_json(json,"description",out->description,sizeof(out->description));
 cJSON *urls=cJSON_GetObjectItemCaseSensitive(json,"content_urls");copy_json(cJSON_GetObjectItemCaseSensitive(urls,"desktop"),"page",out->url,sizeof(out->url));
 if(cJSON_IsNumber(lat)&&cJSON_IsNumber(lon)&&isfinite(lat->valuedouble)&&isfinite(lon->valuedouble)&&fabs(lat->valuedouble)<=90&&fabs(lon->valuedouble)<=180&&out->text[0]&&out->title[0]&&strncmp(out->url,"https://en.wikipedia.org/wiki/",30)==0){
 out->lat=lat->valuedouble;out->lon=lon->valuedouble;out->online=true;snprintf(out->topic,sizeof(out->topic),"%s",topics[index]);ok=summarize_extract(out->title,out->text,out->summary,sizeof(out->summary));}
 cJSON_Delete(json);}
 if(ok){topic_index=(index+1)%TOPIC_COUNT;error_text[0]=0;ESP_LOGI(TAG,"FETCH_OK topic=%s lat=%.3f lon=%.3f",out->topic,out->lat,out->lon);return true;}
 ESP_LOGW(TAG,"FETCH_RETRY status=%d error=%s",status,esp_err_to_name(err));
 }
 snprintf(error_text,sizeof(error_text),"Online source unavailable; keeping saved fact");return false;
}
static void render_current(void){
 bool connected=(xEventGroupGetBits(events)&CONNECTED)!=0;
 uint32_t remaining=schedule_remaining(&fact_schedule,esp_timer_get_time());
 xSemaphoreTake(state_lock,portMAX_DELAY);display_fact(&current,connected);xSemaphoreGive(state_lock);
 display_refresh();displayed_wifi=connected;
 ESP_LOGI(TAG,"DISPLAY_DONE next_in_seconds=%lu free_heap=%lu",(unsigned long)remaining,(unsigned long)esp_get_free_heap_size());
}
static esp_err_t send_json(httpd_req_t *r){
 cJSON *j=cJSON_CreateObject();xSemaphoreTake(state_lock,portMAX_DELAY);
 double map_x,map_y;world_map_point(current.lat,current.lon,&map_x,&map_y);cJSON_AddNumberToObject(j,"map_x",map_x);cJSON_AddNumberToObject(j,"map_y",map_y);cJSON_AddStringToObject(j,"projection","Equal Earth");
 cJSON_AddStringToObject(j,"title",current.title);cJSON_AddStringToObject(j,"text",current.text);cJSON_AddStringToObject(j,"summary",current.summary);cJSON_AddStringToObject(j,"source",current.url);cJSON_AddNumberToObject(j,"latitude",current.lat);cJSON_AddNumberToObject(j,"longitude",current.lon);cJSON_AddBoolToObject(j,"online",current.online);xSemaphoreGive(state_lock);
 cJSON_AddBoolToObject(j,"wifi_connected",(xEventGroupGetBits(events)&CONNECTED)!=0);cJSON_AddNumberToObject(j,"next_in_seconds",schedule_remaining(&fact_schedule,esp_timer_get_time()));cJSON_AddStringToObject(j,"status",error_text);
 char *s=cJSON_PrintUnformatted(j);httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");esp_err_t e=httpd_resp_sendstr(r,s);free(s);cJSON_Delete(j);return e;
}
static const char *page_top="<!doctype html><html><meta name=viewport content='width=device-width,initial-scale=1'><title>GeoFacts</title><style>body{font:18px system-ui;max-width:760px;margin:32px auto;padding:16px;color:#172123;background:#fffdf4}input,button{font:inherit;padding:12px;margin:8px 0;box-sizing:border-box;width:100%}button{background:#eec03f;color:#172123;border:0;border-radius:8px}canvas{width:100%;image-rendering:pixelated;background:white}a{color:#9d2825}small{color:#445}h1{background:#eec03f;color:#172123;padding:20px;border-radius:12px}</style><h1>GeoFacts</h1>";
static bool on_setup_network(httpd_req_t *r){
 struct sockaddr_storage local={0};socklen_t size=sizeof(local);
 if(!ap_active||getsockname(httpd_req_to_sockfd(r),(struct sockaddr*)&local,&size)!=0)return false;
 uint32_t ap_addr=inet_addr("192.168.4.1");
 if(local.ss_family==AF_INET)return ((struct sockaddr_in*)&local)->sin_addr.s_addr==ap_addr;
 if(local.ss_family==AF_INET6){
  // ESP-IDF HTTP server uses dual-stack sockets: IPv4 peers have mapped IPv6 addresses.
  const uint8_t *bytes=(const uint8_t*)&((struct sockaddr_in6*)&local)->sin6_addr;
  const uint8_t prefix[12]={0,0,0,0,0,0,0,0,0,0,255,255};
  return memcmp(bytes,prefix,12)==0&&memcmp(bytes+12,&ap_addr,4)==0;
 }
 return false;
}
static esp_err_t root_handler(httpd_req_t *r){
 httpd_resp_set_type(r,"text/html");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_sendstr_chunk(r,page_top);
 if(on_setup_network(r)){char form[1300];snprintf(form,sizeof(form),"<h2>Connect your badge</h2><p>Use a 2.4 GHz personal Wi-Fi network. Your credentials are saved only on this badge.</p><form method=post action=/wifi><input type=hidden name=token value='%s'><label>Network name (SSID)<input name=ssid maxlength=32 required autocomplete=off></label><label>Password<input name=password type=password maxlength=63 autocomplete=off></label><button>Save and connect</button></form><p>The setup network closes after a successful connection. Rejoin your usual Wi-Fi to read the fact page.</p>",token);httpd_resp_sendstr_chunk(r,form);}
 else {httpd_resp_sendstr_chunk(r,"<h2 id=title>Loading current fact...</h2><p id=fact></p><canvas id=map width=340 height=170></canvas><p id=coords></p><a id=source target=_blank rel=noopener>Read source / Wikipedia contributors</a><p><small>Wikipedia text: CC BY-SA. The marker identifies the subject location, not the source publisher. Map: Natural Earth, public domain.</small></p><p id=status></p><p>Press the side button on the badge to fetch another fact. Hold SHARE for 3 seconds for Wi-Fi setup.</p><script>fetch('/api/fact').then(r=>r.json()).then(f=>{document.getElementById('title').textContent=f.title;document.getElementById('fact').textContent=f.text;document.getElementById('coords').textContent=f.latitude.toFixed(3)+', '+f.longitude.toFixed(3);document.getElementById('source').href=f.source;document.getElementById('status').textContent=f.status||'New fact every hour';let im=new Image;im.onload=()=>{let c=document.getElementById('map').getContext('2d');c.drawImage(im,0,0);let x=f.map_x,y=f.map_y;c.strokeStyle='white';c.lineWidth=4;c.fillStyle='#c83232';c.beginPath();c.arc(x,y,5,0,7);c.fill();c.stroke()};im.src='/world.bmp'});setTimeout(()=>location.reload(),60000)</script>");}
 return httpd_resp_send_chunk(r,NULL,0);
}
static esp_err_t bmp_handler(httpd_req_t *r){
 uint8_t hdr[62]={0};uint32_t size=62+44*170;hdr[0]='B';hdr[1]='M';memcpy(hdr+2,&size,4);hdr[10]=62;hdr[14]=40;uint32_t w=340,h=170;memcpy(hdr+18,&w,4);memcpy(hdr+22,&h,4);hdr[26]=1;hdr[28]=1;hdr[46]=2;hdr[54]=255;hdr[55]=255;hdr[56]=255;
 httpd_resp_set_type(r,"image/bmp");httpd_resp_send_chunk(r,(char*)hdr,62);
 for(int y=169;y>=0;y--){uint8_t row[44]={0};memcpy(row,world_bits+y*43,43);if(httpd_resp_send_chunk(r,(char*)row,44)!=ESP_OK)return ESP_FAIL;}
 return httpd_resp_send_chunk(r,NULL,0);
}
static bool decode(char *out,size_t cap,const char *s){size_t n=0;for(size_t i=0;s[i];i++){int c=(unsigned char)s[i];if(c=='+')c=' ';else if(c=='%'){if(!s[i+1]||!s[i+2])return false;char h[3]={s[i+1],s[i+2],0};char *end;c=strtol(h,&end,16);if(*end)return false;i+=2;}if(c==0||n+1>=cap)return false;out[n++]=c;}out[n]=0;return true;}
static esp_err_t wifi_handler(httpd_req_t *r){
 if(!on_setup_network(r))return httpd_resp_send_err(r,HTTPD_403_FORBIDDEN,"Hold SHARE to enable Wi-Fi setup");
 char body[512],ss[100],pw[200],tk[60],ssid[33],pass[64];if(r->content_len<=0||r->content_len>=sizeof(body))return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Form too large");
 int used=0;while(used<r->content_len){int n=httpd_req_recv(r,body+used,r->content_len-used);if(n<=0)return ESP_FAIL;used+=n;}body[used]=0;
 if(httpd_query_key_value(body,"token",tk,sizeof(tk))!=ESP_OK||strcmp(tk,token)||httpd_query_key_value(body,"ssid",ss,sizeof(ss))!=ESP_OK||httpd_query_key_value(body,"password",pw,sizeof(pw))!=ESP_OK||!decode(ssid,sizeof(ssid),ss)||!decode(pass,sizeof(pass),pw)||!ssid[0]||(pass[0]&&strlen(pass)<8))return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Check network name and password (8-63 characters, or empty for open Wi-Fi).");
 nvs_set_str(store,"ssid",ssid);nvs_set_str(store,"password",pass);ESP_ERROR_CHECK(nvs_commit(store));memset(pass,0,sizeof(pass));memset(body,0,sizeof(body));credentials_apply_after=esp_timer_get_time()+1000000;credentials_changed=true;
 httpd_resp_set_type(r,"text/html");
 return httpd_resp_sendstr(r,"<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>Connecting badge</title><body style='font:20px system-ui;max-width:650px;margin:40px auto;padding:20px'><h1>Connecting your badge</h1><p id=status>Saved. Trying your Wi-Fi network...</p><p>If this network disappears, rejoin your usual Wi-Fi. The badge screen will show its new address.</p><p><a href='/'>Change Wi-Fi details</a></p><script>setInterval(()=>fetch('/api/fact').then(r=>r.json()).then(f=>document.getElementById('status').textContent=f.wifi_connected?'Connected. Rejoin your usual Wi-Fi and check the badge screen.':f.status||'Connecting...').catch(()=>{}),2500)</script>");
}

static void buttons_task(void *arg){
 int side_count=0,share_count=0;bool side_fired=false,share_fired=false;
 for(;;){
 if(!gpio_get_level(SIDE_BUTTON)){if(side_count<1000)side_count++;if(side_count>=3&&!side_fired){want_fact=true;side_fired=true;ESP_LOGI(TAG,"SIDE_BUTTON: new fact requested");}}else{side_count=0;side_fired=false;}
 if(!gpio_get_level(SHARE_BUTTON)){if(share_count<1000)share_count++;if(share_count>=150&&!share_fired){want_setup=true;share_fired=true;ESP_LOGI(TAG,"SHARE_BUTTON: setup toggle requested");}}else{if(share_count>=3&&!share_fired){want_resume=true;ESP_LOGI(TAG,"SHARE_BUTTON: return/reconnect requested");}share_count=0;share_fired=false;}
 vTaskDelay(pdMS_TO_TICKS(20));}
}
void app_main(void){
 ESP_LOGI(TAG,"Badge GeoFacts 1.4 / rev1.2 / hourly online updates / side GPIO%d / SHARE GPIO%d",SIDE_BUTTON,SHARE_BUTTON);
 esp_err_t err=nvs_flash_init();if(err==ESP_ERR_NVS_NO_FREE_PAGES||err==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());err=nvs_flash_init();}ESP_ERROR_CHECK(err);
 ESP_ERROR_CHECK(nvs_open("geofacts",NVS_READWRITE,&store));state_lock=xSemaphoreCreateMutex();events=xEventGroupCreate();
 size_t n=sizeof(current);cache_valid=nvs_get_blob(store,"fact",&current,&n)==ESP_OK&&(n==sizeof(current)||n==sizeof(LegacyFact)||n==sizeof(LegacySummaryFact));
 if(cache_valid){if(n!=sizeof(current))current.description[0]=0;current.description[sizeof(current.description)-1]=0;current.title[sizeof(current.title)-1]=0;current.text[sizeof(current.text)-1]=0;cache_valid=summarize_extract(current.title,current.text,current.summary,sizeof(current.summary));}
 if(!cache_valid){memset(&current,0,sizeof(current));snprintf(current.title,sizeof(current.title),"Okavango Delta");snprintf(current.text,sizeof(current.text),"The Okavango Delta is an inland delta in Botswana. It forms where the Okavango River reaches the Kalahari basin. Connect Wi-Fi to fetch a new geography fact and its location every hour.");snprintf(current.url,sizeof(current.url),"https://en.wikipedia.org/wiki/Okavango_Delta");current.lat=-19.4;current.lon=22.9;summarize_extract(current.title,current.text,current.summary,sizeof(current.summary));}
 current.online=false;current.title[sizeof(current.title)-1]=0;current.text[sizeof(current.text)-1]=0;current.url[sizeof(current.url)-1]=0;
 nvs_get_u32(store,"topic",&topic_index);topic_index%=TOPIC_COUNT;
 int64_t last_success=0;
 if(nvs_get_i64(store,"last_fetch",&last_success)!=ESP_OK){
  int64_t legacy_due=0;uint32_t legacy_interval=14400;nvs_get_i64(store,"next_epoch",&legacy_due);nvs_get_u32(store,"interval_s",&legacy_interval);
  last_success=schedule_legacy_last_success(legacy_due,legacy_interval);
 }
 schedule_init(&fact_schedule,esp_timer_get_time(),time(NULL),cache_valid,last_success,FACT_INTERVAL_SECONDS);
 if(cache_valid&&fact_schedule.clock_restored)persist_schedule();
 uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);snprintf(ap_ssid,sizeof(ap_ssid),"GeoFacts-%02X%02X",mac[4],mac[5]);
 static const char chars[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
 for(int i=0;i<12;i++){ap_password[i]=chars[esp_random()%(sizeof(chars)-1)];}
 ap_password[12]=0;
 for(int i=0;i<16;i++)snprintf(token+i*2,3,"%02x",(unsigned)(esp_random()&255));
 ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_sta();esp_netif_create_default_wifi_ap();
 wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
 ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL));
 apply_wifi();
 esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_setservername(1,"time.cloudflare.com");esp_sntp_init();
 httpd_handle_t server=NULL;httpd_config_t http=HTTPD_DEFAULT_CONFIG();http.stack_size=6144;http.max_open_sockets=3;http.lru_purge_enable=true;
 ESP_ERROR_CHECK(httpd_start(&server,&http));
 httpd_uri_t routes[]={ {.uri="/",.method=HTTP_GET,.handler=root_handler},{.uri="/api/fact",.method=HTTP_GET,.handler=send_json},{.uri="/world.bmp",.method=HTTP_GET,.handler=bmp_handler},{.uri="/wifi",.method=HTTP_POST,.handler=wifi_handler} };
 for(int i=0;i<4;i++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
 gpio_config_t btn={.pin_bit_mask=(1ULL<<SIDE_BUTTON)|(1ULL<<SHARE_BUTTON),.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};ESP_ERROR_CHECK(gpio_config(&btn));
 xTaskCreate(buttons_task,"buttons",3072,NULL,4,NULL);display_init();
 if(has_wifi)xEventGroupWaitBits(events,CONNECTED|CONNECT_FAILED,pdFALSE,pdFALSE,pdMS_TO_TICKS(45000));
 if(!(xEventGroupGetBits(events)&CONNECTED)){
  if(has_wifi){stop_radio();reconnect_due=esp_timer_get_time()+30000000;render_current();}
  else setup_start();
 }else {render_current();}
 for(;;){
 int64_t now=esp_timer_get_time();
 if(schedule_sync_clock(&fact_schedule,now,time(NULL))){persist_schedule();ESP_LOGI(TAG,"SCHEDULE_RESTORED next_in_seconds=%lu",(unsigned long)schedule_remaining(&fact_schedule,now));}
 if(credentials_changed&&now>=credentials_apply_after){credentials_changed=false;apply_wifi();render_current();}
 if(want_resume){want_resume=false;if(ap_active){setup_stop();if(has_wifi)apply_wifi();render_current();}else if(has_wifi&&!(xEventGroupGetBits(events)&CONNECTED)){apply_wifi();render_current();}}
 if(want_setup){want_setup=false;if(ap_active){setup_stop();if(has_wifi)apply_wifi();render_current();}else setup_start();}
 if(connection_changed){connection_changed=false;
  // A reconnect updates connectivity only; it never advances the topic or deadline.
  if(cache_valid&&!displayed_wifi&&!schedule_should_fetch(&fact_schedule,now,true,want_fact))render_current();
 }
 if(!ap_active&&!(xEventGroupGetBits(events)&CONNECTED)&&((xEventGroupGetBits(events)&CONNECT_FAILED)||(station_enabled&&station_deadline&&now>=station_deadline))){
  stop_radio();station_deadline=0;reconnect_due=esp_timer_get_time()+30000000;
  ESP_LOGI(TAG,"WIFI_RETRY_SCHEDULED seconds=30");if(displayed_wifi)render_current();
 }
 if(!ap_active&&has_wifi&&!station_enabled&&!wifi_started&&reconnect_due&&now>=reconnect_due){apply_wifi();}
 if(ap_active&&want_fact&&!(xEventGroupGetBits(events)&CONNECTED)){want_fact=false;snprintf(error_text,sizeof(error_text),"Wi-Fi unavailable; offline fact shown. Hold SHARE for setup");render_current();}
 if(ap_active&&now>ap_deadline){setup_stop();if(has_wifi)apply_wifi();render_current();}
 if(!ap_active&&schedule_should_fetch(&fact_schedule,now,(xEventGroupGetBits(events)&CONNECTED)!=0,want_fact)){
 ESP_LOGI(TAG,"FETCH_TRIGGER reason=%s",want_fact?"side_button":"interval_due");
 want_fact=false;Fact fresh;
 if(fetch_fact(&fresh)){xSemaphoreTake(state_lock,portMAX_DELAY);current=fresh;schedule_succeeded(&fact_schedule,esp_timer_get_time(),time(NULL));cache_valid=true;persist();xSemaphoreGive(state_lock);}
 else{schedule_failed(&fact_schedule,esp_timer_get_time());current.online=false;}
 render_current();}
 vTaskDelay(pdMS_TO_TICKS(100));}
}
