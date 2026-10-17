// =====================================================================
// son_console.ino - v1.1 "Vitesse du son" : echo + effet Doppler
// v1.1 : son Doppler genere sans calcul double (le coeur 0 saturait), volume
//        du son continu plafonne, cause du dernier redemarrage affichee.
// Sur la carte ES3C28P (ESP32-S3, ecran 2,8" ILI9341 tactile FT6336, SD,
// codec audio ES8311 avec haut-parleur et micro MEMS integres).
//
//   ECHO    : un "tic" (chirp 1,5 -> 6,5 kHz) part du haut-parleur, le micro
//             entend le son direct puis l'echo d'un mur. Retard mesure a
//             quelques microsecondes pres -> droite temps/distance -> c.
//   DOPPLER : un son pur (5, 10 ou 17 kHz) en continu ; quelqu'un qui marche
//             devant renvoie un son decale de df ~ 2 v f0 / c -> sa vitesse.
//
// Architecture :
//   - coeur 0 : tache "audio" (I2S 48 kHz en continu : emission + micro,
//               meme horloge), tache "moteur" (FFT, analyse), tache "ecran" ;
//   - coeur 1 : loop() lit le tactile et dessine l'image suivante.
//
// Fichiers du croquis :
//   interface.cpp/.h  ecrans animes et tactile
//   son.cpp/.h        points, droite, c, passages Doppler, statistiques
//   moteur.cpp/.h     boucle de mesure (pings, analyse continue, simulation)
//   dsp.cpp/.h        FFT, filtre adapte, analyse Doppler, sons simules
//   gfx.cpp/.h        moteur de dessin lisse (identique a lumiere_console)
//   polices.cpp/.h    polices Inter lissees avec accents (generees)
//   ft6336_touch.*    pilote tactile de la console NES (inchange)
//   plateforme.h      ce que l'interface demande au materiel (ici)
//
// Reglages Arduino IDE (identiques a la console NES et a lumiere_console) :
//   paquet esp32 3.x, carte "ESP32S3 Dev Module", PSRAM "OPI PSRAM",
//   Flash 16 Mo, partition "16M Flash (3MB APP/9.9MB FATFS)".
//   Bibliotheque TFT_eSPI deja configuree pour l'ecran (meme User_Setup.h).
// =====================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD_MMC.h>
#include <Preferences.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "ft6336_touch.h"
#include "gfx.h"
#include "interface.h"
#include "son.h"
#include "dsp.h"
#include "moteur.h"
#include "plateforme.h"

// ---------------- Broches ES3C28P ----------------------------------------
#define LCD_BL_PIN     45
#define I2C_SDA        16              // bus partage : tactile FT6336 + codec ES8311
#define I2C_SCL        15
#define TOUCH_RST      18
#define SD_SCK         38
#define SD_CMD         40
#define SD_D0          39
#define SD_D1          41
#define SD_D2          48
#define SD_D3          47
#define I2S_MCLK       4
#define I2S_BCLK       5
#define I2S_WS         7
#define I2S_DOUT       8               // ESP32 -> codec (haut-parleur)
#define I2S_DIN        6               // codec -> ESP32 (micro)
#define PA_PIN         1               // ampli du haut-parleur (niveau actif teste au demarrage)
#define ES8311_ADR     0x18

#define FICHIER_CSV    "/son.csv"
#define CH             240             // trames I2S par passage de la tache audio (5 ms)
#define ANNEAU_N       16384           // micro pendant le son continu (341 ms)

TFT_eSPI tft = TFT_eSPI();

// =========================================================================
// Plateforme (voir plateforme.h)
// =========================================================================
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_sd_ok = false;
static Preferences s_prefs;
static bool s_audio_ok = false;
static char s_audio_etat[40] = "démarrage…";

uint32_t plat_ms() { return millis(); }
void plat_verrou() { portENTER_CRITICAL(&s_mux); }
void plat_deverrou() { portEXIT_CRITICAL(&s_mux); }
void plat_luminosite(int pct) { ledcWrite(LCD_BL_PIN, (uint32_t)(pct * 255 / 100)); }
bool plat_sd() { return s_sd_ok; }
uint32_t plat_memoire_libre() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
bool plat_audio_ok() { return s_audio_ok; }
const char *plat_audio_etat() { return s_audio_etat; }

