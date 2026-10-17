// =====================================================================
// lumiere_console.ino - v2.3 "interface animee" (teste avec 2 ESP32-S3 : FTM OK)
// Mesurer la vitesse de la lumiere avec deux ESP32 (Wi-Fi FTM),
// sur la carte ES3C28P (ESP32-S3, ecran 2,8" ILI9341 tactile FT6336, SD).
//
// Architecture :
//   - coeur 0 : tache FTM (mesures Wi-Fi avec la carte A) + tache d'envoi
//     de l'image a l'ecran ;
//   - coeur 1 : loop() lit le tactile et dessine l'image suivante.
//   Deux tampons d'image en PSRAM : pendant que l'un part vers l'ecran,
//   l'autre se dessine (animations plus fluides).
//
// Fichiers du croquis :
//   interface.cpp/.h  ecrans animes et tactile
//   mesure.cpp/.h     echantillons, points, droite, c, simulation
//   gfx.cpp/.h        moteur de dessin lisse (transparence, halos, texte)
//   polices.cpp/.h    polices Inter lissees avec accents (generees)
//   ft6336_touch.*    pilote tactile de la console NES (inchange)
//   plateforme.h      ce que l'interface demande au materiel (ici)
//
// Reglages Arduino IDE (identiques a la console NES) :
//   paquet esp32 3.x, carte "ESP32S3 Dev Module", PSRAM "OPI PSRAM",
//   Flash 16 Mo, partition "16M Flash (3MB APP/9.9MB FATFS)".
//   Bibliotheque TFT_eSPI deja configuree pour l'ecran (meme User_Setup.h).
// =====================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SD_MMC.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "driver/i2c_master.h"
#include "ft6336_touch.h"
#include "gfx.h"
#include "interface.h"
#include "mesure.h"
#include "plateforme.h"

// ---------------- Broches ES3C28P (identiques a la console NES) ---------
#define LCD_BL_PIN     45
#define I2C_SDA        16
#define I2C_SCL        15
#define TOUCH_RST      18
#define SD_SCK         38
#define SD_CMD         40
#define SD_D0          39
#define SD_D1          41
#define SD_D2          48
#define SD_D3          47

#define SSID_LUMIERE   "C_LUMIERE"     // identique a lumiere_repondeur
#define MDP_LUMIERE    "lumiere299792" // identique a lumiere_repondeur
#define NB_TRAMES      16              // trames FTM par session (comme l'exemple officiel)
#define FICHIER_CSV    "/lumiere.csv"

TFT_eSPI tft = TFT_eSPI();

// =========================================================================
// Plateforme (voir plateforme.h)
// =========================================================================
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int s_source = 0;
static volatile bool s_repondeur_ok = false;
static bool s_sd_ok = false;
static Preferences s_prefs;

uint32_t plat_ms() { return millis(); }
void plat_verrou() { portENTER_CRITICAL(&s_mux); }
void plat_deverrou() { portEXIT_CRITICAL(&s_mux); }
void plat_luminosite(int pct) { ledcWrite(LCD_BL_PIN, (uint32_t)(pct * 255 / 100)); }
void plat_source(int s) { s_source = s; }
bool plat_carte_a() { return s_repondeur_ok; }
bool plat_sd() { return s_sd_ok; }
uint32_t plat_memoire_libre() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

void plat_csv(float d, const uint32_t *rtt_ps, const int8_t *rssi, int n)
{
    if (!s_sd_ok) return;
    fs::File f = SD_MMC.open(FICHIER_CSV, FILE_APPEND);
    if (!f) { s_sd_ok = false; return; }
    const char *src = s_source ? "sim" : "reel";
    for (int i = 0; i < n; i++) f.printf("%.2f,%lu,%d,%s\n", d, (unsigned long)rtt_ps[i], rssi[i], src);
    f.close();
}

bool plat_nvs_lire(const char *cle, void *buf, size_t n)
{
    if (s_prefs.getBytesLength(cle) != n) return false;
    return s_prefs.getBytes(cle, buf, n) == n;
}

void plat_nvs_ecrire(const char *cle, const void *buf, size_t n) { s_prefs.putBytes(cle, buf, n); }

