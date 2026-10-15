// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "wifi_access.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <Update.h>
#include <cstring>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_wifi.h>

#include "hw/audio.h"
#include "nexo_codec.h"

namespace nexo {

// Lab-only: the SSIDs the beacon spoofer cycles through ("the rickroll one").
static const char* const kSpamSsids[] = {
    "Never Gonna Give You Up", "Never Gonna Let You Down", "Never Gonna Run Around",
    "And Desert You",          "free wifi",                "get rickrolled",
    "Loading...",              "sus",                      "Rickroll_5G",
};
static const int kSpamSsidCount = (int)(sizeof(kSpamSsids) / sizeof(kSpamSsids[0]));

// Keep uploaded filenames safe: base name only, no separators, ASCII-ish.
static String sanitizeMusicName(const String& raw)
{
    String name = raw;
    int slash = name.lastIndexOf('/');
    int bslash = name.lastIndexOf('\\');
    int cut = slash > bslash ? slash : bslash;
    if (cut >= 0) name = name.substring(cut + 1);
    String out;
    for (unsigned int i = 0; i < name.length(); i++) {
        char c = name[i];
        if (c == '\\' || c == '/' || c == ':' || (unsigned char)c < 0x20) continue;
        out += c;
    }
    out.trim();
    return out;
}

// Minimal, dependency-free browser UI (dark, Material-inspired). No CDNs.
static const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nexo</title><style>
:root{--bg:#121316;--surf:#1e2025;--surf2:#2a2d33;--pri:#a8c7fa;--on:#e3e3e6;--mut:#9aa0a6;--err:#f2b8b5}
*{box-sizing:border-box}body{margin:0;font:15px/1.5 -apple-system,system-ui,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--on)}
header{background:var(--surf);padding:14px 16px;display:flex;align-items:center;gap:10px;position:sticky;top:0}
header b{font-size:18px}h1{font-size:16px;margin:0}
main{padding:16px;max-width:760px;margin:0 auto}
.tabs{display:flex;gap:8px;margin-bottom:12px}
.tab{padding:8px 14px;border-radius:20px;background:var(--surf);cursor:pointer;border:1px solid transparent}
.tab.on{background:var(--pri);color:#062020;font-weight:600}
.card{background:var(--surf);border-radius:16px;padding:14px;margin-bottom:10px}
.card h3{margin:0 0 4px;font-size:15px}.mut{color:var(--mut);font-size:13px}
button{font:inherit;border:0;border-radius:20px;padding:8px 14px;background:var(--surf2);color:var(--on);cursor:pointer}
button.pri{background:var(--pri);color:#062020;font-weight:600}
button.danger{background:var(--err);color:#2a0b0b}
input,textarea,select{width:100%;background:var(--surf2);border:1px solid #3a3d44;border-radius:12px;color:var(--on);padding:10px;font:inherit;margin:4px 0}
.row{display:flex;gap:8px;flex-wrap:wrap}.row>*{flex:1}
.err{color:var(--err)}.badge{display:inline-block;font-size:12px;padding:2px 8px;border-radius:10px;background:var(--surf2);color:var(--mut)}
.pill{font-size:12px;padding:2px 8px;border-radius:10px}
.p0{background:#3a2f2a}.p1{background:#2a3340}.p2{background:#243a2a}
.login{max-width:320px;margin:80px auto;text-align:center}
</style></head><body>
<header><b>Nexo</b><span id="conn" class="badge">local Wi-Fi</span></header>
<main id="app"></main>
<script>
const api=(p,o)=>fetch(p,Object.assign({headers:{'Content-Type':'application/json'}},o)).then(r=>r.json());
let tab='notes', authed=false;
function el(h){const d=document.createElement('div');d.innerHTML=h;return d.firstElementChild}
function esc(s){return (s||'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
const STATUS=['Not Started','In Progress','Done'];
async function render(){
 const app=document.getElementById('app');
 if(!authed){
  app.innerHTML=`<div class="login card"><h3>Sign in</h3><p class="mut">Enter the PIN shown in Nexo settings.</p>
   <input id="pin" type="password" placeholder="PIN" inputmode="numeric"><button class="pri" onclick="login()">Unlock</button>
   <p id="err" class="err"></p></div>`;return;
 }
 let items = tab==='notes' ? (await api('/api/notes')).items : (await api('/api/homework')).items;
 app.innerHTML=`<div class="tabs">
  <div class="tab ${tab==='notes'?'on':''}" onclick="setTab('notes')">Notes</div>
  <div class="tab ${tab==='homework'?'on':''}" onclick="setTab('homework')">Homework</div></div>
  <div class="card"><h3>New ${tab==='notes'?'note':'homework'}</h3>
  <div id="form"></div><button class="pri" onclick="save('')">Add</button></div>
  <div id="list"></div>`;
 const list=app.querySelector('#list');
 for(const it of items){ list.appendChild(card(it)); }
 const f=app.querySelector('#form');
 f.innerHTML = tab==='notes'
  ? `<input id="f_title" placeholder="Title"><textarea id="f_body" rows="2" placeholder="Body"></textarea>`
  : `<input id="f_subject" placeholder="Subject"><textarea id="f_desc" rows="2" placeholder="Description"></textarea>
     <div class="row"><input id="f_due" type="date"><select id="f_status"><option value="0">Not Started</option><option value="1">In Progress</option><option value="2">Done</option></select></div>`;
}
function card(it){
 const c=el(`<div class="card"></div>`);
 if(tab==='notes'){
  c.innerHTML=`<h3>${esc(it.title)||'(untitled)'}</h3><p class="mut">${esc(it.body)}</p>
   <div class="row"><button onclick='editNote(${JSON.stringify(it).replace(/'/g,"&#39;")})'>Edit</button>
   <button class="danger" onclick='del("${it.id}",${it.rev})'>Delete</button></div>`;
 } else {
  c.innerHTML=`<h3>${esc(it.subject)} <span class="pill p${it.status}">${STATUS[it.status]}</span></h3>
   <p class="mut">${esc(it.description)}</p><span class="badge">Due ${it.due||'-'}</span>
   <div class="row"><button onclick='editHw(${JSON.stringify(it).replace(/'/g,"&#39;")})'>Edit</button>
   <button class="danger" onclick='delHw("${it.id}",${it.rev})'>Delete</button></div>`;
 }
 return c;
}
function setTab(t){tab=t;render()}
async function login(){const pin=document.getElementById('pin').value;
 const r=await api('/api/login',{method:'POST',body:JSON.stringify({pin})});
 if(r.ok){authed=true;render()}else{document.getElementById('err').textContent='Wrong PIN';}}
async function save(){
 if(tab==='notes'){await api('/api/notes',{method:'POST',body:JSON.stringify({title:v('f_title'),body:v('f_body')})});}
 else{await api('/api/homework',{method:'POST',body:JSON.stringify({subject:v('f_subject'),description:v('f_desc'),due:v('f_due'),status:+v('f_status')})});}
 render();
}
function editNote(it){document.getElementById('f_title').value=it.title;document.getElementById('f_body').value=it.body;
 document.querySelector('#form').dataset.id=it.id;document.querySelector('#form').dataset.rev=it.rev;}
function editHw(it){document.getElementById('f_subject').value=it.subject;document.getElementById('f_desc').value=it.description;
 document.getElementById('f_due').value=it.due||'';document.getElementById('f_status').value=it.status;
 document.querySelector('#form').dataset.id=it.id;document.querySelector('#form').dataset.rev=it.rev;}
async function del(id,rev){await api('/api/notes/delete',{method:'POST',body:JSON.stringify({id,baseRev:rev})});render()}
async function delHw(id,rev){await api('/api/homework/delete',{method:'POST',body:JSON.stringify({id,baseRev:rev})});render()}
function v(id){return (document.getElementById(id)||{}).value||''}
render();
</script></body></html>)HTML";

String WifiAccess::ipString() const
{
    if (!enabled_) return String("-");
    return WiFi.softAPIP().toString();
}

void WifiAccess::begin(NexoApp* app)
{
    app_ = app;
    // Random session token + AP password derived from the Wi-Fi MAC. Read the MAC
    // from eFuse so this works before the Wi-Fi driver is initialised.
    uint32_t r = esp_random();
    char tok[20];
    snprintf(tok, sizeof(tok), "%08lx%08lx", (unsigned long)r, (unsigned long)esp_random());
    token_ = tok;

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char pw[16];
    snprintf(pw, sizeof(pw), "nexo%02x%02x", mac[4], mac[5]);
    apPassword_ = pw;

    setupRoutes();
    applyEnabled();
}

void WifiAccess::setEnabled(bool on)
{
    enabled_ = on;
    applyEnabled();
}

void WifiAccess::applyEnabled()
{
    if (!app_) return;
    Settings& s = app_->settings();
    if (enabled_) {
        if (started_) return;
        String ssid = s.apSsid.c_str();
        ssid.trim();
        if (ssid.length() == 0) ssid = "Nexo-AP";

        WiFi.mode(WIFI_AP);
        WiFi.setSleep(false);  // keep the AP responsive alongside BLE
        IPAddress ip(192, 168, 4, 1), gw(192, 168, 4, 1), mask(255, 255, 255, 0);
        WiFi.softAPConfig(ip, gw, mask);
        bool ok = WiFi.softAP(ssid.c_str(), apPassword_.c_str());
        if (!ok) {
            Serial.println("[nexo] softAP start failed");
            WiFi.mode(WIFI_OFF);
            return;
        }
        if (server_) server_->begin();
        started_ = true;
        Serial.printf("[nexo] AP '%s' at %s (pass %s)\n", ssid.c_str(),
                      WiFi.softAPIP().toString().c_str(), apPassword_.c_str());
    } else {
        if (started_ && server_) server_->stop();
        if (started_) WiFi.softAPdisconnect(true);
        started_ = false;
        WiFi.mode(WIFI_OFF);
    }
}

// ---- Lab tools -------------------------------------------------------------

void WifiAccess::scanStart()
{
    // The radio can only scan from the STA interface, so tear the AP down and
    // restore it once the async scan reports back (labTick).
    scanRestoreAp_ = enabled_ && started_;
    if (started_) {
        if (server_) server_->stop();
        WiFi.softAPdisconnect(true);
        started_ = false;
    }
    scanResults_.clear();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    WiFi.scanNetworks(true /*async*/, false /*show hidden*/);
    scanning_ = true;
    Serial.println("[lab] wifi scan start");
}

void WifiAccess::labTick()
{
    if (scanning_) {
        int n = WiFi.scanComplete();
        if (n == -2) return;  // still running
        if (n >= 0) {
            for (int i = 0; i < n; ++i) {
                ApInfo a;
                a.ssid = WiFi.SSID(i).c_str();
                if (a.ssid.empty()) a.ssid = "(hidden)";
                a.rssi = (int8_t)WiFi.RSSI(i);
                a.channel = (uint8_t)WiFi.channel(i);
                a.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
                scanResults_.push_back(a);
            }
            WiFi.scanDelete();
        }
        scanning_ = false;
        if (scanRestoreAp_) applyEnabled();  // bring the AP back up
        Serial.printf("[lab] wifi scan done: %u\n", (unsigned)scanResults_.size());
    }

    if (!spamming_) return;
    uint32_t now = millis();
    if ((int32_t)(now - spamNext_) < 0) return;
    spamNext_ = now + 40;
    sendBeacon();
    if (++spamSsidIndex_ >= kSpamSsidCount) {
        spamSsidIndex_ = 0;
        if (++spamChannel_ > 11) spamChannel_ = 1;
        esp_wifi_set_channel(spamChannel_, WIFI_SECOND_CHAN_NONE);
    }
}

void WifiAccess::sendBeacon()
{
    const char* ssid = kSpamSsids[spamSsidIndex_];
    size_t ssidLen = strlen(ssid);
    if (ssidLen > 32) ssidLen = 32;

    uint8_t frame[128];
    size_t i = 0;
    frame[i++] = 0x80;  // frame control: management / beacon
    frame[i++] = 0x00;
    frame[i++] = 0x00;  // duration
    frame[i++] = 0x00;
    for (int k = 0; k < 6; ++k) frame[i++] = 0xFF;           // DA: broadcast
    for (int k = 0; k < 6; ++k) frame[i++] = spamBssid_[k];  // SA
    for (int k = 0; k < 6; ++k) frame[i++] = spamBssid_[k];  // BSSID
    frame[i++] = 0x00;  // sequence control
    frame[i++] = 0x00;
    for (int k = 0; k < 8; ++k) frame[i++] = 0x00;  // timestamp
    frame[i++] = 0x64;  // beacon interval: 100 TU
    frame[i++] = 0x00;
    frame[i++] = 0x01;  // capability: ESS (open)
    frame[i++] = 0x00;
    frame[i++] = 0x00;  // SSID element
    frame[i++] = (uint8_t)ssidLen;
    memcpy(&frame[i], ssid, ssidLen);
    i += ssidLen;
    frame[i++] = 0x01;  // supported rates
    frame[i++] = 0x08;
    frame[i++] = 0x82; frame[i++] = 0x84; frame[i++] = 0x8b; frame[i++] = 0x96;
    frame[i++] = 0x0c; frame[i++] = 0x12; frame[i++] = 0x18; frame[i++] = 0x24;
    frame[i++] = 0x03;  // DS parameter: channel
    frame[i++] = 0x01;
    frame[i++] = spamChannel_;

    esp_wifi_80211_tx(WIFI_IF_AP, frame, (int)i, false);
    ++spamFrames_;
}

void WifiAccess::startBeaconSpam()
{
    if (spamming_) return;
    if (!started_) setEnabled(true);  // raw tx needs a live netif
    if (!started_) WiFi.mode(WIFI_STA);
    for (int k = 0; k < 6; ++k) spamBssid_[k] = (uint8_t)(esp_random() & 0xFF);
    spamBssid_[0] = (uint8_t)((spamBssid_[0] & 0xFE) | 0x02);  // locally administered
    spamChannel_ = 1;
    esp_wifi_set_channel(spamChannel_, WIFI_SECOND_CHAN_NONE);
    spamming_ = true;
    spamFrames_ = 0;
    spamNext_ = millis();
    Serial.println("[lab] beacon spam start");
}

void WifiAccess::stopBeaconSpam()
{
    if (!spamming_) return;
    spamming_ = false;
    Serial.printf("[lab] beacon spam stop (%u frames)\n", (unsigned)spamFrames_);
}

void WifiAccess::setupRoutes()
{
    if (server_) return;
    server_ = new WebServer(80);
    server_->on("/", HTTP_GET, [this]() { handleRoot(); });
    server_->on("/api/login", HTTP_POST, [this]() { handleLogin(); });
    server_->on("/api/info", HTTP_GET, [this]() { handleInfo(); });
    server_->on("/api/notes", HTTP_GET, [this]() { handleNotesList(); });
    server_->on("/api/notes", HTTP_POST, [this]() { handleNoteUpsert(); });
    server_->on("/api/notes/delete", HTTP_POST, [this]() { handleNoteDelete(); });
    server_->on("/api/homework", HTTP_GET, [this]() { handleHomeworkList(); });
    server_->on("/api/homework", HTTP_POST, [this]() { handleHomeworkUpsert(); });
    server_->on("/api/homework/delete", HTTP_POST, [this]() { handleHomeworkDelete(); });
    server_->on("/api/music", HTTP_GET, [this]() { handleMusicList(); });
    server_->on("/api/music", HTTP_POST, [this]() { handleMusicUploadDone(); },
                [this]() { handleMusicUploadStream(); });
    server_->on("/api/music/now", HTTP_GET, [this]() { handleMusicNow(); });
    server_->on("/api/music/control", HTTP_POST, [this]() { handleMusicControl(); });
    server_->on("/api/music/delete", HTTP_POST, [this]() { handleMusicDelete(); });
    server_->on("/api/music/file", HTTP_GET, [this]() { handleMusicDownload(); });
    server_->on("/api/ota", HTTP_POST, [this]() { handleOtaDone(); },
                [this]() { handleOtaStream(); });
    server_->onNotFound([this]() {
        server_->send(404, "application/json", "{\"error\":\"not found\"}");
    });
}

void WifiAccess::loop()
{
    if (enabled_ && started_ && server_) server_->handleClient();
    labTick();
}

bool WifiAccess::tokenMatches()
{
    String cookie = server_->header("Cookie");
    return cookie.indexOf(token_.c_str()) >= 0 ||
           server_->header("X-Nexo-Token") == token_.c_str();
}

bool WifiAccess::requireAuth()
{
    // Token via cookie or X-Nexo-Token header.
    if (!tokenMatches()) {
        server_->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return false;
    }
    return true;
}

void WifiAccess::sendJson(int code, const std::string& body)
{
    server_->send(code, "application/json", body.c_str());
}

void WifiAccess::handleRoot()
{
    server_->send_P(200, "text/html; charset=utf-8", kIndexHtml);
}

void WifiAccess::handleLogin()
{
    if (server_->arg("plain").length() > 512) {
        sendJson(413, "{\"error\":\"too large\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_->arg("plain"))) {
        sendJson(400, "{\"error\":\"bad json\"}");
        return;
    }
    String pin = doc["pin"] | "";
    if (pin == app_->settings().pin.c_str()) {
        server_->sendHeader("Set-Cookie", ("nexo_token=" + token_ + "; Path=/; HttpOnly").c_str());
        sendJson(200, "{\"ok\":true}");
    } else {
        sendJson(401, "{\"error\":\"bad pin\"}");
    }
}

void WifiAccess::handleInfo()
{
    if (!requireAuth()) return;
    JsonDocument doc;
    doc["deviceName"] = app_->settings().deviceName;
    doc["protocol"] = kProtocolVersion;
    doc["notes"] = app_->repo().noteCount();
    doc["homework"] = app_->repo().homeworkCount();
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WifiAccess::handleNotesList()
{
    if (!requireAuth()) return;
    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (const auto& n : app_->repo().notes()) {
        if (n.deleted) continue;
        JsonObject o = arr.add<JsonObject>();
        o["id"] = n.id;
        o["title"] = n.title;
        o["body"] = n.body;
        o["rev"] = n.rev;
        o["updated"] = n.updated;
    }
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WifiAccess::handleNoteUpsert()
{
    if (!requireAuth()) return;
    if (server_->arg("plain").length() > 8192) {
        sendJson(413, "{\"error\":\"too large\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_->arg("plain"))) {
        sendJson(400, "{\"error\":\"bad json\"}");
        return;
    }
    Note n;
    n.id = doc["id"] | "";
    n.title = doc["title"] | "";
    n.body = doc["body"] | "";
    uint32_t base = doc["baseRev"] | 0u;
    // Look up current rev if updating.
    Note cur;
    if (!n.id.empty() && app_->repo().getNote(n.id, cur)) base = cur.rev;
    Note stored;
    RepoStatus st = app_->repo().putNote(n, base, stored);
    if (st != RepoStatus::Ok && st != RepoStatus::Conflict) {
        sendJson(400, "{\"error\":\"bad request\"}");
        return;
    }
    JsonDocument out;
    out["id"] = stored.id;
    out["rev"] = stored.rev;
    out["conflict"] = (st == RepoStatus::Conflict);
    std::string s;
    serializeJson(out, s);
    sendJson(200, s);
}

void WifiAccess::handleNoteDelete()
{
    if (!requireAuth()) return;
    if (server_->arg("plain").length() > 1024) {
        sendJson(413, "{\"error\":\"too large\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_->arg("plain"))) {
        sendJson(400, "{\"error\":\"bad json\"}");
        return;
    }
    std::string id = doc["id"] | "";
    uint32_t base = doc["baseRev"] | 0u;
    Note cur;
    if (app_->repo().getNote(id, cur)) base = cur.rev;
    Note stored;
    RepoStatus st = app_->repo().deleteNote(id, base, stored);
    sendJson(st == RepoStatus::NotFound ? 404 : 200,
             st == RepoStatus::NotFound ? "{\"error\":\"not found\"}" : "{\"ok\":true}");
}

void WifiAccess::handleHomeworkList()
{
    if (!requireAuth()) return;
    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (const auto& h : app_->repo().homework()) {
        if (h.deleted) continue;
        JsonObject o = arr.add<JsonObject>();
        o["id"] = h.id;
        o["subject"] = h.subject;
        o["description"] = h.description;
        o["status"] = static_cast<uint8_t>(h.status);
        o["due"] = h.dueEpochDay ? dueDayToIso(h.dueEpochDay).c_str() : "";
        o["dueEpochDay"] = h.dueEpochDay;
        o["rev"] = h.rev;
        o["updated"] = h.updated;
    }
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WifiAccess::handleHomeworkUpsert()
{
    if (!requireAuth()) return;
    if (server_->arg("plain").length() > 8192) {
        sendJson(413, "{\"error\":\"too large\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_->arg("plain"))) {
        sendJson(400, "{\"error\":\"bad json\"}");
        return;
    }
    Homework h;
    h.id = doc["id"] | "";
    h.subject = doc["subject"] | "";
    h.description = doc["description"] | "";
    String due = doc["due"] | "";
    h.dueEpochDay = due.length() ? isoToDueDay(due.c_str()) : 0;
    h.status = static_cast<HomeworkStatus>((int)(doc["status"] | 0));
    uint32_t base = 0;
    Homework cur;
    if (!h.id.empty() && app_->repo().getHomework(h.id, cur)) base = cur.rev;
    Homework stored;
    RepoStatus st = app_->repo().putHomework(h, base, stored);
    if (st != RepoStatus::Ok && st != RepoStatus::Conflict) {
        sendJson(400, "{\"error\":\"bad request\"}");
        return;
    }
    JsonDocument out;
    out["id"] = stored.id;
    out["rev"] = stored.rev;
    out["conflict"] = (st == RepoStatus::Conflict);
    std::string s;
    serializeJson(out, s);
    sendJson(200, s);
}

void WifiAccess::handleHomeworkDelete()
{
    if (!requireAuth()) return;
    if (server_->arg("plain").length() > 1024) {
        sendJson(413, "{\"error\":\"too large\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_->arg("plain"))) {
        sendJson(400, "{\"error\":\"bad json\"}");
        return;
    }
    std::string id = doc["id"] | "";
    uint32_t base = 0;
    Homework cur;
    if (app_->repo().getHomework(id, cur)) base = cur.rev;
    Homework stored;
    RepoStatus st = app_->repo().deleteHomework(id, base, stored);
    sendJson(st == RepoStatus::NotFound ? 404 : 200,
             st == RepoStatus::NotFound ? "{\"error\":\"not found\"}" : "{\"ok\":true}");
}

void WifiAccess::handleMusicList()
{
    if (!requireAuth()) return;
    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (int i = 0; i < musicCount(); i++) {
        JsonObject o = arr.add<JsonObject>();
        o["name"] = musicTitle(i);
        o["size"] = musicTrackSize(i);
    }
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WifiAccess::handleMusicNow()
{
    if (!requireAuth()) return;
    JsonDocument doc;
    int idx = musicIndex();
    doc["index"] = idx;
    doc["name"] = idx >= 0 ? musicTitle(idx) : "";
    doc["playing"] = musicPlaying();
    doc["position"] = musicPositionSec();
    doc["duration"] = musicDurationSec();
    doc["volume"] = audioVolume();
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WifiAccess::handleMusicControl()
{
    if (!requireAuth()) return;
    if (server_->arg("plain").length() > 512) {
        sendJson(413, "{\"error\":\"too large\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_->arg("plain"))) {
        sendJson(400, "{\"error\":\"bad json\"}");
        return;
    }
    String action = doc["action"] | "";
    int index = doc["index"] | -1;
    int arg = doc["arg"] | 0;
    if (action == "toggle") {
        musicTogglePlay();
    } else if (action == "play") {
        musicPlay(index);
    } else if (action == "next") {
        musicNext();
    } else if (action == "prev") {
        musicPrev();
    } else if (action == "stop") {
        musicStop();
    } else if (action == "seek") {
        musicSeek((uint16_t)arg);
    } else if (action == "volume") {
        audioSetVolume((uint8_t)(arg < 0 ? 0 : (arg > 100 ? 100 : arg)));
    } else {
        sendJson(400, "{\"error\":\"bad action\"}");
        return;
    }
    JsonDocument out;
    int idx = musicIndex();
    out["index"] = idx;
    out["playing"] = musicPlaying();
    out["position"] = musicPositionSec();
    out["duration"] = musicDurationSec();
    out["volume"] = audioVolume();
    std::string s;
    serializeJson(out, s);
    sendJson(200, s);
}

void WifiAccess::handleMusicDelete()
{
    if (!requireAuth()) return;
    if (server_->arg("plain").length() > 512) {
        sendJson(413, "{\"error\":\"too large\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server_->arg("plain"))) {
        sendJson(400, "{\"error\":\"bad json\"}");
        return;
    }
    String name = sanitizeMusicName(String((const char*)(doc["name"] | "")));
    if (name.length() == 0) {
        sendJson(400, "{\"error\":\"bad name\"}");
        return;
    }
    bool ok = musicDelete(name.c_str());
    sendJson(ok ? 200 : 404, ok ? "{\"ok\":true}" : "{\"error\":\"not found\"}");
}

void WifiAccess::handleMusicDownload()
{
    if (!requireAuth()) return;
    String name = sanitizeMusicName(server_->arg("name"));
    if (name.length() == 0) {
        sendJson(400, "{\"error\":\"bad name\"}");
        return;
    }
    String path = String("/music/") + name;
    File f = SD.open(path, FILE_READ);
    if (!f) {
        sendJson(404, "{\"error\":\"not found\"}");
        return;
    }
    String lower = name;
    lower.toLowerCase();
    String type = lower.endsWith(".wav") ? "audio/wav" : "audio/mpeg";
    server_->streamFile(f, type, 200);
    f.close();
}

void WifiAccess::handleMusicUploadStream()
{
    HTTPUpload& up = server_->upload();
    if (up.status == UPLOAD_FILE_START) {
        uploadAuth_ = tokenMatches();
        uploadOk_ = false;
        uploadName_ = "";
        if (!uploadAuth_) return;
        String name = sanitizeMusicName(up.filename);
        String lower = name;
        lower.toLowerCase();
        if (name.length() == 0 ||
            !(lower.endsWith(".mp3") || lower.endsWith(".wav"))) {
            return;
        }
        if (!SD.exists("/music")) SD.mkdir("/music");
        String path = String("/music/") + name;
        if (SD.exists(path)) SD.remove(path);
        uploadFile_ = SD.open(path, FILE_WRITE);
        if (uploadFile_) {
            uploadOk_ = true;
            uploadName_ = name;
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (uploadOk_ && uploadFile_) uploadFile_.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        if (uploadFile_) uploadFile_.close();
        if (uploadOk_) musicRescan();
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile_) uploadFile_.close();
        uploadOk_ = false;
    }
}

void WifiAccess::handleMusicUploadDone()
{
    if (!uploadAuth_) {
        sendJson(401, "{\"error\":\"unauthorized\"}");
        return;
    }
    if (!uploadOk_ || uploadName_.length() == 0) {
        sendJson(400, "{\"error\":\"bad file\"}");
        return;
    }
    JsonDocument out;
    out["ok"] = true;
    out["name"] = uploadName_;
    std::string s;
    serializeJson(out, s);
    uploadOk_ = false;
    sendJson(200, s);
}

void WifiAccess::handleOtaStream()
{
    HTTPUpload& up = server_->upload();
    if (up.status == UPLOAD_FILE_START) {
        otaAuth_ = tokenMatches();
        otaStarted_ = false;
        otaOk_ = false;
        otaWritten_ = 0;
        if (!otaAuth_) return;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.printf("[ota] begin failed: %s\n", Update.errorString());
            return;
        }
        otaStarted_ = true;
        Serial.println("[ota] update started");
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!otaStarted_) return;
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            Serial.printf("[ota] write failed: %s\n", Update.errorString());
            otaStarted_ = false;
            return;
        }
        otaWritten_ += up.currentSize;
    } else if (up.status == UPLOAD_FILE_END) {
        if (!otaStarted_) return;
        otaOk_ = Update.end(true);
        Serial.printf("[ota] %s wrote %u bytes\n", otaOk_ ? "ok" : "FAILED",
                      (unsigned)otaWritten_);
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        otaStarted_ = false;
        otaOk_ = false;
    }
}

void WifiAccess::handleOtaDone()
{
    if (!otaAuth_) {
        sendJson(401, "{\"error\":\"unauthorized\"}");
        return;
    }
    if (!otaOk_) {
        sendJson(400, "{\"error\":\"update failed\"}");
        return;
    }
    JsonDocument out;
    out["ok"] = true;
    out["bytes"] = (uint32_t)otaWritten_;
    std::string s;
    serializeJson(out, s);
    sendJson(200, s);
    Serial.println("[ota] success, rebooting in 500ms");
    // Give the response time to flush before restarting.
    delay(500);
    ESP.restart();
}

}  // namespace nexo