void plat_csv(const char *ligne)
{
    if (!s_sd_ok) return;
    fs::File f = SD_MMC.open(FICHIER_CSV, FILE_APPEND);
    if (!f) { s_sd_ok = false; return; }
    f.println(ligne);
    f.close();
}

bool plat_nvs_lire(const char *cle, void *buf, size_t n)
{
    if (s_prefs.getBytesLength(cle) != n) return false;
    return s_prefs.getBytes(cle, buf, n) == n;
}

void plat_nvs_ecrire(const char *cle, const void *buf, size_t n) { s_prefs.putBytes(cle, buf, n); }

// =========================================================================
// Codec ES8311 (I2C) : esclave, MCLK = 256 x 48 kHz, I2S 16 bits
// =========================================================================
static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_codec = NULL;
static int s_volume = 1, s_gain = 1;

static bool es_ecrire(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_codec, b, 2, 50) == ESP_OK;
}

static int es_lire(uint8_t reg)
{
    uint8_t v = 0;
    if (i2c_master_transmit_receive(s_codec, &reg, 1, &v, 1, 50) != ESP_OK) return -1;
    return v;
}

static void es_niveaux()
{
    if (!s_codec) return;
    static const uint8_t VOL[3] = { 0xA8, 0xB6, 0xBF };     // DAC : -11,5 / -4,5 / 0 dB
    static const uint8_t PGA[3] = { 0x14, 0x17, 0x1A };     // micro : +12 / +21 / +30 dB
    es_ecrire(0x32, VOL[s_volume]);
    es_ecrire(0x14, PGA[s_gain]);
}

void plat_audio_niveaux(int volume, int gain)
{
    s_volume = volume < 0 ? 0 : (volume > 2 ? 2 : volume);
    s_gain = gain < 0 ? 0 : (gain > 2 ? 2 : gain);
    es_niveaux();
}

static bool init_codec()
{
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = ES8311_ADR;
    cfg.scl_speed_hz = 100000;
    if (i2c_master_bus_add_device(s_bus, &cfg, &s_codec) != ESP_OK) return false;
    int id1 = es_lire(0xFD), id2 = es_lire(0xFE);
    Serial.printf("[SON] ES8311 identifiant %02X %02X\n", id1, id2);
    if (id1 < 0) return false;

    // remise a zero puis configuration (sequence des pilotes Espressif)
    es_ecrire(0x00, 0x1F); delay(20); es_ecrire(0x00, 0x00);
    es_ecrire(0x45, 0x00);
    es_ecrire(0x01, 0x30); es_ecrire(0x02, 0x00); es_ecrire(0x03, 0x10);
    es_ecrire(0x16, 0x24); es_ecrire(0x04, 0x10); es_ecrire(0x05, 0x00);
    es_ecrire(0x0B, 0x00); es_ecrire(0x0C, 0x00); es_ecrire(0x10, 0x1F); es_ecrire(0x11, 0x7F);
    es_ecrire(0x00, 0x80);                          // esclave, mise en route
    // horloges pour MCLK 12,288 MHz / 48 kHz
    es_ecrire(0x01, 0x3F);                          // MCLK depuis la broche, toutes horloges actives
    es_ecrire(0x02, 0x00); es_ecrire(0x05, 0x00);
    es_ecrire(0x03, 0x10); es_ecrire(0x04, 0x10);
    es_ecrire(0x07, 0x00); es_ecrire(0x08, 0xFF); es_ecrire(0x06, 0x03);
    // format I2S 16 bits, entree et sortie non muettes
    es_ecrire(0x09, 0x0C); es_ecrire(0x0A, 0x0C);
    // parties analogiques
    es_ecrire(0x13, 0x10);
    es_ecrire(0x1B, 0x0A); es_ecrire(0x1C, 0x6A);  // filtre passe-haut de l'ADC
    es_ecrire(0x17, 0xBF);                          // volume numerique ADC 0 dB
    es_ecrire(0x0E, 0x02);
    es_ecrire(0x12, 0x00);                          // DAC alimente
    es_ecrire(0x0D, 0x01);
    es_ecrire(0x15, 0x40);
    es_ecrire(0x37, 0x08);
    es_ecrire(0x45, 0x00);
    es_niveaux();
    return true;
}