// =========================================================================
// Tache FTM (coeur 0) : chronometre les trames avec la carte A
// =========================================================================
static EventGroupHandle_t s_evt;
#define BIT_OK    BIT0
#define BIT_ECHEC BIT1
static volatile uint8_t s_nb_entrees = 0;
static volatile uint8_t s_statut_ftm = 0;

static const char *nom_statut_ftm(uint8_t s)
{
    switch (s) {
    case FTM_STATUS_SUCCESS:       return "succes";
    case FTM_STATUS_UNSUPPORTED:   return "non pris en charge par la carte A";
    case FTM_STATUS_CONF_REJECTED: return "configuration refusee par la carte A";
    case FTM_STATUS_NO_RESPONSE:   return "pas de reponse de la carte A";
    default:                       return "echec";
    }
}

static void evenement_ftm(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    wifi_event_ftm_report_t *r = (wifi_event_ftm_report_t *)data;
    s_nb_entrees = r->ftm_report_num_entries;
    s_statut_ftm = (uint8_t)r->status;
    xEventGroupSetBits(s_evt, r->status == FTM_STATUS_SUCCESS ? BIT_OK : BIT_ECHEC);
}

// Le chronometrage FTM se fait une fois CONNECTE au reseau de la carte A
// (comme l'exemple officiel FTM_Initiator d'Arduino-ESP32).
static bool connecter_carte_a(uint8_t bssid[6], uint8_t *canal)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[LUMIERE] Connexion au reseau %s de la carte A...\n", SSID_LUMIERE);
        WiFi.begin(SSID_LUMIERE, MDP_LUMIERE);
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) vTaskDelay(pdMS_TO_TICKS(200));
        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("[LUMIERE] Carte A injoignable (statut Wi-Fi %d) - nouvel essai\n", (int)WiFi.status());
            WiFi.disconnect();
            return false;
        }
    }
    WiFi.setSleep(false);                          // pas d'economie d'energie : timing stable
    memcpy(bssid, WiFi.BSSID(), 6);
    *canal = (uint8_t)WiFi.channel();
    Serial.printf("[LUMIERE] Connecte a la carte A (%s), canal %d, %d dBm\n",
                  WiFi.BSSIDstr().c_str(), *canal, (int)WiFi.RSSI());
    return true;
}

static void tache_ftm(void *arg)
{
    static wifi_ftm_report_entry_t entrees[64];
    uint8_t bssid[6] = { 0 };
    uint8_t canal = 0;
    uint32_t session = 0;
    int echecs = 0;

    for (;;) {
        if (s_source != 0) {                       // simulation : radio au repos
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (s_repondeur_ok && WiFi.status() != WL_CONNECTED) {
            Serial.println("[LUMIERE] Liaison avec la carte A perdue");
            s_repondeur_ok = false;
        }
        if (!s_repondeur_ok) {
            if (connecter_carte_a(bssid, &canal)) {
                s_repondeur_ok = true;
                echecs = 0;
            } else {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }
        wifi_ftm_initiator_cfg_t cfg = {};
        memcpy(cfg.resp_mac, bssid, 6);
        cfg.channel = canal;
        cfg.frm_count = NB_TRAMES;
        cfg.burst_period = 2;
        cfg.use_get_report_api = true;

        xEventGroupClearBits(s_evt, BIT_OK | BIT_ECHEC);
        esp_err_t err = esp_wifi_ftm_initiate_session(&cfg);
        if (err != ESP_OK) {
            Serial.printf("[LUMIERE] Lancement FTM refuse : %s\n", esp_err_to_name(err));
            if (++echecs > 10) { s_repondeur_ok = false; WiFi.disconnect(); }
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }
        EventBits_t b = xEventGroupWaitBits(s_evt, BIT_OK | BIT_ECHEC, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
        if (b & BIT_OK) {
            echecs = 0;
            uint8_t n = s_nb_entrees > 64 ? 64 : s_nb_entrees;
            if (n && esp_wifi_ftm_get_report(entrees, n) == ESP_OK) {
                for (uint8_t i = 0; i < n; i++) {
                    if (entrees[i].rtt == 0) continue;
                    mes::ajouter(entrees[i].rtt, entrees[i].rssi);
                    Serial.printf("FTM,%lu,%u,%lu,%d\n", (unsigned long)session, i,
                                  (unsigned long)entrees[i].rtt, entrees[i].rssi);
                }
            }
            mes::session();
            session++;
        } else {
            if (!b) Serial.println("[LUMIERE] Session FTM sans reponse (5 s)");
            else    Serial.printf("[LUMIERE] Session FTM : %s\n", nom_statut_ftm(s_statut_ftm));
            esp_wifi_ftm_end_session();
            if (++echecs > 10) { s_repondeur_ok = false; WiFi.disconnect(); }
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// =========================================================================
// Ecran : deux tampons, envoi sur le coeur 0
// =========================================================================
static uint16_t *s_tampon[2] = { nullptr, nullptr };
static uint16_t *s_fond = nullptr;
static bool s_double = false;
static int s_courant = 0;
static volatile int s_a_envoyer = 0;
static SemaphoreHandle_t s_sem_pret, s_sem_libre;

static void envoyer(const uint16_t *img)
{
    tft.pushImage(0, 0, 320, 240, (uint16_t *)img);
}

static void tache_ecran(void *arg)
{
    for (;;) {
        xSemaphoreTake(s_sem_pret, portMAX_DELAY);
        envoyer(s_tampon[s_a_envoyer]);
        xSemaphoreGive(s_sem_libre);
        vTaskDelay(1);                              // laisse respirer le coeur 0
    }
}

static bool init_tampons()
{
    const size_t taille = 320 * 240 * sizeof(uint16_t);
    s_tampon[0] = (uint16_t *)heap_caps_malloc(taille, MALLOC_CAP_SPIRAM);
    s_tampon[1] = (uint16_t *)heap_caps_malloc(taille, MALLOC_CAP_SPIRAM);
    s_fond = (uint16_t *)heap_caps_malloc(taille, MALLOC_CAP_SPIRAM);
    if (s_tampon[0] && s_tampon[1]) {
        s_double = true;
    } else {                                        // pas de PSRAM : un seul tampon interne
        Serial.println("[LUMIERE] PSRAM absente : un seul tampon, animations ralenties");
        if (s_tampon[0]) heap_caps_free(s_tampon[0]);
        if (s_tampon[1]) heap_caps_free(s_tampon[1]);
        if (s_fond) heap_caps_free(s_fond);
        s_fond = nullptr;
        s_tampon[1] = nullptr;
        s_tampon[0] = (uint16_t *)heap_caps_malloc(taille, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!s_tampon[0]) return false;
    }
    return true;
}

// =========================================================================
// Tactile
// =========================================================================
static i2c_master_bus_handle_t s_bus = NULL;
static bool s_tactile_ok = false;

static bool init_tactile()
{
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = (gpio_num_t)I2C_SDA;
    cfg.scl_io_num = (gpio_num_t)I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&cfg, &s_bus) != ESP_OK) return false;
    return ft6336_touch_init(s_bus, (gpio_num_t)TOUCH_RST, 320, 240);
}

static bool lire_appui_brut(int *rx, int *ry)
{
    static int xs[64], ys[64];
    while (ft6336_touch_get_raw(rx, ry)) delay(10);
    uint32_t t0 = millis();
    while (!ft6336_touch_get_raw(rx, ry)) {
        if (millis() - t0 > 30000) return false;
        delay(10);
    }
    int n = 0;
    while (ft6336_touch_get_raw(&xs[n % 64], &ys[n % 64])) { n++; delay(10); }
    if (n > 64) n = 64;
    if (n < 3) return false;
    qsort(xs, n, sizeof(int), [](const void *a, const void *b) { return *(int *)a - *(int *)b; });
    qsort(ys, n, sizeof(int), [](const void *a, const void *b) { return *(int *)a - *(int *)b; });
    *rx = xs[n / 2];
    *ry = ys[n / 2];
    return true;
}

// Etalonnage 3 points, seulement si la console NES ne l'a pas deja fait
// (meme espace NVS "touchcal" : un seul etalonnage pour les deux programmes).
static void etalonner()
{
    const int px[3] = { 30, 290, 160 }, py[3] = { 30, 120, 210 };
    int rx[3], ry[3];
    for (;;) {
        bool ok = true;
        for (int i = 0; i < 3 && ok; i++) {
            gfx::fb = s_tampon[0];
            gfx::degrade_v(0, 0, 320, 240, gfx::rgb(4, 6, 18), gfx::rgb(16, 12, 42));
            gfx::texte(160, 96, "Étalonnage de l'écran", POLICE_16, gfx::rgb(236, 242, 255), 255, gfx::CENTRE);
            gfx::texte(160, 120, "Touche le centre de la croix", POLICE_13, gfx::rgb(156, 168, 210), 255, gfx::CENTRE);
            gfx::halo(px[i], py[i], 18, gfx::rgb(255, 212, 64), 160);
            gfx::trait(px[i] - 12, py[i], px[i] + 12, py[i], 2, gfx::rgb(255, 212, 64));
            gfx::trait(px[i], py[i] - 12, px[i], py[i] + 12, 2, gfx::rgb(255, 212, 64));
            envoyer(s_tampon[0]);
            ok = lire_appui_brut(&rx[i], &ry[i]);
        }
        if (ok && ft6336_touch_set_calibration(rx[0], ry[0], px[0], py[0], rx[1], ry[1], px[1], py[1],
                                               rx[2], ry[2], px[2], py[2])) {
            ft6336_touch_save_calibration();
            return;
        }
    }
}

// =========================================================================
static void init_sd()
{
    if (!SD_MMC.setPins(SD_SCK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)) return;
    if (!SD_MMC.begin("/sdcard", true)) return;      // 1 bit, comme la console NES
    s_sd_ok = true;
    if (!SD_MMC.exists(FICHIER_CSV)) {
        fs::File f = SD_MMC.open(FICHIER_CSV, FILE_WRITE);
        if (f) { f.println("distance_m,rtt_ps,rssi,source"); f.close(); }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[LUMIERE] Console vitesse de la lumiere %s\n", LUMIERE_VERSION);

    ledcAttachChannel(LCD_BL_PIN, 2000, 8, 0);
    ledcWrite(LCD_BL_PIN, 204);
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);                         // RGB565 normal -> ordre de l'ecran
    tft.fillScreen(TFT_BLACK);

    s_prefs.begin("lumiere", false);
    if (!init_tampons()) {
        tft.drawString("Memoire insuffisante", 10, 10, 2);
        for (;;) delay(1000);
    }
    gfx::init(s_tampon[0]);

    s_tactile_ok = init_tactile();
    if (!s_tactile_ok) Serial.println("[LUMIERE] Tactile introuvable");
    else if (!ft6336_touch_load_calibration()) etalonner();

    init_sd();

    s_evt = xEventGroupCreate();
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    // comme l'exemple FTM officiel : 20 MHz, Wi-Fi 4
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    WiFi.setAutoReconnect(true);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_FTM_REPORT, evenement_ftm, NULL);

    ui::init(s_fond);                               // lit les reglages (source...) en NVS

    xTaskCreatePinnedToCore(tache_ftm, "ftm", 6144, NULL, 5, NULL, 0);
    if (s_double) {
        s_sem_pret = xSemaphoreCreateBinary();
        s_sem_libre = xSemaphoreCreateBinary();
        xSemaphoreGive(s_sem_libre);
        xTaskCreatePinnedToCore(tache_ecran, "ecran", 4096, NULL, 2, NULL, 0);
    }
    Serial.printf("[LUMIERE] Pret - PSRAM libre %u Ko, interne %u Ko\n",
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
}

void loop()
{
    int x = 0, y = 0;
    bool appui = s_tactile_ok && ft6336_touch_read(&x, &y);
    ui::toucher(appui, x, y);

    gfx::fb = s_tampon[s_courant];
    ui::image(millis());

    if (s_double) {
        xSemaphoreTake(s_sem_libre, portMAX_DELAY); // l'image precedente est partie
        s_a_envoyer = s_courant;
        xSemaphoreGive(s_sem_pret);
        s_courant ^= 1;
    } else {
        envoyer(s_tampon[0]);
    }
}