// =========================================================================
// I2S en continu (coeur 0, haute priorite) : chaque passage ecrit CH
// echantillons vers le haut-parleur et lit CH echantillons du micro.
// Emission et reception partagent la meme horloge : le retard entre les
// deux est constant, et de toute facon l'echo est mesure par rapport au
// son direct enregistre dans le meme tampon.
// =========================================================================
static i2s_chan_handle_t s_tx = NULL, s_rx = NULL;

static volatile bool p_actif = false;           // ping en cours
static const int16_t *p_tx = nullptr;
static int16_t *p_rx = nullptr;
static volatile int p_n = 0, p_pos = 0;
static SemaphoreHandle_t p_fin;

static volatile float a_f0 = 0, a_amp = 0;      // son continu
static int16_t *s_anneau = nullptr;
static volatile uint32_t r_ecrit = 0, r_lu = 0;

static bool init_i2s()
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.dma_desc_num = 4;
    cc.dma_frame_num = CH;
    cc.auto_clear = true;                           // silence si rien a emettre
    if (i2s_new_channel(&cc, &s_tx, &s_rx) != ESP_OK) return false;
    i2s_std_config_t sc = {};
    sc.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(dsp::FS);   // MCLK = 256 x 48 kHz
    sc.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    sc.gpio_cfg.mclk = (gpio_num_t)I2S_MCLK;
    sc.gpio_cfg.bclk = (gpio_num_t)I2S_BCLK;
    sc.gpio_cfg.ws = (gpio_num_t)I2S_WS;
    sc.gpio_cfg.dout = (gpio_num_t)I2S_DOUT;
    sc.gpio_cfg.din = (gpio_num_t)I2S_DIN;
    if (i2s_channel_init_std_mode(s_tx, &sc) != ESP_OK) return false;
    if (i2s_channel_init_std_mode(s_rx, &sc) != ESP_OK) return false;
    i2s_channel_enable(s_tx);
    i2s_channel_enable(s_rx);
    return true;
}

static void tache_audio(void *arg)
{
    static int16_t sortie[2 * CH], entree[2 * CH];
    // Son pur par rotation d'un vecteur (x, y) : 4 multiplications en virgule
    // flottante simple par echantillon. (Un sin() en double precision, 48 000
    // fois par seconde, saturait le coeur 0 : l'ESP32-S3 n'a pas d'unite de
    // calcul double, et le chien de garde finissait par redemarrer la carte.)
    float x = 1, y = 0, cr = 1, ci = 0, f_rot = -1;
    float niveau = 0;
    for (;;) {
        bool ping = p_actif;
        int pos = p_pos;
        float f0 = a_f0, cible = f0 > 0 ? a_amp : 0;
        if (f0 > 0 && f0 != f_rot) {
            f_rot = f0;
            cr = cosf(2 * PI * f0 / dsp::FS);
            ci = sinf(2 * PI * f0 / dsp::FS);
        }
        for (int i = 0; i < CH; i++) {
            int16_t s = 0;
            if (ping) {
                if (pos + i < p_n) s = p_tx[pos + i];
            } else {
                niveau += (cible - niveau) * 0.0015f;   // montee/descente douce (~15 ms) : pas de clic
                if (niveau > 1e-4f) {
                    s = (int16_t)(niveau * 32767 * y);
                    float nx = x * cr - y * ci;
                    y = x * ci + y * cr;
                    x = nx;
                }
            }
            sortie[2 * i] = sortie[2 * i + 1] = s;
        }
        float r = x * x + y * y;                    // garde le vecteur sur le cercle unite
        if (r > 0) { r = 1.5f - 0.5f * r; x *= r; y *= r; }
        size_t n = 0;
        i2s_channel_write(s_tx, sortie, sizeof sortie, &n, portMAX_DELAY);
        i2s_channel_read(s_rx, entree, sizeof entree, &n, portMAX_DELAY);
        if (ping) {
            for (int i = 0; i < CH && pos + i < p_n; i++) p_rx[pos + i] = entree[2 * i];
            p_pos = pos + CH;
            if (p_pos >= p_n) {
                p_actif = false;
                xSemaphoreGive(p_fin);
            }
        } else if (f0 > 0) {
            uint32_t e = r_ecrit;
            for (int i = 0; i < CH; i++) s_anneau[(e + i) % ANNEAU_N] = entree[2 * i];
            r_ecrit = e + CH;
        }
    }
}

bool plat_audio_ping(const int16_t *tx, int16_t *rx, int n)
{
    if (!s_tx) return false;
    xSemaphoreTake(p_fin, 0);
    p_tx = tx;
    p_rx = rx;
    p_n = n;
    p_pos = 0;
    p_actif = true;
    if (xSemaphoreTake(p_fin, pdMS_TO_TICKS(1000)) != pdTRUE) {
        p_actif = false;
        return false;
    }
    return true;
}

// Le son continu fait consommer l'ampli en permanence : on le limite selon
// le volume, et davantage si la carte a deja redemarre par baisse de tension.
static float s_limite_son = 1.0f;

void plat_audio_tonalite(float f0, float amp)
{
    static const float MAXI[3] = { 0.18f, 0.28f, 0.40f };     // Doux / Moyen / Fort
    float m = MAXI[s_volume] * s_limite_son;
    a_amp = amp < m ? amp : m;
    a_f0 = f0;
    r_lu = r_ecrit;                                 // on repart sur des echantillons frais
}

bool plat_audio_lire(int16_t *rx, int n)
{
    uint32_t t0 = millis();
    while (r_ecrit - r_lu < (uint32_t)n) {
        if (millis() - t0 > 500) return false;
        vTaskDelay(pdMS_TO_TICKS(3));
    }
    uint32_t e = r_ecrit;
    if (e - r_lu > ANNEAU_N - CH) r_lu = e - n;     // trop de retard : on saute aux plus recents
    for (int i = 0; i < n; i++) rx[i] = s_anneau[(r_lu + i) % ANNEAU_N];
    r_lu += n;
    return true;
}

// Test au demarrage : un tic, le micro doit entendre le son direct.
// Essaie les deux niveaux de commande de l'ampli (les documents divergent).
static void test_audio()
{
    static int16_t rx[dsp::N_ECHO];
    dsp::Echo e;
    for (int essai = 0; essai < 2; essai++) {
        digitalWrite(PA_PIN, essai == 0 ? HIGH : LOW);
        delay(60);
        float meilleur = -99;
        for (int k = 0; k < 3; k++) {
            if (!plat_audio_ping(dsp::chirp(), rx, dsp::N_ECHO)) break;
            dsp::analyser_echo(rx, 1.0f, 343.0f, e);
            if (e.snr_direct_db > meilleur) meilleur = e.snr_direct_db;
            delay(120);
        }
        Serial.printf("[SON] Test ampli niveau %s : son direct %.1f dB, satures %d\n",
                      essai == 0 ? "HAUT" : "BAS", meilleur, e.satur);
        if (meilleur > 25) {
            s_audio_ok = true;
            if (e.satur > 40) snprintf(s_audio_etat, sizeof s_audio_etat, "OK · micro saturé");
            else snprintf(s_audio_etat, sizeof s_audio_etat, "OK · direct %d dB", (int)meilleur);
            return;
        }
    }
    snprintf(s_audio_etat, sizeof s_audio_etat, "micro ou HP muet");
}

static void init_audio()
{
    s_anneau = (int16_t *)heap_caps_malloc(ANNEAU_N * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    p_fin = xSemaphoreCreateBinary();
    pinMode(PA_PIN, OUTPUT);
    digitalWrite(PA_PIN, LOW);
    // l'horloge MCLK doit tourner avant de configurer le codec
    if (!s_anneau || !init_i2s()) { snprintf(s_audio_etat, sizeof s_audio_etat, "I2S en échec"); return; }
    xTaskCreatePinnedToCore(tache_audio, "audio", 4096, NULL, 6, NULL, 0);
    delay(30);
    if (!s_bus || !init_codec()) { snprintf(s_audio_etat, sizeof s_audio_etat, "codec ES8311 absent"); return; }
    delay(100);
    test_audio();
}

// =========================================================================
// Tache moteur (coeur 0) : pings d'echo, analyse Doppler continue
// =========================================================================
static void tache_moteur(void *arg)
{
    for (;;) {
        uint32_t attente = mot::etape(millis());
        if (attente > 50) attente = 50;
        vTaskDelay(pdMS_TO_TICKS(attente ? attente : 1));
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
        vTaskDelay(1);
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
    } else {
        Serial.println("[SON] PSRAM absente : un seul tampon, animations ralenties");
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
// Tactile (meme etalonnage que la console NES et lumiere_console)
// =========================================================================
static bool s_tactile_ok = false;

static bool init_bus()
{
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = (gpio_num_t)I2C_SDA;
    cfg.scl_io_num = (gpio_num_t)I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&cfg, &s_bus) == ESP_OK;
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
    if (!SD_MMC.begin("/sdcard", true)) return;
    s_sd_ok = true;
    if (!SD_MMC.exists(FICHIER_CSV)) {
        fs::File f = SD_MMC.open(FICHIER_CSV, FILE_WRITE);
        if (f) {
            f.println("# echo,distance_m,dt_us,snr_db,source");
            f.println("# passage,f0_Hz,vmax_m_s,vmoy_m_s,duree_s,sens(+1 approche),c_m_s,source");
            f.close();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[SON] Console vitesse du son %s\n", SON_VERSION);
    // cause du dernier redemarrage (utile si la carte redemarre en Doppler)
    esp_reset_reason_t rr = esp_reset_reason();
    const char *cause = nullptr;
    switch (rr) {
    case ESP_RST_BROWNOUT: cause = "baisse de tension (alimentation)"; s_limite_son = 0.5f; break;
    case ESP_RST_TASK_WDT: case ESP_RST_INT_WDT: case ESP_RST_WDT: cause = "chien de garde (processeur bloqué)"; break;
    case ESP_RST_PANIC: cause = "plantage (panic)"; break;
    default: break;
    }
    if (cause) Serial.printf("[SON] ATTENTION : dernier redémarrage dû à : %s\n", cause);

    ledcAttachChannel(LCD_BL_PIN, 2000, 8, 0);
    ledcWrite(LCD_BL_PIN, 204);
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);

    s_prefs.begin("son", false);
    if (!init_tampons()) {
        tft.drawString("Memoire insuffisante", 10, 10, 2);
        for (;;) delay(1000);
    }
    gfx::init(s_tampon[0]);

    if (init_bus()) {
        s_tactile_ok = ft6336_touch_init(s_bus, (gpio_num_t)TOUCH_RST, 320, 240);
        if (!s_tactile_ok) Serial.println("[SON] Tactile introuvable");
        else if (!ft6336_touch_load_calibration()) etalonner();
    }

    init_sd();
    mot::init();                                    // FFT, chirp (PSRAM)
    init_audio();
    if (rr == ESP_RST_BROWNOUT) snprintf(s_audio_etat, sizeof s_audio_etat, "OK · alim. faible : son réduit");
    else if (cause && s_audio_ok) snprintf(s_audio_etat, sizeof s_audio_etat, "OK · redémarré (%s)", rr == ESP_RST_PANIC ? "panic" : "watchdog");
    Serial.printf("[SON] Audio : %s\n", s_audio_etat);

    ui::init(s_fond);                               // lit les reglages et applique volume / gain

    xTaskCreatePinnedToCore(tache_moteur, "moteur", 8192, NULL, 4, NULL, 0);
    if (s_double) {
        s_sem_pret = xSemaphoreCreateBinary();
        s_sem_libre = xSemaphoreCreateBinary();
        xSemaphoreGive(s_sem_libre);
        xTaskCreatePinnedToCore(tache_ecran, "ecran", 4096, NULL, 2, NULL, 0);
    }
    Serial.printf("[SON] Pret - PSRAM libre %u Ko, interne %u Ko\n",
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
        xSemaphoreTake(s_sem_libre, portMAX_DELAY);
        s_a_envoyer = s_courant;
        xSemaphoreGive(s_sem_pret);
        s_courant ^= 1;
    } else {
        envoyer(s_tampon[0]);
    }
}
